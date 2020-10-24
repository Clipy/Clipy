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

#import "RLMNetworkClient.h"

#import "RLMRealmConfiguration.h"
#import "RLMJSONModels.h"
#import "RLMSyncUtil_Private.hpp"
#import "RLMSyncManager_Private.h"
#import "RLMUtil.hpp"

#import <realm/util/scope_exit.hpp>

typedef void(^RLMServerURLSessionCompletionBlock)(NSData *, NSURLResponse *, NSError *);

static NSUInteger const kHTTPCodeRange = 100;

typedef enum : NSUInteger {
    Informational       = 1, // 1XX
    Success             = 2, // 2XX
    Redirection         = 3, // 3XX
    ClientError         = 4, // 4XX
    ServerError         = 5, // 5XX
} RLMServerHTTPErrorCodeType;

static NSRange rangeForErrorType(RLMServerHTTPErrorCodeType type) {
    return NSMakeRange(type*100, kHTTPCodeRange);
}

#pragma mark Network client

@interface RLMSessionDelegate <NSURLSessionDelegate> : NSObject
+ (instancetype)delegateWithCertificatePaths:(NSDictionary *)paths
                                  completion:(void (^)(NSError *, NSDictionary *))completion;
@end

@interface RLMSyncServerEndpoint ()
/// The HTTP method the endpoint expects. Defaults to POST.
+ (NSString *)httpMethod;

/// The URL to which the request should be made. Must be implemented.
+ (NSURL *)urlForAuthServer:(NSURL *)authServerURL payload:(NSDictionary *)json;

/// The body for the request, if any.
+ (NSData *)httpBodyForPayload:(NSDictionary *)json error:(NSError **)error;

/// The HTTP headers to be added to the request, if any.
+ (NSDictionary<NSString *, NSString *> *)httpHeadersForPayload:(NSDictionary *)json
                                                        options:(nullable RLMNetworkRequestOptions *)options;
@end

@implementation RLMSyncServerEndpoint

+ (void)sendRequestToServer:(NSURL *)serverURL
                       JSON:(NSDictionary *)jsonDictionary
                 completion:(void (^)(NSError *))completionBlock {
    [self sendRequestToServer:serverURL JSON:jsonDictionary timeout:0
                   completion:^(NSError *error, NSDictionary *) {
        completionBlock(error);
    }];
}
+ (void)sendRequestToServer:(NSURL *)serverURL
                       JSON:(NSDictionary *)jsonDictionary
                    timeout:(NSTimeInterval)timeout
                 completion:(void (^)(NSError *, NSDictionary *))completionBlock {
    // If the timeout isn't set then use the timeout set on the sync manager,
    // or 60 seconds if it isn't set there either.
    RLMSyncManager *syncManager = RLMSyncManager.sharedManager;
    if (timeout < 1)
        timeout = syncManager.timeoutOptions.connectTimeout / 1000.0;
    if (timeout < 1)
        timeout = 60.0;

    // Create the request
    NSError *localError = nil;
    NSURL *requestURL = [self urlForAuthServer:serverURL payload:jsonDictionary];
    NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:requestURL];
    request.HTTPMethod = [self httpMethod];
    if (![request.HTTPMethod isEqualToString:@"GET"]) {
        request.HTTPBody = [self httpBodyForPayload:jsonDictionary error:&localError];
        if (localError) {
            completionBlock(localError, nil);
            return;
        }
    }
    request.timeoutInterval = timeout;
    RLMNetworkRequestOptions *options = syncManager.networkRequestOptions;
    NSDictionary<NSString *, NSString *> *headers = [self httpHeadersForPayload:jsonDictionary options:options];
    for (NSString *key in headers) {
        [request addValue:headers[key] forHTTPHeaderField:key];
    }
    id delegate = [RLMSessionDelegate delegateWithCertificatePaths:options.pinnedCertificatePaths
                                                        completion:completionBlock];
    auto session = [NSURLSession sessionWithConfiguration:NSURLSessionConfiguration.defaultSessionConfiguration
                                                 delegate:delegate delegateQueue:nil];

    // Add the request to a task and start it
    [[session dataTaskWithRequest:request] resume];
    // Tell the session to destroy itself once it's done with the request
    [session finishTasksAndInvalidate];
}

+ (NSString *)httpMethod {
    return @"POST";
}

+ (NSURL *)urlForAuthServer:(__unused NSURL *)authServerURL payload:(__unused NSDictionary *)json {
    NSAssert(NO, @"This method must be overriden by concrete subclasses.");
    return nil;
}

+ (NSData *)httpBodyForPayload:(NSDictionary *)json error:(NSError **)error {
    NSError *localError = nil;
    NSData *jsonData = [NSJSONSerialization dataWithJSONObject:json
                                                       options:(NSJSONWritingOptions)0
                                                         error:&localError];
    if (jsonData && !localError) {
        return jsonData;
    }
    NSAssert(localError, @"If there isn't a converted data object there must be an error.");
    if (error) {
        *error = localError;
    }
    return nil;
}

+ (NSDictionary<NSString *, NSString *> *)httpHeadersForPayload:(NSDictionary *)json
                                                        options:(nullable RLMNetworkRequestOptions *)options {
    NSMutableDictionary<NSString *, NSString *> *headers = [[NSMutableDictionary alloc] init];
    headers[@"Content-Type"] = @"application/json;charset=utf-8";
    headers[@"Accept"] = @"application/json";

    if (NSDictionary<NSString *, NSString *> *customHeaders = options.customHeaders) {
        [headers addEntriesFromDictionary:customHeaders];
    }
    if (NSString *authToken = [json objectForKey:kRLMSyncTokenKey]) {
        headers[options.authorizationHeaderName ?: @"Authorization"] = authToken;
    }

    return headers;
}
@end

@implementation RLMSessionDelegate {
    NSDictionary<NSString *, NSURL *> *_certificatePaths;
    NSData *_data;
    void (^_completionBlock)(NSError *, NSDictionary *);
}

+ (instancetype)delegateWithCertificatePaths:(NSDictionary *)paths
                                  completion:(void (^)(NSError *, NSDictionary *))completion {
    RLMSessionDelegate *delegate = [RLMSessionDelegate new];
    delegate->_certificatePaths = paths;
    delegate->_completionBlock = completion;
    return delegate;
}

- (void)URLSession:(__unused NSURLSession *)session didReceiveChallenge:(NSURLAuthenticationChallenge *)challenge
 completionHandler:(void (^)(NSURLSessionAuthChallengeDisposition disposition, NSURLCredential *credential))completionHandler {
    auto protectionSpace = challenge.protectionSpace;

    // Just fall back to the default logic for HTTP basic auth
    if (protectionSpace.authenticationMethod != NSURLAuthenticationMethodServerTrust || !protectionSpace.serverTrust) {
        completionHandler(NSURLSessionAuthChallengePerformDefaultHandling, nil);
        return;
    }

    // If we have a pinned certificate for this hostname, we want to validate
    // against that, and otherwise just do the default thing
    auto certPath = _certificatePaths[protectionSpace.host];
    if (!certPath) {
        completionHandler(NSURLSessionAuthChallengePerformDefaultHandling, nil);
        return;
    }
    if ([certPath isKindOfClass:[NSString class]]) {
        certPath = [NSURL fileURLWithPath:(id)certPath];
    }


    // Reject the server auth and report an error if any errors occur along the way
    CFArrayRef items = nil;
    NSError *error;
    auto reportStatus = realm::util::make_scope_exit([&]() noexcept {
        if (items) {
            CFRelease(items);
        }
        if (error) {
            _completionBlock(error, nil);
            // Don't also report errors about the connection itself failing later
            _completionBlock = ^(NSError *, id) { };
            completionHandler(NSURLSessionAuthChallengeRejectProtectionSpace, nil);
        }
    });

    NSData *data = [NSData dataWithContentsOfURL:certPath options:0 error:&error];
    if (!data) {
        return;
    }

    // Load our pinned certificate and add it to the anchor set
#if TARGET_OS_IPHONE
    id certificate = (__bridge_transfer id)SecCertificateCreateWithData(NULL, (__bridge CFDataRef)data);
    if (!certificate) {
        error = [NSError errorWithDomain:NSOSStatusErrorDomain code:errSecUnknownFormat userInfo:nil];
        return;
    }
    items = (CFArrayRef)CFBridgingRetain(@[certificate]);
#else
    SecItemImportExportKeyParameters params{
        .version = SEC_KEY_IMPORT_EXPORT_PARAMS_VERSION
    };
    if (OSStatus status = SecItemImport((__bridge CFDataRef)data, (__bridge CFStringRef)certPath.absoluteString,
                                        nullptr, nullptr, 0, &params, nullptr, &items)) {
        error = [NSError errorWithDomain:NSOSStatusErrorDomain code:status userInfo:nil];
        return;
    }
#endif
    SecTrustRef serverTrust = protectionSpace.serverTrust;
    if (OSStatus status = SecTrustSetAnchorCertificates(serverTrust, items)) {
        error = [NSError errorWithDomain:NSOSStatusErrorDomain code:status userInfo:nil];
        return;
    }

    // Only use our certificate and not the ones from the default CA roots
    if (OSStatus status = SecTrustSetAnchorCertificatesOnly(serverTrust, true)) {
        error = [NSError errorWithDomain:NSOSStatusErrorDomain code:status userInfo:nil];
        return;
    }

    // Verify that our pinned certificate is valid for this connection
    SecTrustResultType trustResult;
    if (OSStatus status = SecTrustEvaluate(serverTrust, &trustResult)) {
        error = [NSError errorWithDomain:NSOSStatusErrorDomain code:status userInfo:nil];
        return;
    }
    if (trustResult != kSecTrustResultProceed && trustResult != kSecTrustResultUnspecified) {
        completionHandler(NSURLSessionAuthChallengeRejectProtectionSpace, nil);
        return;
    }

    completionHandler(NSURLSessionAuthChallengeUseCredential,
                      [NSURLCredential credentialForTrust:protectionSpace.serverTrust]);
}

- (void)URLSession:(__unused NSURLSession *)session
          dataTask:(__unused NSURLSessionDataTask *)dataTask
    didReceiveData:(NSData *)data {
    if (!_data) {
        _data = data;
        return;
    }
    if (![_data respondsToSelector:@selector(appendData:)]) {
        _data = [_data mutableCopy];
    }
    [(id)_data appendData:data];
}

- (void)URLSession:(__unused NSURLSession *)session
              task:(NSURLSessionTask *)task
didCompleteWithError:(NSError *)error
{
    if (error) {
        _completionBlock(error, nil);
        return;
    }

    if (NSError *error = [self validateResponse:task.response data:_data]) {
        _completionBlock(error, nil);
        return;
    }

    id json = [NSJSONSerialization JSONObjectWithData:_data
                                              options:(NSJSONReadingOptions)0
                                                error:&error];
    if (!json) {
        _completionBlock(error, nil);
        return;
    }
    if (![json isKindOfClass:[NSDictionary class]]) {
        _completionBlock(make_auth_error_bad_response(json), nil);
        return;
    }

    _completionBlock(nil, (NSDictionary *)json);
}

- (NSError *)validateResponse:(NSURLResponse *)response data:(NSData *)data {
    if (![response isKindOfClass:[NSHTTPURLResponse class]]) {
        // FIXME: Provide error message
        return make_auth_error_bad_response();
    }

    NSHTTPURLResponse *httpResponse = (NSHTTPURLResponse *)response;
    BOOL badResponse = (NSLocationInRange(httpResponse.statusCode, rangeForErrorType(ClientError))
                        || NSLocationInRange(httpResponse.statusCode, rangeForErrorType(ServerError)));
    if (badResponse) {
        if (RLMSyncErrorResponseModel *responseModel = [self responseModelFromData:data]) {
            switch (responseModel.code) {
                case RLMSyncAuthErrorInvalidParameters:
                case RLMSyncAuthErrorMissingPath:
                case RLMSyncAuthErrorInvalidCredential:
                case RLMSyncAuthErrorUserDoesNotExist:
                case RLMSyncAuthErrorUserAlreadyExists:
                case RLMSyncAuthErrorAccessDeniedOrInvalidPath:
                case RLMSyncAuthErrorInvalidAccessToken:
                case RLMSyncAuthErrorExpiredPermissionOffer:
                case RLMSyncAuthErrorAmbiguousPermissionOffer:
                case RLMSyncAuthErrorFileCannotBeShared:
                    return make_auth_error(responseModel);
                default:
                    // Right now we assume that any codes not described
                    // above are generic HTTP error codes.
                    return make_auth_error_http_status(responseModel.status);
            }
        }
        return make_auth_error_http_status(httpResponse.statusCode);
    }

    if (!data) {
        // FIXME: provide error message
        return make_auth_error_bad_response();
    }

    return nil;
}

- (RLMSyncErrorResponseModel *)responseModelFromData:(NSData *)data {
    if (data.length == 0) {
        return nil;
    }
    id json = [NSJSONSerialization JSONObjectWithData:data
                                              options:(NSJSONReadingOptions)0
                                                error:nil];
    if (!json || ![json isKindOfClass:[NSDictionary class]]) {
        return nil;
    }
    return [[RLMSyncErrorResponseModel alloc] initWithDictionary:json];
}

@end

@implementation RLMNetworkRequestOptions
@end

#pragma mark - Endpoint Implementations

@implementation RLMSyncAuthEndpoint
+ (NSURL *)urlForAuthServer:(NSURL *)authServerURL payload:(__unused NSDictionary *)json {
    return [authServerURL URLByAppendingPathComponent:@"auth"];
}
@end

@implementation RLMSyncChangePasswordEndpoint
+ (NSString *)httpMethod {
    return @"PUT";
}

+ (NSURL *)urlForAuthServer:(NSURL *)authServerURL payload:(__unused NSDictionary *)json {
    return [authServerURL URLByAppendingPathComponent:@"auth/password"];
}
@end

@implementation RLMSyncUpdateAccountEndpoint
+ (NSURL *)urlForAuthServer:(NSURL *)authServerURL payload:(__unused NSDictionary *)json {
    return [authServerURL URLByAppendingPathComponent:@"auth/password/updateAccount"];
}
@end

@implementation RLMSyncGetUserInfoEndpoint
+ (NSString *)httpMethod {
    return @"GET";
}

+ (NSURL *)urlForAuthServer:(NSURL *)authServerURL payload:(NSDictionary *)json {
    NSString *provider = json[kRLMSyncProviderKey];
    NSString *providerID = json[kRLMSyncProviderIDKey];
    NSAssert([provider isKindOfClass:[NSString class]] && [providerID isKindOfClass:[NSString class]],
             @"malformed request; this indicates a logic error in the binding.");
    NSCharacterSet *allowed = [NSCharacterSet URLQueryAllowedCharacterSet];
    NSString *pathComponent = [NSString stringWithFormat:@"auth/users/%@/%@",
                               [provider stringByAddingPercentEncodingWithAllowedCharacters:allowed],
                               [providerID stringByAddingPercentEncodingWithAllowedCharacters:allowed]];
    return [authServerURL URLByAppendingPathComponent:pathComponent];
}

+ (NSData *)httpBodyForPayload:(__unused NSDictionary *)json error:(__unused NSError **)error {
    return nil;
}
@end

@implementation RLMSyncGetPermissionsEndpoint
+ (NSString *)httpMethod {
    return @"GET";
}
+ (NSURL *)urlForAuthServer:(NSURL *)authServerURL payload:(__unused NSDictionary *)json {
    return [authServerURL URLByAppendingPathComponent:@"permissions"];
}
@end

@implementation RLMSyncGetPermissionOffersEndpoint
+ (NSString *)httpMethod {
    return @"GET";
}
+ (NSURL *)urlForAuthServer:(NSURL *)authServerURL payload:(__unused NSDictionary *)json {
    return [authServerURL URLByAppendingPathComponent:@"permissions/offers"];
}
@end

@implementation RLMSyncApplyPermissionsEndpoint
+ (NSURL *)urlForAuthServer:(NSURL *)authServerURL payload:(__unused NSDictionary *)json {
    return [authServerURL URLByAppendingPathComponent:@"permissions/apply"];
}
@end

@implementation RLMSyncOfferPermissionsEndpoint
+ (NSURL *)urlForAuthServer:(NSURL *)authServerURL payload:(__unused NSDictionary *)json {
    return [authServerURL URLByAppendingPathComponent:@"permissions/offers"];
}
@end

@implementation RLMSyncAcceptPermissionOfferEndpoint
+ (NSURL *)urlForAuthServer:(NSURL *)authServerURL payload:(NSDictionary *)json {
    return [authServerURL URLByAppendingPathComponent:[NSString stringWithFormat:@"permissions/offers/%@/accept", json[@"offerToken"]]];
}
@end

@implementation RLMSyncInvalidatePermissionOfferEndpoint
+ (NSString *)httpMethod {
    return @"DELETE";
}
+ (NSURL *)urlForAuthServer:(NSURL *)authServerURL payload:(NSDictionary *)json {
    return [authServerURL URLByAppendingPathComponent:[NSString stringWithFormat:@"permissions/offers/%@", json[@"offerToken"]]];
}
@end
