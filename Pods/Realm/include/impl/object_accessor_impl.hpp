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

// An object accessor context which can be used to create and access objects
// using util::Any as the type-erased value type. In addition, this serves as
// the reference implementation of an accessor context that must be implemented
// by each binding.
class CppContext {
public:
    // This constructor is the only one used by the object accessor code, and is
    // used when recurring into a link or array property during object creation
    // (i.e. prop.type will always be Object or Array).
    CppContext(CppContext& c, Property const& prop)
    : realm(c.realm)
    , object_schema(prop.type == PropertyType::Object ? &*realm->schema().find(prop.object_type) : c.object_schema)
    { }

    CppContext() = default;
    CppContext(std::shared_ptr<Realm> realm, const ObjectSchema* os=nullptr)
    : realm(std::move(realm)), object_schema(os) { }

    // The use of util::Optional for the following two functions is not a hard
    // requirement; only that it be some type which can be evaluated in a
    // boolean context to determine if it contains a value, and if it does
    // contain a value it must be dereferencable to obtain that value.

    // Get the value for a property in an input object, or `util::none` if no
    // value present. The property is identified both by the name of the
    // property and its index within the ObjectScehma's persisted_properties
    // array.
    util::Optional<util::Any> value_for_property(util::Any& dict,
                                                 std::string const& prop_name,
                                                 size_t /* property_index */) const
    {
        auto const& v = any_cast<AnyDict&>(dict);
        auto it = v.find(prop_name);
        return it == v.end() ? util::none : util::make_optional(it->second);
    }

    // Get the default value for the given property in the given object schema,
    // or `util::none` if there is none (which is distinct from the default
    // being `null`).
    //
    // This implementation does not support default values; see the default
    // value tests for an example of one which does.
    util::Optional<util::Any>
    default_value_for_property(ObjectSchema const&, std::string const&) const
    {
        return util::none;
    }

    // Invoke `fn` with each of the values from an enumerable type
    template<typename Func>
    void enumerate_list(util::Any& value, Func&& fn) {
        for (auto&& v : any_cast<AnyVector&>(value))
            fn(v);
    }

    // Determine if `value` boxes the same List as `list`
    bool is_same_list(List const& list, util::Any const& value)
    {
        if (auto list2 = any_cast<List>(&value))
            return list == *list2;
        return false;
    }

    // Convert from core types to the boxed type
    util::Any box(BinaryData v) const { return std::string(v); }
    util::Any box(List v) const { return v; }
    util::Any box(Object v) const { return v; }
    util::Any box(Results v) const { return v; }
    util::Any box(StringData v) const { return std::string(v); }
    util::Any box(Timestamp v) const { return v; }
    util::Any box(bool v) const { return v; }
    util::Any box(double v) const { return v; }
    util::Any box(float v) const { return v; }
    util::Any box(int64_t v) const { return v; }
    util::Any box(util::Optional<bool> v) const { return v; }
    util::Any box(util::Optional<double> v) const { return v; }
    util::Any box(util::Optional<float> v) const { return v; }
    util::Any box(util::Optional<int64_t> v) const { return v; }
    util::Any box(RowExpr) const;

    // Any properties are only supported by the Cocoa binding to enable reading
    // old Realm files that may have used them. Other bindings can safely not
    // implement this.
    util::Any box(Mixed) const { REALM_TERMINATE("not supported"); }

    // Convert from the boxed type to core types. This needs to be implemented
    // for all of the types which `box()` can take, plus `RowExpr` and optional
    // versions of the numeric types, minus `List` and `Results`.
    //
    // `create` and `update` are only applicable to `unbox<RowExpr>`. If
    // `create` is false then when given something which is not a managed Realm
    // object `unbox()` should simply return a detached row expr, while if it's
    // true then `unbox()` should create a new object in the context's Realm
    // using the provided value. If `update` is true then upsert semantics
    // should be used for this.
    template<typename T>
    T unbox(util::Any& v, bool /*create*/= false, bool /*update*/= false) const { return any_cast<T>(v); }

    bool is_null(util::Any const& v) const noexcept { return !v.has_value(); }
    util::Any null_value() const noexcept { return {}; }
    util::Optional<util::Any> no_value() const noexcept { return {}; }

    // KVO hooks which will be called before and after modying a property from
    // within Object::create().
    void will_change(Object const&, Property const&) {}
    void did_change() {}

    // Get a string representation of the given value for use in error messages.
    std::string print(util::Any const&) const { return "not implemented"; }

    // Cocoa allows supplying fewer values than there are properties when
    // creating objects using an array of values. Other bindings should not
    // mimick this behavior so just return false here.
    bool allow_missing(util::Any const&) const { return false; }

private:
    std::shared_ptr<Realm> realm;
    const ObjectSchema* object_schema = nullptr;

};

inline util::Any CppContext::box(RowExpr row) const
{
    REALM_ASSERT(object_schema);
    return Object(realm, *object_schema, row);
}

template<>
inline StringData CppContext::unbox(util::Any& v, bool, bool) const
{
    if (!v.has_value())
        return StringData();
    auto& value = any_cast<std::string&>(v);
    return StringData(value.c_str(), value.size());
}

template<>
inline BinaryData CppContext::unbox(util::Any& v, bool, bool) const
{
    if (!v.has_value())
        return BinaryData();
    auto& value = any_cast<std::string&>(v);
    return BinaryData(value.c_str(), value.size());
}

template<>
inline RowExpr CppContext::unbox(util::Any& v, bool create, bool update) const
{
    if (auto object = any_cast<Object>(&v))
        return object->row();
    if (auto row = any_cast<RowExpr>(&v))
        return *row;
    if (!create)
        return RowExpr();

    REALM_ASSERT(object_schema);
    return Object::create(const_cast<CppContext&>(*this), realm, *object_schema, v, update).row();
}

template<>
inline util::Optional<bool> CppContext::unbox(util::Any& v, bool, bool) const
{
    return v.has_value() ? util::make_optional(unbox<bool>(v)) : util::none;
}

template<>
inline util::Optional<int64_t> CppContext::unbox(util::Any& v, bool, bool) const
{
    return v.has_value() ? util::make_optional(unbox<int64_t>(v)) : util::none;
}

template<>
inline util::Optional<double> CppContext::unbox(util::Any& v, bool, bool) const
{
    return v.has_value() ? util::make_optional(unbox<double>(v)) : util::none;
}

template<>
inline util::Optional<float> CppContext::unbox(util::Any& v, bool, bool) const
{
    return v.has_value() ? util::make_optional(unbox<float>(v)) : util::none;
}

template<>
inline Mixed CppContext::unbox(util::Any&, bool, bool) const
{
    throw std::logic_error("'Any' type is unsupported");
}
}

#endif // REALM_OS_OBJECT_ACCESSOR_IMPL_HPP
