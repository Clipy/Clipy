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

#ifndef REALM_COLUMN_FWD_HPP
#define REALM_COLUMN_FWD_HPP

#include <cstdint>

namespace realm {

class IntegerColumn;
class IntegerColumnIterator;

// Templated classes
template <class T>
class BPlusTree;

namespace util {
template <class>
class Optional;
}

// Shortcuts, aka typedefs.
using DoubleColumn = BPlusTree<double>;
using FloatColumn = BPlusTree<float>;

} // namespace realm

#endif // REALM_COLUMN_FWD_HPP
