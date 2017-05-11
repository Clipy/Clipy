////////////////////////////////////////////////////////////////////////////
//
// Copyright 2015 Realm Inc.
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

#ifndef REALM_PROPERTY_HPP
#define REALM_PROPERTY_HPP

#include <string>

#include "util/compiler.hpp"

namespace realm {
    enum class PropertyType : unsigned char {
        Int    = 0,
        Bool   = 1,
        Float  = 9,
        Double = 10,
        String = 2,
        Data   = 4,
        Any    = 6, // deprecated and will be removed in the future
        Date   = 8,
        Object = 12,
        Array  = 13,
        LinkingObjects  = 14,
    };

    static const char *string_for_property_type(PropertyType type);

    struct Property {
        std::string name = "";
        PropertyType type = PropertyType::Int;
        std::string object_type = "";
        std::string link_origin_property_name = "";
        bool is_primary = false;
        bool is_indexed = false;
        bool is_nullable = false;

        size_t table_column = -1;
        bool requires_index() const { return is_primary || is_indexed; }

        bool is_indexable() const
        {
            return type == PropertyType::Int
                || type == PropertyType::Bool
                || type == PropertyType::Date
                || type == PropertyType::String;
        }

        bool type_is_nullable() const
        {
            return type == PropertyType::Int
                || type == PropertyType::Bool
                || type == PropertyType::Float
                || type == PropertyType::Double
                || type == PropertyType::Date
                || type == PropertyType::String
                || type == PropertyType::Data
                || type == PropertyType::Object;
        }

        std::string type_string() const
        {
            switch (type) {
                case PropertyType::Object:
                    return "<" + object_type + ">";
                case PropertyType::Array:
                    return "array<" + object_type + ">";
                case PropertyType::LinkingObjects:
                    return "linking objects<" + object_type + ">";
                default:
                    return string_for_property_type(type);
            }
        }

        enum class Nullable { yes, no };
        enum class PrimaryKey { yes, no };
        enum class Indexed { yes, no };
        static Property make(std::string name, PropertyType type,
                             PrimaryKey is_primary=PrimaryKey::no,
                             Indexed is_indexed=Indexed::no,
                             Nullable is_nullable=Nullable::no)
        { 
            return { std::move(name), type, "", "", is_primary == PrimaryKey::yes, is_indexed == Indexed::yes, is_nullable == Nullable::yes };
        }

#if __GNUC__ < 5
        // GCC 4.9 does not support C++14 braced-init with NSDMIs
        Property(std::string name="", PropertyType type=PropertyType::Int,
                 std::string object_type="", std::string link_origin_property_name="",
                 bool is_primary=false, bool is_indexed=false, bool is_nullable=false)
        : name(std::move(name))
        , type(type)
        , object_type(std::move(object_type))
        , link_origin_property_name(std::move(link_origin_property_name))
        , is_primary(is_primary)
        , is_indexed(is_indexed)
        , is_nullable(is_nullable)
        {
        }
#endif
    };

    inline bool operator==(Property const& lft, Property const& rgt)
    {
        // note: not checking table_column
        // ordered roughly by the cost of the check
        return lft.type == rgt.type
            && lft.is_primary == rgt.is_primary
            && lft.is_nullable == rgt.is_nullable
            && lft.requires_index() == rgt.requires_index()
            && lft.name == rgt.name
            && lft.object_type == rgt.object_type
            && lft.link_origin_property_name == rgt.link_origin_property_name;
    }

    static const char *string_for_property_type(PropertyType type)
    {
        switch (type) {
            case PropertyType::String:
                return "string";
            case PropertyType::Int:
                return "int";
            case PropertyType::Bool:
                return "bool";
            case PropertyType::Date:
                return "date";
            case PropertyType::Data:
                return "data";
            case PropertyType::Double:
                return "double";
            case PropertyType::Float:
                return "float";
            case PropertyType::Any:
                return "any";
            case PropertyType::Object:
                return "object";
            case PropertyType::Array:
                return "array";
            case PropertyType::LinkingObjects:
                return "linking objects";
#if __GNUC__ || _MSC_VER
            default:
                REALM_COMPILER_HINT_UNREACHABLE();
#endif
        }
    }
}

#endif /* REALM_PROPERTY_HPP */
