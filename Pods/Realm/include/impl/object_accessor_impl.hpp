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

#ifndef REALM_OS_OBJECT_ACCESSOR_IMPL_HPP
#define REALM_OS_OBJECT_ACCESSOR_IMPL_HPP

#include "object_accessor.hpp"
#include "util/any.hpp"

namespace realm {
using AnyDict = std::map<std::string, util::Any>;
using AnyVector = std::vector<util::Any>;

struct CppContext {
    std::map<std::string, AnyDict> defaults;
};

template<>
class NativeAccessor<util::Any, CppContext*> {
public:
    static bool dict_has_value_for_key(CppContext*, util::Any dict, const std::string &prop_name) {
        return any_cast<AnyDict>(dict).count(prop_name) != 0;
    }

    static util::Any dict_value_for_key(CppContext*, util::Any dict, const std::string &prop_name) {
        return any_cast<AnyDict>(dict).at(prop_name);
    }

    static size_t list_size(CppContext*, util::Any& v) { return any_cast<AnyVector>(v).size(); }
    static util::Any list_value_at_index(CppContext*, util::Any& v, size_t index) { return any_cast<AnyVector>(v)[index]; }

    static bool has_default_value_for_property(CppContext* context, Realm*, ObjectSchema const& object, std::string const& prop) {
        auto it = context->defaults.find(object.name);
        if (it != context->defaults.end())
            return it->second.count(prop);
        return false;
    }

    static util::Any default_value_for_property(CppContext* context, Realm*, ObjectSchema const& object, std::string const& prop) {
        return context->defaults.at(object.name).at(prop);
    }

    static Timestamp to_timestamp(CppContext*, util::Any& v) { return any_cast<Timestamp>(v); }
    static bool to_bool(CppContext*, util::Any& v) { return any_cast<bool>(v); }
    static double to_double(CppContext*, util::Any& v) { return any_cast<double>(v); }
    static float to_float(CppContext*, util::Any& v) { return any_cast<float>(v); }
    static long long to_long(CppContext*, util::Any& v) { return any_cast<long long>(v); }
    static std::string to_binary(CppContext*, util::Any& v) { return any_cast<std::string>(v); }
    static std::string to_string(CppContext*, util::Any& v) { return any_cast<std::string>(v); }
    static Mixed to_mixed(CppContext*, util::Any&) { throw std::logic_error("'Mixed' type is unsupported"); }

    static util::Any from_binary(CppContext*, BinaryData v) { return std::string(v); }
    static util::Any from_bool(CppContext*, bool v) { return v; }
    static util::Any from_double(CppContext*, double v) { return v; }
    static util::Any from_float(CppContext*, float v) { return v; }
    static util::Any from_long(CppContext*, long long v) { return v; }
    static util::Any from_string(CppContext*, StringData v) { return std::string(v); }
    static util::Any from_timestamp(CppContext*, Timestamp v) { return v; }
    static util::Any from_list(CppContext*, List v) { return v; }
    static util::Any from_results(CppContext*, Results v) { return v; }
    static util::Any from_object(CppContext*, Object v) { return v; }

    static bool is_null(CppContext*, util::Any& v) { return !v.has_value(); }
    static util::Any null_value(CppContext*) { return {}; }

    static size_t to_existing_object_index(CppContext*, SharedRealm, util::Any &) { REALM_TERMINATE("not implemented"); }
    static size_t to_object_index(CppContext* context, SharedRealm realm, util::Any& value, std::string const& object_type, bool update) {
        if (auto object = any_cast<Object>(&value)) {
            return object->row().get_index();
        }
        return Object::create(context, realm, *realm->schema().find(object_type), value, update).row().get_index();
    }
};
}

#endif /* REALM_OS_OBJECT_ACCESSOR_IMPL_HPP */
