/*************************************************************************
 *
 * Copyright 2017 Realm Inc.
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

#ifndef REALM_QUERY_OPERATORS_HPP
#define REALM_QUERY_OPERATORS_HPP

#include <realm/binary_data.hpp>
#include <realm/link_view.hpp>
#include <realm/string_data.hpp>
#include <realm/table.hpp>

namespace realm {

// This is not supported in the general case
template <class T>
struct Size;

template <>
struct Size<StringData> {
    int64_t operator()(StringData v) const
    {
        return v.size();
    }
    typedef StringData type;
};

template <>
struct Size<BinaryData> {
    int64_t operator()(BinaryData v) const
    {
        return v.size();
    }
    typedef BinaryData type;
};

template <>
struct Size<ConstTableRef> {
    int64_t operator()(ConstTableRef v) const
    {
        return v->size();
    }
    typedef ConstTableRef type;
};

template <>
struct Size<ConstLinkViewRef> {
    int64_t operator()(ConstLinkViewRef v) const
    {
        return v->size();
    }
    typedef ConstLinkViewRef type;
};

} // namespace realm

#endif // REALM_QUERY_OPERATORS_HPP
