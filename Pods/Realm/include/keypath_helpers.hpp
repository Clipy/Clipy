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

#include "object_schema.hpp"
#include "object_store.hpp"
#include "shared_realm.hpp"

#include <realm/parser/keypath_mapping.hpp>

namespace realm {
/// Create the mappings from user defined names of linkingObjects into the verbose
/// syntax that the parser supports: @links.Class.property.
inline void alias_backlinks(parser::KeyPathMapping& mapping, Realm& realm)
{
    for (auto& object_schema : realm.schema()) {
        for (Property const& property : object_schema.computed_properties) {
            if (property.type == PropertyType::LinkingObjects) {
                auto table = ObjectStore::table_for_object_type(realm.read_group(), object_schema.name);
                auto native_name = util::format("@links.%1.%2",
                                                ObjectStore::table_name_for_object_type(property.object_type),
                                                property.link_origin_property_name);
                mapping.add_mapping(table, property.name, std::move(native_name));
            }
        }
    }
}

/// Generate an IncludeDescriptor from a list of key paths.
///
/// Each key path in the list is a period ('.') seperated property path, beginning
/// at the class defined by `object_schema` and ending with a linkingObjects relationship.
inline IncludeDescriptor generate_include_from_keypaths(std::vector<StringData> const& paths,
                                                        Realm& realm, ObjectSchema const& object_schema,
                                                        parser::KeyPathMapping& mapping)
{
    auto base_table = ObjectStore::table_for_object_type(realm.read_group(), object_schema.name);
    REALM_ASSERT(base_table);
    // FIXME: the following is mostly copied from core's query_builder::apply_ordering
    std::vector<std::vector<LinkPathPart>> properties;
    for (size_t i = 0; i < paths.size(); ++i) {
        StringData keypath = paths[i];
        if (keypath.size() == 0) {
            throw InvalidPathError("missing property name while generating INCLUDE from keypaths");
        }

        util::KeyPath path = util::key_path_from_string(keypath);
        size_t index = 0;
        std::vector<LinkPathPart> links;
        ConstTableRef cur_table = base_table;

        while (index < path.size()) {
            parser::KeyPathElement element = mapping.process_next_path(cur_table, path, index); // throws if invalid
            // backlinks use type_LinkList since list operations apply to them (and is_backlink is set)
            if (element.col_type != type_Link && element.col_type != type_LinkList) {
                throw InvalidPathError(util::format("Property '%1' is not a link in object of type '%2' in 'INCLUDE' clause",
                                                    element.table->get_column_name(element.col_ndx),
                                                    get_printable_table_name(*element.table)));
            }
            if (element.table == cur_table) {
                if (element.col_ndx == realm::npos) {
                    cur_table = element.table;
                }
                else {
                    cur_table = element.table->get_link_target(element.col_ndx); // advance through forward link
                }
            }
            else {
                cur_table = element.table; // advance through backlink
            }
            ConstTableRef tr;
            if (element.is_backlink) {
                tr = element.table;
            }
            links.emplace_back(element.col_ndx, tr);
        }
        properties.push_back(std::move(links));
    }
    return IncludeDescriptor{*base_table, properties};
}
}
