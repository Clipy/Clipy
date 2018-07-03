////////////////////////////////////////////////////////////////////////////
//
// Copyright 2014 Realm Inc.
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

#import "RLMMigration_Private.h"

#import "RLMAccessor.h"
#import "RLMObject_Private.h"
#import "RLMObject_Private.hpp"
#import "RLMObjectSchema_Private.hpp"
#import "RLMObjectStore.h"
#import "RLMProperty_Private.h"
#import "RLMRealm_Dynamic.h"
#import "RLMRealm_Private.hpp"
#import "RLMResults_Private.hpp"
#import "RLMSchema_Private.hpp"
#import "RLMUtil.hpp"

#import "object_store.hpp"
#import "shared_realm.hpp"
#import "schema.hpp"

#import <realm/table.hpp>

using namespace realm;

// The source realm for a migration has to use a SharedGroup to be able to share
// the file with the destination realm, but we don't want to let the user call
// beginWriteTransaction on it as that would make no sense.
@interface RLMMigrationRealm : RLMRealm
@end

@implementation RLMMigrationRealm
- (BOOL)readonly {
    return YES;
}

- (void)beginWriteTransaction {
    @throw RLMException(@"Cannot modify the source Realm in a migration");
}
@end

@implementation RLMMigration {
    realm::Schema *_schema;
    std::unordered_map<NSString *, realm::IndexSet> _deletedObjectIndices;
}

- (instancetype)initWithRealm:(RLMRealm *)realm oldRealm:(RLMRealm *)oldRealm schema:(realm::Schema &)schema {
    self = [super init];
    if (self) {
        _realm = realm;
        _oldRealm = oldRealm;
        _schema = &schema;
        object_setClass(_oldRealm, RLMMigrationRealm.class);
    }
    return self;
}

- (RLMSchema *)oldSchema {
    return self.oldRealm.schema;
}

- (RLMSchema *)newSchema {
    return self.realm.schema;
}

- (void)enumerateObjects:(NSString *)className block:(__attribute__((noescape)) RLMObjectMigrationBlock)block {
    RLMResults *objects = [_realm.schema schemaForClassName:className] ? [_realm allObjects:className] : nil;
    RLMResults *oldObjects = [_oldRealm.schema schemaForClassName:className] ? [_oldRealm allObjects:className] : nil;

    // For whatever reason if this is a newly added table we enumerate the
    // objects in it, while in all other cases we enumerate only the existing
    // objects. It's unclear how this could be useful, but changing it would
    // also be a pointless breaking change and it's unlikely to be hurting anyone.
    if (objects && !oldObjects) {
        for (auto i = objects.count; i > 0; --i) {
            @autoreleasepool {
                block(nil, objects[i - 1]);
            }
        }
        return;
    }

    auto count = oldObjects.count;
    if (count == 0) {
        return;
    }
    auto deletedObjects = _deletedObjectIndices.find(className);
    for (auto i = count; i > 0; --i) {
        auto index = i - 1;
        if (deletedObjects != _deletedObjectIndices.end() && deletedObjects->second.contains(index)) {
            continue;
        }
        @autoreleasepool {
            block(oldObjects[index], objects[index]);
        }
    }
}

- (void)execute:(RLMMigrationBlock)block {
    @autoreleasepool {
        // disable all primary keys for migration and use DynamicObject for all types
        for (RLMObjectSchema *objectSchema in _realm.schema.objectSchema) {
            objectSchema.accessorClass = RLMDynamicObject.class;
            objectSchema.primaryKeyProperty.isPrimary = NO;
        }
        for (RLMObjectSchema *objectSchema in _oldRealm.schema.objectSchema) {
            objectSchema.accessorClass = RLMDynamicObject.class;
        }

        block(self, _oldRealm->_realm->schema_version());

        [self deleteObjectsMarkedForDeletion];

        _oldRealm = nil;
        _realm = nil;
    }
}

- (RLMObject *)createObject:(NSString *)className withValue:(id)value {
    return [_realm createObject:className withValue:value];
}

- (RLMObject *)createObject:(NSString *)className withObject:(id)object {
    return [self createObject:className withValue:object];
}

- (void)deleteObject:(RLMObject *)object {
    _deletedObjectIndices[object.objectSchema.className].add(object->_row.get_index());
}

- (void)deleteObjectsMarkedForDeletion {
    for (auto& objectType : _deletedObjectIndices) {
        TableRef table = ObjectStore::table_for_object_type(_realm.group, objectType.first.UTF8String);
        if (!table) {
            continue;
        }

        auto& indices = objectType.second;
        // Just clear the table if we're removing all of the rows
        if (table->size() == indices.count()) {
            table->clear();
        }
        // Otherwise delete in reverse order to avoid invaliding any of the
        // not-yet-deleted indices
        else {
            for (auto it = std::make_reverse_iterator(indices.end()), end = std::make_reverse_iterator(indices.begin()); it != end; ++it) {
                for (size_t i = it->second; i > it->first; --i) {
                    table->move_last_over(i - 1);
                }
            }
        }
    }
}

- (BOOL)deleteDataForClassName:(NSString *)name {
    if (!name) {
        return false;
    }

    TableRef table = ObjectStore::table_for_object_type(_realm.group, name.UTF8String);
    if (!table) {
        return false;
    }
    _deletedObjectIndices[name].set(table->size());
    if (![_realm.schema schemaForClassName:name]) {
        realm::ObjectStore::delete_data_for_object(_realm.group, name.UTF8String);
    }

    return true;
}

- (void)renamePropertyForClass:(NSString *)className oldName:(NSString *)oldName newName:(NSString *)newName {
    realm::ObjectStore::rename_property(_realm.group, *_schema, className.UTF8String,
                                        oldName.UTF8String, newName.UTF8String);
}

@end
