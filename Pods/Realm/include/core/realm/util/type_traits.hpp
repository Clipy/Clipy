/*************************************************************************
 *
 * Copyright 2016 Realm Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 **************************************************************************/

#ifndef REALM_UTIL_TYPE_TRAITS_HPP
#define REALM_UTIL_TYPE_TRAITS_HPP

#include <cstdint>
#include <climits>
#include <cwchar>
#include <limits>
#include <type_traits>

#include <realm/util/features.h>
#include <realm/util/assert.hpp>
#include <realm/util/type_list.hpp>

namespace realm {
namespace util {

template <class From, class To>
struct CopyConst {
private:
    typedef typename std::remove_const<To>::type type_1;

public:
    typedef typename std::conditional<std::is_const<From>::value, const type_1, type_1>::type type;
};


/// Member `type` is the type resulting from integral or
/// floating-point promotion of a value of type `T`.
///
/// \note Enum types are supported only when the compiler supports the
/// C++11 'decltype' feature.
template <class T>
struct Promote;


/// Member `type` is the type of the result of a binary arithmetic (or
/// bitwise) operation (+, -, *, /, %, |, &, ^) when applied to
/// operands of type `A` and `B` respectively. The type of the result
/// of a shift operation (<<, >>) can instead be found as the type
/// resulting from integral promotion of the left operand. The type of
/// the result of a unary arithmetic (or bitwise) operation can be
/// found as the type resulting from integral promotion of the
/// operand.
///
/// \note Enum types are supported only when the compiler supports the
/// C++11 'decltype' feature.
template <class A, class B>
struct ArithBinOpType;


/// Member `type` is `B` if `B` has more value bits than `A`,
/// otherwise is is `A`.
template <class A, class B>
struct ChooseWidestInt;


/// Member `type` is the first of `unsigned char`, `unsigned short`,
/// `unsigned int`, `unsigned long`, and `unsigned long long` that has
/// at least `bits` value bits.
template <int bits>
struct LeastUnsigned;


/// Member `type` is `unsigned` if `unsigned` has at least `bits`
/// value bits, otherwise it is the same as
/// `LeastUnsigned<bits>::%type`.
template <int bits>
struct FastestUnsigned;


// Implementation


template <class T>
struct Promote {
    typedef decltype(+T()) type; // FIXME: This is not performing floating-point promotion.
};


template <class A, class B>
struct ArithBinOpType {
    typedef decltype(A() + B()) type;
};


template <class A, class B>
struct ChooseWidestInt {
private:
    typedef std::numeric_limits<A> lim_a;
    typedef std::numeric_limits<B> lim_b;
    static_assert(lim_a::is_specialized && lim_b::is_specialized,
                  "std::numeric_limits<> must be specialized for both types");
    static_assert(lim_a::is_integer && lim_b::is_integer, "Both types must be integers");

public:
    typedef typename std::conditional<(lim_a::digits >= lim_b::digits), A, B>::type type;
};


template <int bits>
struct LeastUnsigned {
private:
    typedef void types_0;
    typedef TypeAppend<types_0, unsigned char>::type types_1;
    typedef TypeAppend<types_1, unsigned short>::type types_2;
    typedef TypeAppend<types_2, unsigned int>::type types_3;
    typedef TypeAppend<types_3, unsigned long>::type types_4;
    typedef TypeAppend<types_4, unsigned long long>::type types_5;
    typedef types_5 types;
    // The `dummy<>` template is there to work around a bug in
    // VisualStudio (seen in versions 2010 and 2012). Without the
    // `dummy<>` template, The C++ compiler in Visual Studio would
    // attempt to instantiate `FindType<type, pred>` before the
    // instantiation of `LeastUnsigned<>` which obviously fails
    // because `pred` depends on `bits`.
    template <int>
    struct dummy {
        template <class T>
        struct pred {
            static const bool value = std::numeric_limits<T>::digits >= bits;
        };
    };

public:
    typedef typename FindType<types, dummy<bits>::template pred>::type type;
    static_assert(!(std::is_same<type, void>::value), "No unsigned type is that wide");
};


template <int bits>
struct FastestUnsigned {
private:
    typedef typename util::LeastUnsigned<bits>::type least_unsigned;

public:
    typedef typename util::ChooseWidestInt<unsigned, least_unsigned>::type type;
};


} // namespace util
} // namespace realm

#endif // REALM_UTIL_TYPE_TRAITS_HPP
