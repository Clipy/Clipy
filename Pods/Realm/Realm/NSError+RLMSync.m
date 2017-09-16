////////////////////////////////////////////////////////////////////////////
//
// Copyright 2017 Realm Inc.
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

#import "NSError+RLMSync.h"

#import "RLMSyncUtil.h"

@implementation NSError (RLMSync)

- (void(^)(void))rlmSync_clientResetBlock {
    if (self.domain == RLMSyncErrorDomain && self.code == RLMSyncErrorClientResetError) {
        return self.userInfo[kRLMSyncInitiateClientResetBlockKey];
    }
    return nil;
}

- (void(^)(void))rlmSync_deleteRealmBlock {
    if (self.domain == RLMSyncErrorDomain && self.code == RLMSyncErrorPermissionDeniedError) {
        return self.userInfo[kRLMSyncInitiateDeleteRealmBlockKey];
    }
    return nil;
}

- (NSString *)rlmSync_clientResetBackedUpRealmPath {
    if (self.domain == RLMSyncErrorDomain && self.code == RLMSyncErrorClientResetError) {
        return self.userInfo[kRLMSyncPathOfRealmBackupCopyKey];
    }
    return nil;
}

@end
