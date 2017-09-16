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

#import "RLMCollection_Private.hpp"

#import "RLMAccessor.hpp"
#import "RLMArray_Private.hpp"
#import "RLMListBase.h"
#import "RLMObjectSchema_Private.hpp"
#import "RLMObjectStore.h"
#import "RLMObject_Private.hpp"
#import "RLMProperty_Private.h"

#import "collection_notifications.hpp"
#import "list.hpp"
#import "results.hpp"

static const int RLMEnumerationBufferSize = 16;

@implementation RLMFastEnumerator {
    // The buffer supplied by fast enumeration does not retain the objects given
    // to it, but because we create objects on-demand and don't want them
    // autoreleased (a table can have more rows than the device has memory for
    // accessor objects) we need a thing to retain them.
    id _strongBuffer[RLMEnumerationBufferSize];

    RLMRealm *_realm;
    RLMClassInfo *_info;

    // A pointer to either _snapshot or a Results from the source collection,
    // to avoid having to copy the Results when not in a write transaction
    realm::Results *_results;
    realm::Results _snapshot;

    // A strong reference to the collection being enumerated to ensure it stays
    // alive when we're holding a pointer to a member in it
    id _collection;
}

- (instancetype)initWithList:(realm::List&)list
                  collection:(id)collection
                       realm:(RLMRealm *)realm
                   classInfo:(RLMClassInfo&)info
{
    self = [super init];
    if (self) {
        if (realm.inWriteTransaction) {
            _snapshot = list.snapshot();
        }
        else {
            _snapshot = list.as_results();
            _collection = collection;
            [realm registerEnumerator:self];
        }
        _results = &_snapshot;
        _realm = realm;
        _info = &info;
    }
    return self;
}

- (instancetype)initWithResults:(realm::Results&)results
                     collection:(id)collection
                          realm:(RLMRealm *)realm
                      classInfo:(RLMClassInfo&)info
{
    self = [super init];
    if (self) {
        if (realm.inWriteTransaction) {
            _snapshot = results.snapshot();
            _results = &_snapshot;
        }
        else {
            _results = &results;
            _collection = collection;
            [realm registerEnumerator:self];
        }
        _realm = realm;
        _info = &info;
    }
    return self;
}

- (void)dealloc {
    if (_collection) {
        [_realm unregisterEnumerator:self];
    }
}

- (void)detach {
    _snapshot = _results->snapshot();
    _results = &_snapshot;
    _collection = nil;
}

- (NSUInteger)countByEnumeratingWithState:(NSFastEnumerationState *)state
                                    count:(NSUInteger)len {
    [_realm verifyThread];
    if (!_results->is_valid()) {
        @throw RLMException(@"Collection is no longer valid");
    }
    // The fast enumeration buffer size is currently a hardcoded number in the
    // compiler so this can't actually happen, but just in case it changes in
    // the future...
    if (len > RLMEnumerationBufferSize) {
        len = RLMEnumerationBufferSize;
    }

    NSUInteger batchCount = 0, count = state->extra[1];

    @autoreleasepool {
        RLMAccessorContext ctx(_realm, *_info);
        for (NSUInteger index = state->state; index < count && batchCount < len; ++index) {
            _strongBuffer[batchCount] = _results->get(ctx, index);
            batchCount++;
        }
    }

    for (NSUInteger i = batchCount; i < len; ++i) {
        _strongBuffer[i] = nil;
    }

    if (batchCount == 0) {
        // Release our data if we're done, as we're autoreleased and so may
        // stick around for a while
        if (_collection) {
            _collection = nil;
            [_realm unregisterEnumerator:self];
        }
        _snapshot = {};
    }

    state->itemsPtr = (__unsafe_unretained id *)(void *)_strongBuffer;
    state->state += batchCount;
    state->mutationsPtr = state->extra+1;

    return batchCount;
}
@end

NSUInteger RLMFastEnumerate(NSFastEnumerationState *state, NSUInteger len, id<RLMFastEnumerable> collection) {
    __autoreleasing RLMFastEnumerator *enumerator;
    if (state->state == 0) {
        enumerator = collection.fastEnumerator;
        state->extra[0] = (long)enumerator;
        state->extra[1] = collection.count;
    }
    else {
        enumerator = (__bridge id)(void *)state->extra[0];
    }

    return [enumerator countByEnumeratingWithState:state count:len];
}

template<typename Collection>
NSArray *RLMCollectionValueForKey(Collection& collection, NSString *key,
                                  RLMRealm *realm, RLMClassInfo& info) {
    size_t count = collection.size();
    if (count == 0) {
        return @[];
    }

    NSMutableArray *array = [NSMutableArray arrayWithCapacity:count];
    if ([key isEqualToString:@"self"]) {
        RLMAccessorContext context(realm, info);
        for (size_t i = 0; i < count; ++i) {
            [array addObject:collection.get(context, i) ?: NSNull.null];
        }
        return array;
    }

    RLMObject *accessor = RLMCreateManagedAccessor(info.rlmObjectSchema.accessorClass, realm, &info);

    // List properties need to be handled specially since we need to create a
    // new List each time
    if (info.rlmObjectSchema.isSwiftClass) {
        auto prop = info.rlmObjectSchema[key];
        if (prop && prop.type == RLMPropertyTypeArray && prop.swiftIvar) {
            // Grab the actual class for the generic List from an instance of it
            // so that we can make instances of the List without creating a new
            // object accessor each time
            Class cls = [object_getIvar(accessor, prop.swiftIvar) class];
            for (size_t i = 0; i < count; ++i) {
                RLMListBase *list = [[cls alloc] init];
                list._rlmArray = [[RLMManagedArray alloc] initWithList:realm::List(realm->_realm, *info.table(),
                                                                                   info.tableColumn(prop),
                                                                                   collection.get(i).get_index())
                                                                 realm:realm parentInfo:&info
                                                              property:prop];
                [array addObject:list];
            }
            return array;
        }
    }

    for (size_t i = 0; i < count; i++) {
        accessor->_row = collection.get(i);
        RLMInitializeSwiftAccessorGenerics(accessor);
        [array addObject:[accessor valueForKey:key] ?: NSNull.null];
    }
    return array;
}

template NSArray *RLMCollectionValueForKey(realm::Results&, NSString *, RLMRealm *, RLMClassInfo&);
template NSArray *RLMCollectionValueForKey(realm::List&, NSString *, RLMRealm *, RLMClassInfo&);

void RLMCollectionSetValueForKey(id<RLMFastEnumerable> collection, NSString *key, id value) {
    realm::TableView tv = [collection tableView];
    if (tv.size() == 0) {
        return;
    }

    RLMRealm *realm = collection.realm;
    RLMClassInfo *info = collection.objectInfo;
    RLMObject *accessor = RLMCreateManagedAccessor(info->rlmObjectSchema.accessorClass, realm, info);
    for (size_t i = 0; i < tv.size(); i++) {
        accessor->_row = tv[i];
        RLMInitializeSwiftAccessorGenerics(accessor);
        [accessor setValue:value forKey:key];
    }
}

NSString *RLMDescriptionWithMaxDepth(NSString *name,
                                     id<RLMCollection> collection,
                                     NSUInteger depth) {
    if (depth == 0) {
        return @"<Maximum depth exceeded>";
    }

    const NSUInteger maxObjects = 100;
    auto str = [NSMutableString stringWithFormat:@"%@<%@> <%p> (\n", name,
                [collection objectClassName], (void *)collection];
    size_t index = 0, skipped = 0;
    for (id obj in collection) {
        NSString *sub;
        if ([obj respondsToSelector:@selector(descriptionWithMaxDepth:)]) {
            sub = [obj descriptionWithMaxDepth:depth - 1];
        }
        else {
            sub = [obj description];
        }

        // Indent child objects
        NSString *objDescription = [sub stringByReplacingOccurrencesOfString:@"\n"
                                                                  withString:@"\n\t"];
        [str appendFormat:@"\t[%zu] %@,\n", index++, objDescription];
        if (index >= maxObjects) {
            skipped = collection.count - maxObjects;
            break;
        }
    }

    // Remove last comma and newline characters
    if (collection.count > 0) {
        [str deleteCharactersInRange:NSMakeRange(str.length-2, 2)];
    }
    if (skipped) {
        [str appendFormat:@"\n\t... %zu objects skipped.", skipped];
    }
    [str appendFormat:@"\n)"];
    return str;
}

std::vector<std::pair<std::string, bool>> RLMSortDescriptorsToKeypathArray(NSArray<RLMSortDescriptor *> *properties) {
    std::vector<std::pair<std::string, bool>> keypaths;
    keypaths.reserve(properties.count);
    for (RLMSortDescriptor *desc in properties) {
        if ([desc.keyPath rangeOfString:@"@"].location != NSNotFound) {
            @throw RLMException(@"Cannot sort on key path '%@': KVC collection operators are not supported.", desc.keyPath);
        }
        keypaths.push_back({desc.keyPath.UTF8String, desc.ascending});
    }
    return keypaths;
}

@implementation RLMCancellationToken {
    realm::NotificationToken _token;
    __unsafe_unretained RLMRealm *_realm;
}
- (instancetype)initWithToken:(realm::NotificationToken)token realm:(RLMRealm *)realm {
    self = [super init];
    if (self) {
        _token = std::move(token);
        _realm = realm;
    }
    return self;
}

- (RLMRealm *)realm {
    return _realm;
}

- (void)suppressNextNotification {
    _token.suppress_next();
}

- (void)stop {
    _token = {};
}

@end

@implementation RLMCollectionChange {
    realm::CollectionChangeSet _indices;
}

- (instancetype)initWithChanges:(realm::CollectionChangeSet)indices {
    self = [super init];
    if (self) {
        _indices = std::move(indices);
    }
    return self;
}

static NSArray *toArray(realm::IndexSet const& set) {
    NSMutableArray *ret = [NSMutableArray new];
    for (auto index : set.as_indexes()) {
        [ret addObject:@(index)];
    }
    return ret;
}

- (NSArray *)insertions {
    return toArray(_indices.insertions);
}

- (NSArray *)deletions {
    return toArray(_indices.deletions);
}

- (NSArray *)modifications {
    return toArray(_indices.modifications);
}

static NSArray *toIndexPathArray(realm::IndexSet const& set, NSUInteger section) {
    NSMutableArray *ret = [NSMutableArray new];
    NSUInteger path[2] = {section, 0};
    for (auto index : set.as_indexes()) {
        path[1] = index;
        [ret addObject:[NSIndexPath indexPathWithIndexes:path length:2]];
    }
    return ret;
}

- (NSArray<NSIndexPath *> *)deletionsInSection:(NSUInteger)section {
    return toIndexPathArray(_indices.deletions, section);
}

- (NSArray<NSIndexPath *> *)insertionsInSection:(NSUInteger)section {
    return toIndexPathArray(_indices.insertions, section);

}

- (NSArray<NSIndexPath *> *)modificationsInSection:(NSUInteger)section {
    return toIndexPathArray(_indices.modifications, section);

}
@end

template<typename Collection>
RLMNotificationToken *RLMAddNotificationBlock(id objcCollection,
                                              Collection& collection,
                                              void (^block)(id, RLMCollectionChange *, NSError *),
                                              bool suppressInitialChange) {
    struct IsValid {
        static bool call(realm::List const& list) {
            return list.is_valid();
        }
        static bool call(realm::Results const&) {
            return true;
        }
    };

    auto skip = suppressInitialChange ? std::make_shared<bool>(true) : nullptr;
    auto cb = [=, &collection](realm::CollectionChangeSet const& changes,
                               std::exception_ptr err) {
        if (err) {
            try {
                rethrow_exception(err);
            }
            catch (...) {
                NSError *error = nil;
                RLMRealmTranslateException(&error);
                block(nil, nil, error);
                return;
            }
        }

        if (!IsValid::call(collection)) {
            return;
        }

        if (skip && *skip) {
            *skip = false;
            block(objcCollection, nil, nil);
        }
        else if (changes.empty()) {
            block(objcCollection, nil, nil);
        }
        else {
            block(objcCollection, [[RLMCollectionChange alloc] initWithChanges:changes], nil);
        }
    };

    return [[RLMCancellationToken alloc] initWithToken:collection.add_notification_callback(cb)
                                                 realm:(RLMRealm *)[objcCollection realm]];
}

// Explicitly instantiate the templated function for the two types we'll use it on
template RLMNotificationToken *RLMAddNotificationBlock<realm::List>(id, realm::List&, void (^)(id, RLMCollectionChange *, NSError *), bool);
template RLMNotificationToken *RLMAddNotificationBlock<realm::Results>(id, realm::Results&, void (^)(id, RLMCollectionChange *, NSError *), bool);
