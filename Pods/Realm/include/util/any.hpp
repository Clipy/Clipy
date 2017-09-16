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

#ifndef REALM_OS_UTIL_ANY_HPP
#define REALM_OS_UTIL_ANY_HPP

#include <memory>
#include <stdexcept>
#include <type_traits>
#include <typeinfo>

namespace realm {
namespace util {

// A naive implementation of C++17's std::any
// This does not perform the small-object optimization or make any particular
// attempt at being performant
class Any final {
public:
    // Constructors

    Any() = default;
    Any(Any&&) noexcept = default;
    ~Any() = default;
    Any& operator=(Any&&) noexcept = default;

    Any(Any const& rhs)
    : m_value(rhs.m_value ? rhs.m_value->copy() : nullptr)
    {
    }

    template<typename T, typename = typename std::enable_if<!std::is_same<typename std::decay<T>::type, Any>::value>::type>
    Any(T&& value)
    : m_value(std::make_unique<Value<typename std::decay<T>::type>>(std::forward<T>(value)))
    {
    }

    Any& operator=(Any const& rhs)
    {
        m_value = rhs.m_value ? rhs.m_value->copy() : nullptr;
        return *this;
    }

    template<typename T, typename = typename std::enable_if<!std::is_same<typename std::decay<T>::type, Any>::value>::type>
    Any& operator=(T&& value)
    {
        m_value = std::make_unique<Value<typename std::decay<T>::type>>(std::forward<T>(value));
        return *this;
    }

    // Modifiers

    void reset() noexcept { m_value.reset(); }
    void swap(Any& rhs) noexcept { std::swap(m_value, rhs.m_value); }

    // Observers

    bool has_value() const noexcept { return m_value != nullptr; }
    std::type_info const& type() const noexcept { return m_value ? m_value->type() : typeid(void); }

private:
    struct ValueBase {
        virtual ~ValueBase() noexcept { }
        virtual std::type_info const& type() const noexcept = 0;
        virtual std::unique_ptr<ValueBase> copy() const = 0;
    };
    template<typename T>
    struct Value : ValueBase {
        T value;
        template<typename U> Value(U&& v) : value(std::forward<U>(v)) { }

        std::type_info const& type() const noexcept override { return typeid(T); }
        std::unique_ptr<ValueBase> copy() const override
        {
            return std::make_unique<Value<T>>(value);
        }
    };
    std::unique_ptr<ValueBase> m_value;

    template<typename T>
    friend const T* any_cast(const Any* operand) noexcept;
    template<typename T>
    friend T* any_cast(Any* operand) noexcept;

    template<typename T>
    const T* cast() const noexcept
    {
        return &static_cast<Value<T>*>(m_value.get())->value;
    }

    template<typename T>
    T* cast() noexcept
    {
        return &static_cast<Value<T>*>(m_value.get())->value;
    }
};

template<typename T>
T any_cast(Any const& value)
{
    auto ptr = any_cast<typename std::add_const<typename std::remove_reference<T>::type>::type>(&value);
    if (!ptr)
        throw std::bad_cast();
    return *ptr;
}

template<typename T>
T any_cast(Any& value)
{
    auto ptr = any_cast<typename std::remove_reference<T>::type>(&value);
    if (!ptr)
        throw std::bad_cast();
    return *ptr;
}

template<typename T>
T any_cast(Any&& value)
{
    auto ptr = any_cast<typename std::remove_reference<T>::type>(&value);
    if (!ptr)
        throw std::bad_cast();
    return std::move(*ptr);
}

template<typename T>
T* any_cast(Any* value) noexcept
{
    return value && value->type() == typeid(T) ? value->cast<T>() : nullptr;
}

template<typename T>
const T* any_cast(const Any* value) noexcept
{
    return value && value->type() == typeid(T) ? value->cast<T>() : nullptr;
}
} // namespace util
} // namespace realm

namespace std {
inline void swap(realm::util::Any& lhs, realm::util::Any& rhs) noexcept
{
    lhs.swap(rhs);
}
} // namespace std

#endif // REALM_OS_UTIL_ANY_HPP
