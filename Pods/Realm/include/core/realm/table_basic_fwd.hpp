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

#ifndef REALM_TABLE_BASIC_FWD_HPP
#define REALM_TABLE_BASIC_FWD_HPP

namespace realm {


template <class Spec>
class BasicTable;

template <class T>
struct IsBasicTable {
    static const bool value = false;
};
template <class Spec>
struct IsBasicTable<BasicTable<Spec>> {
    static const bool value = true;
};
template <class Spec>
struct IsBasicTable<const BasicTable<Spec>> {
    static const bool value = true;
};


} // namespace realm

#endif // REALM_TABLE_BASIC_FWD_HPP
