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

#ifndef REALM_COLUMN_TYPE_TRAITS_HPP
#define REALM_COLUMN_TYPE_TRAITS_HPP

#include <realm/column_fwd.hpp>
#include <realm/column_type.hpp>
#include <realm/data_type.hpp>

namespace realm {

class OldDateTime;
class ArrayBinary;
class ArrayInteger;
class ArrayIntNull;
template <class>
class BasicArray;

template <class T>
struct ColumnTypeTraits;

template <>
struct ColumnTypeTraits<int64_t> {
    using column_type = Column<int64_t>;
    using leaf_type = ArrayInteger;
    using sum_type = int64_t;
    using minmax_type = int64_t;
    static const DataType id = type_Int;
    static const ColumnType column_id = col_type_Int;
    static const ColumnType real_column_type = col_type_Int;
};

template <>
struct ColumnTypeTraits<util::Optional<int64_t>> {
    using column_type = Column<util::Optional<int64_t>>;
    using leaf_type = ArrayIntNull;
    using sum_type = int64_t;
    using minmax_type = int64_t;
    static const DataType id = type_Int;
    static const ColumnType column_id = col_type_Int;
    static const ColumnType real_column_type = col_type_Int;
};

template <>
struct ColumnTypeTraits<bool> : ColumnTypeTraits<int64_t> {
    static const DataType id = type_Bool;
    static const ColumnType column_id = col_type_Bool;
};

template <>
struct ColumnTypeTraits<util::Optional<bool>> : ColumnTypeTraits<util::Optional<int64_t>> {
    static const DataType id = type_Bool;
    static const ColumnType column_id = col_type_Bool;
};

template <>
struct ColumnTypeTraits<float> {
    using column_type = FloatColumn;
    using leaf_type = BasicArray<float>;
    using sum_type = double;
    using minmax_type = float;
    static const DataType id = type_Float;
    static const ColumnType column_id = col_type_Float;
    static const ColumnType real_column_type = col_type_Float;
};

template <>
struct ColumnTypeTraits<double> {
    using column_type = DoubleColumn;
    using leaf_type = BasicArray<double>;
    using sum_type = double;
    using minmax_type = double;
    static const DataType id = type_Double;
    static const ColumnType column_id = col_type_Double;
    static const ColumnType real_column_type = col_type_Double;
};

template <>
struct ColumnTypeTraits<OldDateTime> : ColumnTypeTraits<int64_t> {
    static const DataType id = type_OldDateTime;
    static const ColumnType column_id = col_type_OldDateTime;
};

template <>
struct ColumnTypeTraits<util::Optional<OldDateTime>> : ColumnTypeTraits<util::Optional<int64_t>> {
    static const DataType id = type_OldDateTime;
    static const ColumnType column_id = col_type_OldDateTime;
};

template <>
struct ColumnTypeTraits<StringData> {
    using column_type = StringEnumColumn;
    using leaf_type = ArrayInteger;
    using sum_type = int64_t;
    static const DataType id = type_String;
    static const ColumnType column_id = col_type_String;
    static const ColumnType real_column_type = col_type_String;
};

template <>
struct ColumnTypeTraits<BinaryData> {
    using column_type = BinaryColumn;
    using leaf_type = ArrayBinary;
    static const DataType id = type_Binary;
    static const ColumnType column_id = col_type_Binary;
    static const ColumnType real_column_type = col_type_Binary;
};

template <DataType, bool Nullable>
struct GetColumnType;
template <>
struct GetColumnType<type_Int, false> {
    using type = IntegerColumn;
};
template <>
struct GetColumnType<type_Int, true> {
    using type = IntNullColumn;
};
template <bool N>
struct GetColumnType<type_Float, N> {
    // FIXME: Null definition
    using type = FloatColumn;
};
template <bool N>
struct GetColumnType<type_Double, N> {
    // FIXME: Null definition
    using type = DoubleColumn;
};

// Only purpose is to return 'double' if and only if source column (T) is float and you're doing a sum (A)
template <class T, Action A>
struct ColumnTypeTraitsSum {
    typedef T sum_type;
};

template <>
struct ColumnTypeTraitsSum<float, act_Sum> {
    typedef double sum_type;
};

template <Action A>
struct ColumnTypeTraitsSum<util::Optional<int64_t>, A> {
    using sum_type = int64_t;
};
}

#endif // REALM_COLUMN_TYPE_TRAITS_HPP
