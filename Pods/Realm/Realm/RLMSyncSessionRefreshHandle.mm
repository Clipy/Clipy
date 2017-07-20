////////////////////////////////////////////////////////////////////////////
//
// Copyright 2016 Realm Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
////////////////////////////////////////////////////////////////////////////

#import "RLMSyncSessionRefreshHandle.hpp"

#import "RLMAuthResponseModel.h"
#import "RLMNetworkClient.h"
#import "RLMSyncManager_Private.h"
#import "RLMSyncUser_Private.hpp"
#import "RLMSyncUtil_Private.hpp"
#import "RLMTokenModels.h"
#import "RLMUtil.hpp"

#import "sync/sync_session.hpp"

using namespace realm;

namespace {

void unregisterRefreshHandle(const std::weak_ptr<SyncUser>& user, const std::string& path) {
    if (auto strong_user = user.lock()) {
        std::static_pointer_cast<CocoaSyncUserContext>(strong_user->binding_context())->unregister_refresh_handle(path);
    }
}

}

@interface RLMSyncSessionRefreshHandle () {
    std::weak_ptr<SyncUser> _user;
    std::string _path;
    std::weak_ptr<SyncSession> _session;
    std::shared_ptr<SyncSession> _strongSession;
}

@property (nonatomic) NSTimer *timer;
@property (nonatomic) NSString *identity;

@property (nonatomic) NSURL *realmURL;
@property (nonatomic) NSURL *authServerURL;
@property (nonatomic, copy) RLMSyncBasicErrorReportingBlock completionBlock;

@end

@implementation RLMSyncSessionRefreshHandle

- (instancetype)initWithRealmURL:(NSURL *)realmURL
                            user:(RLMSyncUser *)user
                         session:(std::shared_ptr<realm::SyncSession>)session
                 completionBlock:(RLMSyncBasicErrorReportingBlock)completionBlock {
    if (self = [super init]) {
        NSString *path = [realmURL path];
        _path = [path UTF8String];
        self.identity = user.identity;
        if (!self.identity) {
            @throw RLMException(@"Refresh handles cannot be created for users without a valid identity.");
        }
        self.authServerURL = user.authenticationServer;
        self.completionBlock = completionBlock;
        self.realmURL = realmURL;
        // For the initial bind, we want to prolong the session's lifetime.
        _strongSession = std::move(session);
        _session = _strongSession;
        _user = [user _syncUser];
        // Immediately fire off the network request.
        [self _timerFired:nil];
        return self;
    }
    return nil;
}

- (void)dealloc {
    [self.timer invalidate];
}

- (void)invalidate {
    _strongSession = nullptr;
    [self.timer invalidate];
}

+ (NSDate *)fireDateForTokenExpirationDate:(NSDate *)date nowDate:(NSDate *)nowDate {
    static const NSTimeInterval refreshBuffer = 10;
    NSDate *fireDate = [date dateByAddingTimeInterval:-refreshBuffer];
    // Only fire times in the future are valid.
    return ([fireDate compare:nowDate] == NSOrderedDescending ? fireDate : nil);
}

- (void)scheduleRefreshTimer:(NSDate *)dateWhenTokenExpires {
    // Schedule the timer on the main queue.
    // It's very likely that this method will be run on a side thread, for example
    // on the thread that runs `NSURLSession`'s completion blocks. We can't be
    // guaranteed that there's an existing runloop on those threads, and we don't want
    // to create and start a new one if one doesn't already exist.
    dispatch_async(dispatch_get_main_queue(), ^{
        [self.timer invalidate];
        NSDate *fireDate = [RLMSyncSessionRefreshHandle fireDateForTokenExpirationDate:dateWhenTokenExpires
                                                                               nowDate:[NSDate date]];
        if (!fireDate) {
            unregisterRefreshHandle(_user, _path);
            return;
        }
        self.timer = [[NSTimer alloc] initWithFireDate:fireDate
                                              interval:0
                                                target:self
                                              selector:@selector(_timerFired:)
                                              userInfo:nil
                                               repeats:NO];
        [[NSRunLoop currentRunLoop] addTimer:self.timer forMode:NSDefaultRunLoopMode];
    });
}

/// Handler for network requests whose responses successfully parse into an auth response model.
- (BOOL)_handleSuccessfulRequest:(RLMAuthResponseModel *)model {
    // Success
    std::shared_ptr<SyncSession> session = _session.lock();
    if (!session) {
        // The session is dead or in a fatal error state.
        unregisterRefreshHandle(_user, _path);
        [self invalidate];
        return NO;
    }
    bool success = session->state() != SyncSession::PublicState::Error;
    if (success) {
        // Calculate the resolved path.
        NSString *resolvedURLString = nil;
        RLMServerPath resolvedPath = model.accessToken.tokenData.path;
        // Munge the path back onto the original URL, because the `sync` API expects an entire URL.
        NSURLComponents *urlBuffer = [NSURLComponents componentsWithURL:self.realmURL
                                                resolvingAgainstBaseURL:YES];
        urlBuffer.path = resolvedPath;
        resolvedURLString = [[urlBuffer URL] absoluteString];
        if (!resolvedURLString) {
            @throw RLMException(@"Resolved path returned from the server was invalid (%@).", resolvedPath);
        }
        // Pass the token and resolved path to the underlying sync subsystem.
        session->refresh_access_token([model.accessToken.token UTF8String], {resolvedURLString.UTF8String});
        success = session->state() != SyncSession::PublicState::Error;
        if (success) {
            // Schedule a refresh. If we're successful we must already have `bind()`ed the session
            // initially, so we can null out the strong pointer.
            _strongSession = nullptr;
            NSDate *expires = [NSDate dateWithTimeIntervalSince1970:model.accessToken.tokenData.expires];
            [self scheduleRefreshTimer:expires];
        } else {
            // The session is dead or in a fatal error state.
            unregisterRefreshHandle(_user, _path);
            [self invalidate];
        }
    }
    if (self.completionBlock) {
        self.completionBlock(success ? nil : make_auth_error_client_issue());
    }
    return success;
}

/// Handler for network requests that failed before the JSON parsing stage.
- (BOOL)_handleFailedRequest:(NSError *)error {
    NSError *authError;
    if ([error.domain isEqualToString:RLMSyncAuthErrorDomain]) {
        // Network client may return sync related error
        authError = error;
    } else {
        // Something else went wrong
        authError = make_auth_error_bad_response();
    }
    if (self.completionBlock) {
        self.completionBlock(authError);
    }
    [[RLMSyncManager sharedManager] _fireError:make_sync_error(authError)];
    // Certain errors related to network connectivity should trigger a retry.
    NSDate *nextTryDate = nil;
    if ([error.domain isEqualToString:NSURLErrorDomain]) {
        switch (error.code) {
            case NSURLErrorCannotConnectToHost:
            case NSURLErrorNotConnectedToInternet:
            case NSURLErrorNetworkConnectionLost:
            case NSURLErrorTimedOut:
            case NSURLErrorDNSLookupFailed:
            case NSURLErrorCannotFindHost:
                // FIXME: 10 seconds is an arbitrarily chosen value, consider rationalizing it.
                nextTryDate = [NSDate dateWithTimeIntervalSinceNow:10];
                break;
            default:
                break;
        }
    }
    if (!nextTryDate) {
        // This error isn't a network failure error. Just invalidate the refresh handle and stop.
        unregisterRefreshHandle(_user, _path);
        [self invalidate];
        return NO;
    }
    // If we tried to initially bind the session and failed, we'll try again. However, each
    // subsequent attempt will use a weak pointer to avoid prolonging the session's lifetime
    // unnecessarily.
    _strongSession = nullptr;
    [self scheduleRefreshTimer:nextTryDate];
    return NO;
}

/// Callback handler for network requests.
- (BOOL)_onRefreshCompletionWithError:(NSError *)error json:(NSDictionary *)json {
    if (json && !error) {
        RLMAuthResponseModel *model = [[RLMAuthResponseModel alloc] initWithDictionary:json
                                                                    requireAccessToken:YES
                                                                   requireRefreshToken:NO];
        if (model) {
            return [self _handleSuccessfulRequest:model];
        }
        // Otherwise, malformed JSON
        unregisterRefreshHandle(_user, _path);
        [self.timer invalidate];
        if (self.completionBlock) {
            self.completionBlock(error);
        }
        [[RLMSyncManager sharedManager] _fireError:make_sync_error(make_auth_error_bad_response(json))];
        return NO;
    } else {
        REALM_ASSERT(error);
        return [self _handleFailedRequest:error];
    }
}

- (void)_timerFired:(__unused NSTimer *)timer {
    RLMServerToken refreshToken = nil;
    if (auto user = _user.lock()) {
        refreshToken = @(user->refresh_token().c_str());
    }
    if (!refreshToken) {
        unregisterRefreshHandle(_user, _path);
        [self.timer invalidate];
        return;
    }

    NSDictionary *json = @{
                           kRLMSyncProviderKey: @"realm",
                           kRLMSyncPathKey: @(_path.c_str()),
                           kRLMSyncDataKey: refreshToken,
                           kRLMSyncAppIDKey: [RLMSyncManager sharedManager].appID,
                           };

    __weak RLMSyncSessionRefreshHandle *weakSelf = self;
    RLMSyncCompletionBlock handler = ^(NSError *error, NSDictionary *json) {
        [weakSelf _onRefreshCompletionWithError:error json:json];
    };
    [RLMNetworkClient postRequestToEndpoint:RLMServerEndpointAuth
                                     server:self.authServerURL
                                       JSON:json
                                 completion:handler];
}

@end
