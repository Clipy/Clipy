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

/*
This file lets you write queries in C++ syntax like: Expression* e = (first + 1 / second >= third + 12.3);

Type conversion/promotion semantics is the same as in the C++ expressions, e.g float + int > double == float +
(float)int > double.


Grammar:
-----------------------------------------------------------------------------------------------------------------------
    Expression:         Subexpr2<T>  Compare<Cond, T>  Subexpr2<T>
                        operator! Expression

    Subexpr2<T>:        Value<T>
                        Columns<T>
                        Subexpr2<T>  Operator<Oper<T>  Subexpr2<T>
                        power(Subexpr2<T>) // power(x) = x * x, as example of unary operator

    Value<T>:           T

    Operator<Oper<T>>:  +, -, *, /

    Compare<Cond, T>:   ==, !=, >=, <=, >, <

    T:                  bool, int, int64_t, float, double, StringData


Class diagram
-----------------------------------------------------------------------------------------------------------------------
Subexpr2
    void evaluate(size_t i, ValueBase* destination)

Compare: public Subexpr2
    size_t find_first(size_t start, size_t end)     // main method that executes query

    unique_ptr<Subexpr2> m_left;                               // left expression subtree
    unique_ptr<Subexpr2> m_right;                              // right expression subtree

Operator: public Subexpr2
    void evaluate(size_t i, ValueBase* destination)
    unique_ptr<Subexpr2> m_left;                               // left expression subtree
    unique_ptr<Subexpr2> m_right;                              // right expression subtree

Value<T>: public Subexpr2
    void evaluate(size_t i, ValueBase* destination)
    T m_v[8];

Columns<T>: public Subexpr2
    void evaluate(size_t i, ValueBase* destination)
    SequentialGetter<T> sg;                         // class bound to a column, lets you read values in a fast way
    Table* m_table;

class ColumnAccessor<>: public Columns<double>


Call diagram:
-----------------------------------------------------------------------------------------------------------------------
Example of 'table.first > 34.6 + table.second':

size_t Compare<Greater>::find_first()-------------+
         |                                        |
         |                                        |
         |                                        |
         +--> Columns<float>::evaluate()          +--------> Operator<Plus>::evaluate()
                                                                |               |
                                               Value<float>::evaluate()    Columns<float>::evaluate()

Operator, Value and Columns have an evaluate(size_t i, ValueBase* destination) method which returns a Value<T>
containing 8 values representing table rows i...i + 7.

So Value<T> contains 8 concecutive values and all operations are based on these chunks. This is
to save overhead by virtual calls needed for evaluating a query that has been dynamically constructed at runtime.


Memory allocation:
-----------------------------------------------------------------------------------------------------------------------
Subexpressions created by the end-user are stack allocated. They are cloned to the heap when passed to UnaryOperator,
Operator, and Compare. Those types own the clones and deallocate them when destroyed.


Caveats, notes and todos
-----------------------------------------------------------------------------------------------------------------------
    * Perhaps disallow columns from two different tables in same expression
    * The name Columns (with s) an be confusing because we also have Column (without s)
    * We have Columns::m_table, Query::m_table and ColumnAccessorBase::m_table that point at the same thing, even with
      ColumnAccessor<> extending Columns. So m_table is redundant, but this is in order to keep class dependencies and
      entanglement low so that the design is flexible (if you perhaps later want a Columns class that is not dependent
      on ColumnAccessor)

Nulls
-----------------------------------------------------------------------------------------------------------------------
First note that at array level, nulls are distinguished between non-null in different ways:
String:
    m_data == 0 && m_size == 0

Integer, Bool, OldDateTime stored in ArrayIntNull:
    value == get(0) (entry 0 determins a magic value that represents nulls)

Float/double:
    null::is_null(value) which tests if value bit-matches one specific bit pattern reserved for null

The Columns class encapsulates all this into a simple class that, for any type T has
    evaluate(size_t index) that reads values from a column, taking nulls in count
    get(index)
    set(index)
    is_null(index)
    set_null(index)
*/

#ifndef REALM_QUERY_EXPRESSION_HPP
#define REALM_QUERY_EXPRESSION_HPP

#include <realm/column_link.hpp>
#include <realm/column_linklist.hpp>
#include <realm/column_table.hpp>
#include <realm/column_type_traits.hpp>
#include <realm/impl/sequential_getter.hpp>
#include <realm/link_view.hpp>
#include <realm/metrics/query_info.hpp>
#include <realm/query_operators.hpp>
#include <realm/util/optional.hpp>
#include <realm/util/serializer.hpp>

#include <numeric>

// Normally, if a next-generation-syntax condition is supported by the old query_engine.hpp, a query_engine node is
// created because it's faster (by a factor of 5 - 10). Because many of our existing next-generation-syntax unit
// unit tests are indeed simple enough to fallback to old query_engine, query_expression gets low test coverage. Undef
// flag to get higher query_expression test coverage. This is a good idea to try out each time you develop on/modify
// query_expression.

#define REALM_OLDQUERY_FALLBACK

namespace realm {

template <class T>
T minimum(T a, T b)
{
    return a < b ? a : b;
}

#ifdef REALM_OLDQUERY_FALLBACK
// Hack to avoid template instantiation errors. See create(). Todo, see if we can simplify only_numeric somehow
namespace _impl {

template <class T, class U>
inline T only_numeric(U in)
{
    return static_cast<T>(util::unwrap(in));
}

template <class T>
inline int only_numeric(const StringData&)
{
    REALM_ASSERT(false);
    return 0;
}

template <class T>
inline int only_numeric(const BinaryData&)
{
    REALM_ASSERT(false);
    return 0;
}

template <class T>
inline StringData only_string_op_types(T in)
{
    REALM_ASSERT(false);
    static_cast<void>(in);
    return StringData();
}

inline BinaryData only_string_op_types(BinaryData in)
{
    return in;
}

template <>
inline StringData only_string_op_types<StringData>(StringData in)
{
    return in;
}

template <class T, class U>
inline T no_timestamp(U in)
{
    return static_cast<T>(util::unwrap(in));
}

template <class T>
inline int no_timestamp(const Timestamp&)
{
    REALM_ASSERT(false);
    return 0;
}

} // namespace _impl

#endif // REALM_OLDQUERY_FALLBACK

template <class T>
struct Plus {
    T operator()(T v1, T v2) const
    {
        return v1 + v2;
    }
    static std::string description()
    {
        return "+";
    }
    typedef T type;
};

template <class T>
struct Minus {
    T operator()(T v1, T v2) const
    {
        return v1 - v2;
    }
    static std::string description()
    {
        return "-";
    }
    typedef T type;
};

template <class T>
struct Div {
    T operator()(T v1, T v2) const
    {
        return v1 / v2;
    }
    static std::string description()
    {
        return "/";
    }
    typedef T type;
};

template <class T>
struct Mul {
    T operator()(T v1, T v2) const
    {
        return v1 * v2;
    }
    static std::string description()
    {
        return "*";
    }
    typedef T type;
};

// Unary operator
template <class T>
struct Pow {
    T operator()(T v) const
    {
        return v * v;
    }
    static std::string description()
    {
        return "^";
    }
    typedef T type;
};

// Finds a common type for T1 and T2 according to C++ conversion/promotion in arithmetic (float + int => float, etc)
template <class T1, class T2, bool T1_is_int = std::numeric_limits<T1>::is_integer || std::is_same<T1, null>::value,
          bool T2_is_int = std::numeric_limits<T2>::is_integer || std::is_same<T2, null>::value,
          bool T1_is_widest = (sizeof(T1) > sizeof(T2) || std::is_same<T2, null>::value)>
struct Common;
template <class T1, class T2, bool b>
struct Common<T1, T2, b, b, true> {
    typedef T1 type;
};
template <class T1, class T2, bool b>
struct Common<T1, T2, b, b, false> {
    typedef T2 type;
};
template <class T1, class T2, bool b>
struct Common<T1, T2, false, true, b> {
    typedef T1 type;
};
template <class T1, class T2, bool b>
struct Common<T1, T2, true, false, b> {
    typedef T2 type;
};


struct RowIndex {
    enum DetachedTag {
        Detached,
    };

    explicit RowIndex()
        : m_row_index(npos)
    {
    }
    explicit RowIndex(size_t row_index)
        : m_row_index(row_index)
    {
    }
    RowIndex(DetachedTag)
        : m_row_index()
    {
    }

    bool is_attached() const
    {
        return bool(m_row_index);
    }
    bool is_null() const
    {
        return is_attached() && *m_row_index == npos;
    }

    bool operator==(const RowIndex& other) const
    {
        // Row indexes that are detached are never equal to any other row index.
        if (!is_attached() || !other.is_attached())
            return false;
        return m_row_index == other.m_row_index;
    }
    bool operator!=(const RowIndex& other) const
    {
        return !(*this == other);
    }
private:
    util::Optional<size_t> m_row_index;
};

struct ValueBase {
    static const size_t default_size = 8;
    virtual void export_bool(ValueBase& destination) const = 0;
    virtual void export_Timestamp(ValueBase& destination) const = 0;
    virtual void export_int(ValueBase& destination) const = 0;
    virtual void export_float(ValueBase& destination) const = 0;
    virtual void export_int64_t(ValueBase& destination) const = 0;
    virtual void export_double(ValueBase& destination) const = 0;
    virtual void export_StringData(ValueBase& destination) const = 0;
    virtual void export_BinaryData(ValueBase& destination) const = 0;
    virtual void export_RowIndex(ValueBase& destination) const = 0;
    virtual void export_null(ValueBase& destination) const = 0;
    virtual void import(const ValueBase& destination) = 0;

    // If true, all values in the class come from a link list of a single field in the parent table (m_table). If
    // false, then values come from successive rows of m_table (query operations are operated on in bulks for speed)
    bool m_from_link_list;

    // Number of values stored in the class.
    size_t m_values;
};

class Expression {
public:
    Expression()
    {
    }
    virtual ~Expression()
    {
    }

    virtual size_t find_first(size_t start, size_t end) const = 0;
    virtual void set_base_table(const Table* table) = 0;
    virtual void verify_column() const = 0;
    virtual const Table* get_base_table() const = 0;
    virtual std::string description(util::serializer::SerialisationState& state) const = 0;

    virtual std::unique_ptr<Expression> clone(QueryNodeHandoverPatches*) const = 0;
    virtual void apply_handover_patch(QueryNodeHandoverPatches&, Group&)
    {
    }
};

template <typename T, typename... Args>
std::unique_ptr<Expression> make_expression(Args&&... args)
{
    return std::unique_ptr<Expression>(new T(std::forward<Args>(args)...));
}

class Subexpr {
public:
    virtual ~Subexpr()
    {
    }

    virtual std::unique_ptr<Subexpr> clone(QueryNodeHandoverPatches* = nullptr) const = 0;
    virtual void apply_handover_patch(QueryNodeHandoverPatches&, Group&)
    {
    }

    // When the user constructs a query, it always "belongs" to one single base/parent table (regardless of
    // any links or not and regardless of any queries assembled with || or &&). When you do a Query::find(),
    // then Query::m_table is set to this table, and set_base_table() is called on all Columns and LinkMaps in
    // the query expression tree so that they can set/update their internals as required.
    //
    // During thread-handover of a Query, set_base_table() is also called to make objects point at the new table
    // instead of the old one from the old thread.
    virtual void set_base_table(const Table*)
    {
    }

    virtual void verify_column() const = 0;
    virtual std::string description(util::serializer::SerialisationState& state) const = 0;

    // Recursively fetch tables of columns in expression tree. Used when user first builds a stand-alone expression
    // and
    // binds it to a Query at a later time
    virtual const Table* get_base_table() const
    {
        return nullptr;
    }

    virtual void evaluate(size_t index, ValueBase& destination) = 0;
};

template <typename T, typename... Args>
std::unique_ptr<Subexpr> make_subexpr(Args&&... args)
{
    return std::unique_ptr<Subexpr>(new T(std::forward<Args>(args)...));
}

template <class T>
class Columns;
template <class T>
class Value;
class ConstantStringValue;
template <class T>
class Subexpr2;
template <class oper, class TLeft = Subexpr, class TRight = Subexpr>
class Operator;
template <class oper, class TLeft = Subexpr>
class UnaryOperator;
template <class oper, class TLeft = Subexpr>
class SizeOperator;
template <class TCond, class T, class TLeft = Subexpr, class TRight = Subexpr>
class Compare;
template <bool has_links>
class UnaryLinkCompare;
class ColumnAccessorBase;


// Handle cases where left side is a constant (int, float, int64_t, double, StringData)
template <class Cond, class L, class R>
Query create(L left, const Subexpr2<R>& right)
{
// Purpose of below code is to intercept the creation of a condition and test if it's supported by the old
// query_engine.hpp which is faster. If it's supported, create a query_engine.hpp node, otherwise create a
// query_expression.hpp node.
//
// This method intercepts only Value <cond> Subexpr2. Interception of Subexpr2 <cond> Subexpr is elsewhere.

#ifdef REALM_OLDQUERY_FALLBACK // if not defined, then never fallback to query_engine.hpp; always use query_expression
    const Columns<R>* column = dynamic_cast<const Columns<R>*>(&right);
    // TODO: recognize size operator expressions
    // auto size_operator = dynamic_cast<const SizeOperator<Size<StringData>, Subexpr>*>(&right);

    if (column && ((std::numeric_limits<L>::is_integer && std::numeric_limits<R>::is_integer) ||
                   (std::is_same<L, double>::value && std::is_same<R, double>::value) ||
                   (std::is_same<L, float>::value && std::is_same<R, float>::value) ||
                   (std::is_same<L, Timestamp>::value && std::is_same<R, Timestamp>::value) ||
                   (std::is_same<L, StringData>::value && std::is_same<R, StringData>::value) ||
                   (std::is_same<L, BinaryData>::value && std::is_same<R, BinaryData>::value)) &&
        !column->links_exist()) {
        const Table* t = column->get_base_table();
        Query q = Query(*t);

        if (std::is_same<Cond, Less>::value)
            q.greater(column->column_ndx(), _impl::only_numeric<R>(left));
        else if (std::is_same<Cond, Greater>::value)
            q.less(column->column_ndx(), _impl::only_numeric<R>(left));
        else if (std::is_same<Cond, Equal>::value)
            q.equal(column->column_ndx(), left);
        else if (std::is_same<Cond, NotEqual>::value)
            q.not_equal(column->column_ndx(), left);
        else if (std::is_same<Cond, LessEqual>::value)
            q.greater_equal(column->column_ndx(), _impl::only_numeric<R>(left));
        else if (std::is_same<Cond, GreaterEqual>::value)
            q.less_equal(column->column_ndx(), _impl::only_numeric<R>(left));
        else if (std::is_same<Cond, EqualIns>::value)
            q.equal(column->column_ndx(), _impl::only_string_op_types(left), false);
        else if (std::is_same<Cond, NotEqualIns>::value)
            q.not_equal(column->column_ndx(), _impl::only_string_op_types(left), false);
        else if (std::is_same<Cond, BeginsWith>::value)
            q.begins_with(column->column_ndx(), _impl::only_string_op_types(left));
        else if (std::is_same<Cond, BeginsWithIns>::value)
            q.begins_with(column->column_ndx(), _impl::only_string_op_types(left), false);
        else if (std::is_same<Cond, EndsWith>::value)
            q.ends_with(column->column_ndx(), _impl::only_string_op_types(left));
        else if (std::is_same<Cond, EndsWithIns>::value)
            q.ends_with(column->column_ndx(), _impl::only_string_op_types(left), false);
        else if (std::is_same<Cond, Contains>::value)
            q.contains(column->column_ndx(), _impl::only_string_op_types(left));
        else if (std::is_same<Cond, ContainsIns>::value)
            q.contains(column->column_ndx(), _impl::only_string_op_types(left), false);
        else if (std::is_same<Cond, Like>::value)
            q.like(column->column_ndx(), _impl::only_string_op_types(left));
        else if (std::is_same<Cond, LikeIns>::value)
            q.like(column->column_ndx(), _impl::only_string_op_types(left), false);
        else {
            // query_engine.hpp does not support this Cond. Please either add support for it in query_engine.hpp or
            // fallback to using use 'return new Compare<>' instead.
            REALM_ASSERT(false);
        }
        // Return query_engine.hpp node
        return q;
    }
    else
#endif
    {
        // Return query_expression.hpp node
        using CommonType = typename Common<L, R>::type;
        using ValueType =
            typename std::conditional<std::is_same<L, StringData>::value, ConstantStringValue, Value<L>>::type;
        return make_expression<Compare<Cond, CommonType>>(make_subexpr<ValueType>(left), right.clone());
    }
}


// All overloads where left-hand-side is Subexpr2<L>:
//
// left-hand-side       operator                              right-hand-side
// Subexpr2<L>          +, -, *, /, <, >, ==, !=, <=, >=      R, Subexpr2<R>
//
// For L = R = {int, int64_t, float, double, StringData, Timestamp}:
template <class L, class R>
class Overloads {
    typedef typename Common<L, R>::type CommonType;

    std::unique_ptr<Subexpr> clone_subexpr() const
    {
        return static_cast<const Subexpr2<L>&>(*this).clone();
    }

public:
    // Arithmetic, right side constant
    Operator<Plus<CommonType>> operator+(R right) const
    {
        return {clone_subexpr(), make_subexpr<Value<R>>(right)};
    }
    Operator<Minus<CommonType>> operator-(R right) const
    {
        return {clone_subexpr(), make_subexpr<Value<R>>(right)};
    }
    Operator<Mul<CommonType>> operator*(R right) const
    {
        return {clone_subexpr(), make_subexpr<Value<R>>(right)};
    }
    Operator<Div<CommonType>> operator/(R right) const
    {
        return {clone_subexpr(), make_subexpr<Value<R>>(right)};
    }

    // Arithmetic, right side subexpression
    Operator<Plus<CommonType>> operator+(const Subexpr2<R>& right) const
    {
        return {clone_subexpr(), right.clone()};
    }
    Operator<Minus<CommonType>> operator-(const Subexpr2<R>& right) const
    {
        return {clone_subexpr(), right.clone()};
    }
    Operator<Mul<CommonType>> operator*(const Subexpr2<R>& right) const
    {
        return {clone_subexpr(), right.clone()};
    }
    Operator<Div<CommonType>> operator/(const Subexpr2<R>& right) const
    {
        return {clone_subexpr(), right.clone()};
    }

    // Compare, right side constant
    Query operator>(R right)
    {
        return create<Less>(right, static_cast<Subexpr2<L>&>(*this));
    }
    Query operator<(R right)
    {
        return create<Greater>(right, static_cast<Subexpr2<L>&>(*this));
    }
    Query operator>=(R right)
    {
        return create<LessEqual>(right, static_cast<Subexpr2<L>&>(*this));
    }
    Query operator<=(R right)
    {
        return create<GreaterEqual>(right, static_cast<Subexpr2<L>&>(*this));
    }
    Query operator==(R right)
    {
        return create<Equal>(right, static_cast<Subexpr2<L>&>(*this));
    }
    Query operator!=(R right)
    {
        return create<NotEqual>(right, static_cast<Subexpr2<L>&>(*this));
    }

    // Purpose of this method is to intercept the creation of a condition and test if it's supported by the old
    // query_engine.hpp which is faster. If it's supported, create a query_engine.hpp node, otherwise create a
    // query_expression.hpp node.
    //
    // This method intercepts Subexpr2 <cond> Subexpr2 only. Value <cond> Subexpr2 is intercepted elsewhere.
    template <class Cond>
    Query create2(const Subexpr2<R>& right)
    {
#ifdef REALM_OLDQUERY_FALLBACK // if not defined, never fallback query_engine; always use query_expression
        // Test if expressions are of type Columns. Other possibilities are Value and Operator.
        const Columns<R>* left_col = dynamic_cast<const Columns<R>*>(static_cast<Subexpr2<L>*>(this));
        const Columns<R>* right_col = dynamic_cast<const Columns<R>*>(&right);

        // query_engine supports 'T-column <op> <T-column>' for T = {int64_t, float, double}, op = {<, >, ==, !=, <=,
        // >=},
        // but only if both columns are non-nullable, and aren't in linked tables.
        if (left_col && right_col && std::is_same<L, R>::value && !left_col->is_nullable() &&
            !right_col->is_nullable() && !left_col->links_exist() && !right_col->links_exist() &&
            !std::is_same<L, Timestamp>::value) {
            const Table* t = left_col->get_base_table();
            Query q = Query(*t);

            if (std::numeric_limits<L>::is_integer || std::is_same<L, OldDateTime>::value) {
                if (std::is_same<Cond, Less>::value)
                    q.less_int(left_col->column_ndx(), right_col->column_ndx());
                else if (std::is_same<Cond, Greater>::value)
                    q.greater_int(left_col->column_ndx(), right_col->column_ndx());
                else if (std::is_same<Cond, Equal>::value)
                    q.equal_int(left_col->column_ndx(), right_col->column_ndx());
                else if (std::is_same<Cond, NotEqual>::value)
                    q.not_equal_int(left_col->column_ndx(), right_col->column_ndx());
                else if (std::is_same<Cond, LessEqual>::value)
                    q.less_equal_int(left_col->column_ndx(), right_col->column_ndx());
                else if (std::is_same<Cond, GreaterEqual>::value)
                    q.greater_equal_int(left_col->column_ndx(), right_col->column_ndx());
                else {
                    REALM_ASSERT(false);
                }
            }
            else if (std::is_same<L, float>::value) {
                if (std::is_same<Cond, Less>::value)
                    q.less_float(left_col->column_ndx(), right_col->column_ndx());
                else if (std::is_same<Cond, Greater>::value)
                    q.greater_float(left_col->column_ndx(), right_col->column_ndx());
                else if (std::is_same<Cond, Equal>::value)
                    q.equal_float(left_col->column_ndx(), right_col->column_ndx());
                else if (std::is_same<Cond, NotEqual>::value)
                    q.not_equal_float(left_col->column_ndx(), right_col->column_ndx());
                else if (std::is_same<Cond, LessEqual>::value)
                    q.less_equal_float(left_col->column_ndx(), right_col->column_ndx());
                else if (std::is_same<Cond, GreaterEqual>::value)
                    q.greater_equal_float(left_col->column_ndx(), right_col->column_ndx());
                else {
                    REALM_ASSERT(false);
                }
            }
            else if (std::is_same<L, double>::value) {
                if (std::is_same<Cond, Less>::value)
                    q.less_double(left_col->column_ndx(), right_col->column_ndx());
                else if (std::is_same<Cond, Greater>::value)
                    q.greater_double(left_col->column_ndx(), right_col->column_ndx());
                else if (std::is_same<Cond, Equal>::value)
                    q.equal_double(left_col->column_ndx(), right_col->column_ndx());
                else if (std::is_same<Cond, NotEqual>::value)
                    q.not_equal_double(left_col->column_ndx(), right_col->column_ndx());
                else if (std::is_same<Cond, LessEqual>::value)
                    q.less_equal_double(left_col->column_ndx(), right_col->column_ndx());
                else if (std::is_same<Cond, GreaterEqual>::value)
                    q.greater_equal_double(left_col->column_ndx(), right_col->column_ndx());
                else {
                    REALM_ASSERT(false);
                }
            }
            else {
                REALM_ASSERT(false);
            }
            // Return query_engine.hpp node
            return q;
        }
        else
#endif
        {
            // Return query_expression.hpp node
            return make_expression<Compare<Cond, typename Common<L, R>::type>>(clone_subexpr(), right.clone());
        }
    }

    // Compare, right side subexpression
    Query operator==(const Subexpr2<R>& right)
    {
        return create2<Equal>(right);
    }
    Query operator!=(const Subexpr2<R>& right)
    {
        return create2<NotEqual>(right);
    }
    Query operator>(const Subexpr2<R>& right)
    {
        return create2<Greater>(right);
    }
    Query operator<(const Subexpr2<R>& right)
    {
        return create2<Less>(right);
    }
    Query operator>=(const Subexpr2<R>& right)
    {
        return create2<GreaterEqual>(right);
    }
    Query operator<=(const Subexpr2<R>& right)
    {
        return create2<LessEqual>(right);
    }
};

// With this wrapper class we can define just 20 overloads inside Overloads<L, R> instead of 5 * 20 = 100. Todo: We
// can
// consider if it's simpler/better to remove this class completely and just list all 100 overloads manually anyway.
template <class T>
class Subexpr2 : public Subexpr,
                 public Overloads<T, const char*>,
                 public Overloads<T, int>,
                 public Overloads<T, float>,
                 public Overloads<T, double>,
                 public Overloads<T, int64_t>,
                 public Overloads<T, StringData>,
                 public Overloads<T, bool>,
                 public Overloads<T, Timestamp>,
                 public Overloads<T, OldDateTime>,
                 public Overloads<T, null> {
public:
    virtual ~Subexpr2()
    {
    }

#define RLM_U2(t, o) using Overloads<T, t>::operator o;
#define RLM_U(o)                                                                                                     \
    RLM_U2(int, o)                                                                                                   \
    RLM_U2(float, o)                                                                                                 \
    RLM_U2(double, o)                                                                                                \
    RLM_U2(int64_t, o)                                                                                               \
    RLM_U2(StringData, o) RLM_U2(bool, o) RLM_U2(OldDateTime, o) RLM_U2(Timestamp, o) RLM_U2(null, o)
    RLM_U(+) RLM_U(-) RLM_U(*) RLM_U(/) RLM_U(>) RLM_U(<) RLM_U(==) RLM_U(!=) RLM_U(>=) RLM_U(<=)
};

// Subexpr2<Link> only provides equality comparisons. Their implementations can be found later in this file.
template <>
class Subexpr2<Link> : public Subexpr {
};

template <>
class Subexpr2<StringData> : public Subexpr, public Overloads<StringData, StringData> {
public:
    Query equal(StringData sd, bool case_sensitive = true);
    Query equal(const Subexpr2<StringData>& col, bool case_sensitive = true);
    Query not_equal(StringData sd, bool case_sensitive = true);
    Query not_equal(const Subexpr2<StringData>& col, bool case_sensitive = true);
    Query begins_with(StringData sd, bool case_sensitive = true);
    Query begins_with(const Subexpr2<StringData>& col, bool case_sensitive = true);
    Query ends_with(StringData sd, bool case_sensitive = true);
    Query ends_with(const Subexpr2<StringData>& col, bool case_sensitive = true);
    Query contains(StringData sd, bool case_sensitive = true);
    Query contains(const Subexpr2<StringData>& col, bool case_sensitive = true);
    Query like(StringData sd, bool case_sensitive = true);
    Query like(const Subexpr2<StringData>& col, bool case_sensitive = true);
};

template <>
class Subexpr2<BinaryData> : public Subexpr, public Overloads<BinaryData, BinaryData> {
public:
    Query equal(BinaryData sd, bool case_sensitive = true);
    Query equal(const Subexpr2<BinaryData>& col, bool case_sensitive = true);
    Query not_equal(BinaryData sd, bool case_sensitive = true);
    Query not_equal(const Subexpr2<BinaryData>& col, bool case_sensitive = true);
    Query begins_with(BinaryData sd, bool case_sensitive = true);
    Query begins_with(const Subexpr2<BinaryData>& col, bool case_sensitive = true);
    Query ends_with(BinaryData sd, bool case_sensitive = true);
    Query ends_with(const Subexpr2<BinaryData>& col, bool case_sensitive = true);
    Query contains(BinaryData sd, bool case_sensitive = true);
    Query contains(const Subexpr2<BinaryData>& col, bool case_sensitive = true);
    Query like(BinaryData sd, bool case_sensitive = true);
    Query like(const Subexpr2<BinaryData>& col, bool case_sensitive = true);
};


/*
This class is used to store N values of type T = {int64_t, bool, OldDateTime or StringData}, and allows an entry
to be null too. It's used by the Value class for internal storage.

To indicate nulls, we could have chosen a separate bool vector or some other bitmask construction. But for
performance, we customize indication of nulls to match the same indication that is used in the persisted database
file

Queries in query_expression.hpp execute by processing chunks of 8 rows at a time. Assume you have a column:

    price (int) = {1, 2, 3, null, 1, 6, 6, 9, 5, 2, null}

And perform a query:

    Query q = (price + 2 == 5);

query_expression.hpp will then create a NullableVector<int> = {5, 5, 5, 5, 5, 5, 5, 5} and then read values
NullableVector<int> = {1, 2, 3, null, 1, 6, 6, 9} from the column, and then perform `+` and `==` on these chunks.

See the top of this file for more information on all this.

Assume the user specifies the null constant in a query:

Query q = (price == null)

The query system will then construct a NullableVector of type `null` (NullableVector<null>). This allows compile
time optimizations for these cases.
*/

template <class T, size_t prealloc = 8>
struct NullableVector {
    using Underlying = typename util::RemoveOptional<T>::type;
    using t_storage =
        typename std::conditional<std::is_same<Underlying, bool>::value || std::is_same<Underlying, int>::value,
                                  int64_t, Underlying>::type;

    NullableVector()
    {
    }

    NullableVector& operator=(const NullableVector& other)
    {
        if (this != &other) {
            init(other.m_size);
            realm::safe_copy_n(other.m_first, other.m_size, m_first);
            m_null = other.m_null;
        }
        return *this;
    }

    NullableVector(const NullableVector& other)
    {
        init(other.m_size);
        realm::safe_copy_n(other.m_first, other.m_size, m_first);
        m_null = other.m_null;
    }

    ~NullableVector()
    {
        dealloc();
    }

    T operator[](size_t index) const
    {
        REALM_ASSERT_3(index, <, m_size);
        return static_cast<T>(m_first[index]);
    }

    inline bool is_null(size_t index) const
    {
        REALM_ASSERT((std::is_same<t_storage, int64_t>::value));
        return m_first[index] == m_null;
    }

    inline void set_null(size_t index)
    {
        REALM_ASSERT((std::is_same<t_storage, int64_t>::value));
        m_first[index] = m_null;
    }

    template <typename Type = t_storage>
    typename std::enable_if<std::is_same<Type, int64_t>::value, void>::type set(size_t index, t_storage value)
    {
        REALM_ASSERT((std::is_same<t_storage, int64_t>::value));

        // If value collides with magic null value, then switch to a new unique representation for null
        if (REALM_UNLIKELY(value == m_null)) {
            // adding a prime will generate 2^64 unique values. Todo: Only works on 2's complement architecture
            uint64_t candidate = static_cast<uint64_t>(m_null) + 0xfffffffbULL;
            while (std::find(m_first, m_first + m_size, static_cast<int64_t>(candidate)) != m_first + m_size)
                candidate += 0xfffffffbULL;
            std::replace(m_first, m_first + m_size, m_null, static_cast<int64_t>(candidate));
        }
        m_first[index] = value;
    }

    template <typename Type = T>
    typename std::enable_if<realm::is_any<Type, float, double, OldDateTime, BinaryData, StringData, RowIndex,
                                          Timestamp, ConstTableRef, null>::value,
                            void>::type
    set(size_t index, t_storage value)
    {
        m_first[index] = value;
    }

    inline util::Optional<T> get(size_t index) const
    {
        if (is_null(index))
            return util::none;

        return util::make_optional((*this)[index]);
    }

    inline void set(size_t index, util::Optional<Underlying> value)
    {
        if (value) {
            Underlying v = *value;
            set(index, v);
        }
        else {
            set_null(index);
        }
    }

    void fill(T value)
    {
        for (size_t t = 0; t < m_size; t++) {
            if (std::is_same<T, null>::value)
                set_null(t);
            else
                set(t, value);
        }
    }

    void init(size_t size)
    {
        if (size == m_size)
            return;

        dealloc();
        m_size = size;
        if (m_size > 0) {
            if (m_size > prealloc)
                m_first = reinterpret_cast<t_storage*>(new t_storage[m_size]);
            else
                m_first = m_cache;
        }
    }

    void init(size_t size, T values)
    {
        init(size);
        fill(values);
    }

    void dealloc()
    {
        if (m_first) {
            if (m_size > prealloc)
                delete[] m_first;
            m_first = nullptr;
        }
    }

    t_storage m_cache[prealloc];
    t_storage* m_first = &m_cache[0];
    size_t m_size = 0;

    int64_t m_null = reinterpret_cast<int64_t>(&m_null); // choose magic value to represent nulls
};

// Double
// NOTE: fails in gcc 4.8 without `inline`. Do not remove. Same applies for all methods below.
template <>
inline bool NullableVector<double>::is_null(size_t index) const
{
    return null::is_null_float(m_first[index]);
}

template <>
inline void NullableVector<double>::set_null(size_t index)
{
    m_first[index] = null::get_null_float<double>();
}

// Float
template <>
inline bool NullableVector<float>::is_null(size_t index) const
{
    return null::is_null_float(m_first[index]);
}

template <>
inline void NullableVector<float>::set_null(size_t index)
{
    m_first[index] = null::get_null_float<float>();
}


// Null
template <>
inline void NullableVector<null>::set_null(size_t)
{
    return;
}
template <>
inline bool NullableVector<null>::is_null(size_t) const
{
    return true;
}


// OldDateTime
template <>
inline bool NullableVector<OldDateTime>::is_null(size_t index) const
{
    return m_first[index].get_olddatetime() == m_null;
}


template <>
inline void NullableVector<OldDateTime>::set_null(size_t index)
{
    m_first[index] = m_null;
}

// StringData

template <>
inline bool NullableVector<StringData>::is_null(size_t index) const
{
    return m_first[index].is_null();
}

template <>
inline void NullableVector<StringData>::set_null(size_t index)
{
    m_first[index] = StringData();
}

// BinaryData

template <>
inline bool NullableVector<BinaryData>::is_null(size_t index) const
{
    return m_first[index].is_null();
}

template <>
inline void NullableVector<BinaryData>::set_null(size_t index)
{
    m_first[index] = BinaryData();
}

// RowIndex
template <>
inline bool NullableVector<RowIndex>::is_null(size_t index) const
{
    return m_first[index].is_null();
}
template <>
inline void NullableVector<RowIndex>::set_null(size_t index)
{
    m_first[index] = RowIndex();
}


// Timestamp

template <>
inline bool NullableVector<Timestamp>::is_null(size_t index) const
{
    return m_first[index].is_null();
}

template <>
inline void NullableVector<Timestamp>::set_null(size_t index)
{
    m_first[index] = Timestamp{};
}

// ConstTableRef
template <>
inline bool NullableVector<ConstTableRef>::is_null(size_t index) const
{
    return !bool(m_first[index]);
}
template <>
inline void NullableVector<ConstTableRef>::set_null(size_t index)
{
    m_first[index].reset();
}

template <typename Operator>
struct OperatorOptionalAdapter {
    template <typename L, typename R>
    util::Optional<typename Operator::type> operator()(const util::Optional<L>& left, const util::Optional<R>& right)
    {
        if (!left || !right)
            return util::none;
        return Operator()(*left, *right);
    }

    template <typename T>
    util::Optional<typename Operator::type> operator()(const util::Optional<T>& arg)
    {
        if (!arg)
            return util::none;
        return Operator()(*arg);
    }
};


struct TrueExpression : Expression {
    size_t find_first(size_t start, size_t end) const override
    {
        REALM_ASSERT(start <= end);
        if (start != end)
            return start;

        return realm::not_found;
    }
    void set_base_table(const Table*) override
    {
    }
    const Table* get_base_table() const override
    {
        return nullptr;
    }
    void verify_column() const override
    {
    }
    std::string description(util::serializer::SerialisationState&) const override
    {
        return "TRUEPREDICATE";
    }
    std::unique_ptr<Expression> clone(QueryNodeHandoverPatches*) const override
    {
        return std::unique_ptr<Expression>(new TrueExpression(*this));
    }
};


struct FalseExpression : Expression {
    size_t find_first(size_t, size_t) const override
    {
        return realm::not_found;
    }
    void set_base_table(const Table*) override
    {
    }
    void verify_column() const override
    {
    }
    std::string description(util::serializer::SerialisationState&) const override
    {
        return "FALSEPREDICATE";
    }
    const Table* get_base_table() const override
    {
        return nullptr;
    }
    std::unique_ptr<Expression> clone(QueryNodeHandoverPatches*) const override
    {
        return std::unique_ptr<Expression>(new FalseExpression(*this));
    }
};


// Stores N values of type T. Can also exchange data with other ValueBase of different types
template <class T>
class Value : public ValueBase, public Subexpr2<T> {
public:
    Value()
    {
        init(false, ValueBase::default_size, T());
    }
    Value(T v)
    {
        init(false, ValueBase::default_size, v);
    }

    Value(bool from_link_list, size_t values)
    {
        init(from_link_list, values, T());
    }

    Value(bool from_link_list, size_t values, T v)
    {
        init(from_link_list, values, v);
    }

    Value(const Value&) = default;
    Value& operator=(const Value&) = default;

    void init(bool from_link_list, size_t values, T v)
    {
        m_storage.init(values, v);
        ValueBase::m_from_link_list = from_link_list;
        ValueBase::m_values = values;
    }

    void init(bool from_link_list, size_t values)
    {
        m_storage.init(values);
        ValueBase::m_from_link_list = from_link_list;
        ValueBase::m_values = values;
    }

    void verify_column() const override
    {
    }

    virtual std::string description(util::serializer::SerialisationState&) const override
    {
        if (ValueBase::m_from_link_list) {
            return util::serializer::print_value(util::to_string(ValueBase::m_values)
                                        + (ValueBase::m_values == 1 ? " value" : " values"));
        }
        if (m_storage.m_size > 0) {
            return util::serializer::print_value(m_storage[0]);
        }
        return "";
    }

    void evaluate(size_t, ValueBase& destination) override
    {
        destination.import(*this);
    }


    template <class TOperator>
    REALM_FORCEINLINE void fun(const Value* left, const Value* right)
    {
        OperatorOptionalAdapter<TOperator> o;

        if (!left->m_from_link_list && !right->m_from_link_list) {
            // Operate on values one-by-one (one value is one row; no links)
            size_t min = std::min(left->m_values, right->m_values);
            init(false, min);

            for (size_t i = 0; i < min; i++) {
                m_storage.set(i, o(left->m_storage.get(i), right->m_storage.get(i)));
            }
        }
        else if (left->m_from_link_list && right->m_from_link_list) {
            // FIXME: Many-to-many links not supported yet. Need to specify behaviour
            REALM_ASSERT_DEBUG(false);
        }
        else if (!left->m_from_link_list && right->m_from_link_list) {
            // Right values come from link. Left must come from single row.
            REALM_ASSERT_DEBUG(left->m_values > 0);
            init(true, right->m_values);

            auto left_value = left->m_storage.get(0);
            for (size_t i = 0; i < right->m_values; i++) {
                m_storage.set(i, o(left_value, right->m_storage.get(i)));
            }
        }
        else if (left->m_from_link_list && !right->m_from_link_list) {
            // Same as above, but with left values coming from links
            REALM_ASSERT_DEBUG(right->m_values > 0);
            init(true, left->m_values);

            auto right_value = right->m_storage.get(0);
            for (size_t i = 0; i < left->m_values; i++) {
                m_storage.set(i, o(left->m_storage.get(i), right_value));
            }
        }
    }

    template <class TOperator>
    REALM_FORCEINLINE void fun(const Value* value)
    {
        init(value->m_from_link_list, value->m_values);

        OperatorOptionalAdapter<TOperator> o;
        for (size_t i = 0; i < value->m_values; i++) {
            m_storage.set(i, o(value->m_storage.get(i)));
        }
    }


    // Below import and export methods are for type conversion between float, double, int64_t, etc.
    template <class D>
    typename std::enable_if<std::is_convertible<T, D>::value>::type REALM_FORCEINLINE
    export2(ValueBase& destination) const
    {
        Value<D>& d = static_cast<Value<D>&>(destination);
        d.init(ValueBase::m_from_link_list, ValueBase::m_values, D());
        for (size_t t = 0; t < ValueBase::m_values; t++) {
            if (m_storage.is_null(t))
                d.m_storage.set_null(t);
            else {
                d.m_storage.set(t, static_cast<D>(m_storage[t]));
            }
        }
    }

    template <class D>
    typename std::enable_if<!std::is_convertible<T, D>::value>::type REALM_FORCEINLINE export2(ValueBase&) const
    {
        // export2 is instantiated for impossible conversions like T=StringData, D=int64_t. These are never
        // performed at runtime but would result in a compiler error if we did not provide this implementation.
        REALM_ASSERT_DEBUG(false);
    }

    REALM_FORCEINLINE void export_Timestamp(ValueBase& destination) const override
    {
        export2<Timestamp>(destination);
    }

    REALM_FORCEINLINE void export_bool(ValueBase& destination) const override
    {
        export2<bool>(destination);
    }

    REALM_FORCEINLINE void export_int64_t(ValueBase& destination) const override
    {
        export2<int64_t>(destination);
    }

    REALM_FORCEINLINE void export_float(ValueBase& destination) const override
    {
        export2<float>(destination);
    }

    REALM_FORCEINLINE void export_int(ValueBase& destination) const override
    {
        export2<int>(destination);
    }

    REALM_FORCEINLINE void export_double(ValueBase& destination) const override
    {
        export2<double>(destination);
    }
    REALM_FORCEINLINE void export_StringData(ValueBase& destination) const override
    {
        export2<StringData>(destination);
    }
    REALM_FORCEINLINE void export_BinaryData(ValueBase& destination) const override
    {
        export2<BinaryData>(destination);
    }
    REALM_FORCEINLINE void export_RowIndex(ValueBase& destination) const override
    {
        export2<RowIndex>(destination);
    }
    REALM_FORCEINLINE void export_null(ValueBase& destination) const override
    {
        Value<null>& d = static_cast<Value<null>&>(destination);
        d.init(m_from_link_list, m_values);
    }

    REALM_FORCEINLINE void import(const ValueBase& source) override
    {
        if (std::is_same<T, int>::value)
            source.export_int(*this);
        else if (std::is_same<T, Timestamp>::value)
            source.export_Timestamp(*this);
        else if (std::is_same<T, bool>::value)
            source.export_bool(*this);
        else if (std::is_same<T, float>::value)
            source.export_float(*this);
        else if (std::is_same<T, double>::value)
            source.export_double(*this);
        else if (std::is_same<T, int64_t>::value || std::is_same<T, bool>::value ||
                 std::is_same<T, OldDateTime>::value)
            source.export_int64_t(*this);
        else if (std::is_same<T, StringData>::value)
            source.export_StringData(*this);
        else if (std::is_same<T, BinaryData>::value)
            source.export_BinaryData(*this);
        else if (std::is_same<T, RowIndex>::value)
            source.export_RowIndex(*this);
        else if (std::is_same<T, null>::value)
            source.export_null(*this);
        else
            REALM_ASSERT_DEBUG(false);
    }

    // Given a TCond (==, !=, >, <, >=, <=) and two Value<T>, return index of first match
    template <class TCond>
    REALM_FORCEINLINE static size_t compare(Value<T>* left, Value<T>* right)
    {
        TCond c;

        if (!left->m_from_link_list && !right->m_from_link_list) {
            // Compare values one-by-one (one value is one row; no link lists)
            size_t min = minimum(left->ValueBase::m_values, right->ValueBase::m_values);
            for (size_t m = 0; m < min; m++) {

                if (c(left->m_storage[m], right->m_storage[m], left->m_storage.is_null(m),
                      right->m_storage.is_null(m)))
                    return m;
            }
        }
        else if (left->m_from_link_list && right->m_from_link_list) {
            // FIXME: Many-to-many links not supported yet. Need to specify behaviour
            REALM_ASSERT_DEBUG(false);
        }
        else if (!left->m_from_link_list && right->m_from_link_list) {
            // Right values come from link list. Left must come from single row. Semantics: Match if at least 1
            // linked-to-value fulfills the condition
            REALM_ASSERT_DEBUG(left->m_values > 0);
            for (size_t r = 0; r < right->m_values; r++) {
                if (c(left->m_storage[0], right->m_storage[r], left->m_storage.is_null(0),
                      right->m_storage.is_null(r)))
                    return 0;
            }
        }
        else if (left->m_from_link_list && !right->m_from_link_list) {
            // Same as above, but with left values coming from link list.
            REALM_ASSERT_DEBUG(right->m_values > 0);
            for (size_t l = 0; l < left->m_values; l++) {
                if (c(left->m_storage[l], right->m_storage[0], left->m_storage.is_null(l),
                      right->m_storage.is_null(0)))
                    return 0;
            }
        }

        return not_found; // no match
    }

    std::unique_ptr<Subexpr> clone(QueryNodeHandoverPatches*) const override
    {
        return make_subexpr<Value<T>>(*this);
    }

    NullableVector<T> m_storage;
};

class ConstantStringValue : public Value<StringData> {
public:
    ConstantStringValue(const StringData& string)
        : Value()
        , m_string(string.is_null() ? util::none : util::make_optional(std::string(string)))
    {
        init(false, ValueBase::default_size, m_string);
    }

    std::unique_ptr<Subexpr> clone(QueryNodeHandoverPatches*) const override
    {
        return std::unique_ptr<Subexpr>(new ConstantStringValue(*this));
    }

private:
    ConstantStringValue(const ConstantStringValue& other)
        : Value()
        , m_string(other.m_string)
    {
        init(other.m_from_link_list, other.m_values, m_string);
    }

    util::Optional<std::string> m_string;
};

// All overloads where left-hand-side is L:
//
// left-hand-side       operator                              right-hand-side
// L                    +, -, *, /, <, >, ==, !=, <=, >=      Subexpr2<R>
//
// For L = R = {int, int64_t, float, double, Timestamp}:
// Compare numeric values
template <class R>
Query operator>(double left, const Subexpr2<R>& right)
{
    return create<Greater>(left, right);
}
template <class R>
Query operator>(float left, const Subexpr2<R>& right)
{
    return create<Greater>(left, right);
}
template <class R>
Query operator>(int left, const Subexpr2<R>& right)
{
    return create<Greater>(left, right);
}
template <class R>
Query operator>(int64_t left, const Subexpr2<R>& right)
{
    return create<Greater>(left, right);
}
template <class R>
Query operator>(Timestamp left, const Subexpr2<R>& right)
{
    return create<Greater>(left, right);
}

template <class R>
Query operator<(double left, const Subexpr2<R>& right)
{
    return create<Less>(left, right);
}
template <class R>
Query operator<(float left, const Subexpr2<R>& right)
{
    return create<Less>(left, right);
}
template <class R>
Query operator<(int left, const Subexpr2<R>& right)
{
    return create<Less>(left, right);
}
template <class R>
Query operator<(int64_t left, const Subexpr2<R>& right)
{
    return create<Less>(left, right);
}
template <class R>
Query operator<(Timestamp left, const Subexpr2<R>& right)
{
    return create<Less>(left, right);
}
template <class R>
Query operator==(double left, const Subexpr2<R>& right)
{
    return create<Equal>(left, right);
}
template <class R>
Query operator==(float left, const Subexpr2<R>& right)
{
    return create<Equal>(left, right);
}
template <class R>
Query operator==(int left, const Subexpr2<R>& right)
{
    return create<Equal>(left, right);
}
template <class R>
Query operator==(int64_t left, const Subexpr2<R>& right)
{
    return create<Equal>(left, right);
}
template <class R>
Query operator==(Timestamp left, const Subexpr2<R>& right)
{
    return create<Equal>(left, right);
}
template <class R>
Query operator>=(double left, const Subexpr2<R>& right)
{
    return create<GreaterEqual>(left, right);
}
template <class R>
Query operator>=(float left, const Subexpr2<R>& right)
{
    return create<GreaterEqual>(left, right);
}
template <class R>
Query operator>=(int left, const Subexpr2<R>& right)
{
    return create<GreaterEqual>(left, right);
}
template <class R>
Query operator>=(int64_t left, const Subexpr2<R>& right)
{
    return create<GreaterEqual>(left, right);
}
template <class R>
Query operator>=(Timestamp left, const Subexpr2<R>& right)
{
    return create<GreaterEqual>(left, right);
}
template <class R>
Query operator<=(double left, const Subexpr2<R>& right)
{
    return create<LessEqual>(left, right);
}
template <class R>
Query operator<=(float left, const Subexpr2<R>& right)
{
    return create<LessEqual>(left, right);
}
template <class R>
Query operator<=(int left, const Subexpr2<R>& right)
{
    return create<LessEqual>(left, right);
}
template <class R>
Query operator<=(int64_t left, const Subexpr2<R>& right)
{
    return create<LessEqual>(left, right);
}
template <class R>
Query operator<=(Timestamp left, const Subexpr2<R>& right)
{
    return create<LessEqual>(left, right);
}
template <class R>
Query operator!=(double left, const Subexpr2<R>& right)
{
    return create<NotEqual>(left, right);
}
template <class R>
Query operator!=(float left, const Subexpr2<R>& right)
{
    return create<NotEqual>(left, right);
}
template <class R>
Query operator!=(int left, const Subexpr2<R>& right)
{
    return create<NotEqual>(left, right);
}
template <class R>
Query operator!=(int64_t left, const Subexpr2<R>& right)
{
    return create<NotEqual>(left, right);
}
template <class R>
Query operator!=(Timestamp left, const Subexpr2<R>& right)
{
    return create<NotEqual>(left, right);
}

// Arithmetic
template <class R>
Operator<Plus<typename Common<R, double>::type>> operator+(double left, const Subexpr2<R>& right)
{
    return {make_subexpr<Value<double>>(left), right.clone()};
}
template <class R>
Operator<Plus<typename Common<R, float>::type>> operator+(float left, const Subexpr2<R>& right)
{
    return {make_subexpr<Value<float>>(left), right.clone()};
}
template <class R>
Operator<Plus<typename Common<R, int>::type>> operator+(int left, const Subexpr2<R>& right)
{
    return {make_subexpr<Value<int>>(left), right.clone()};
}
template <class R>
Operator<Plus<typename Common<R, int64_t>::type>> operator+(int64_t left, const Subexpr2<R>& right)
{
    return {make_subexpr<Value<int64_t>>(left), right.clone()};
}
template <class R>
Operator<Minus<typename Common<R, double>::type>> operator-(double left, const Subexpr2<R>& right)
{
    return {make_subexpr<Value<double>>(left), right.clone()};
}
template <class R>
Operator<Minus<typename Common<R, float>::type>> operator-(float left, const Subexpr2<R>& right)
{
    return {make_subexpr<Value<float>>(left), right.clone()};
}
template <class R>
Operator<Minus<typename Common<R, int>::type>> operator-(int left, const Subexpr2<R>& right)
{
    return {make_subexpr<Value<int>>(left), right.clone()};
}
template <class R>
Operator<Minus<typename Common<R, int64_t>::type>> operator-(int64_t left, const Subexpr2<R>& right)
{
    return {make_subexpr<Value<int64_t>>(left), right.clone()};
}
template <class R>
Operator<Mul<typename Common<R, double>::type>> operator*(double left, const Subexpr2<R>& right)
{
    return {make_subexpr<Value<double>>(left), right.clone()};
}
template <class R>
Operator<Mul<typename Common<R, float>::type>> operator*(float left, const Subexpr2<R>& right)
{
    return {make_subexpr<Value<float>>(left), right.clone()};
}
template <class R>
Operator<Mul<typename Common<R, int>::type>> operator*(int left, const Subexpr2<R>& right)
{
    return {make_subexpr<Value<int>>(left), right.clone()};
}
template <class R>
Operator<Mul<typename Common<R, int64_t>::type>> operator*(int64_t left, const Subexpr2<R>& right)
{
    return {make_subexpr<Value<int64_t>>(left), right.clone()};
}
template <class R>
Operator<Div<typename Common<R, double>::type>> operator/(double left, const Subexpr2<R>& right)
{
    return {make_subexpr<Value<double>>(left), right.clone()};
}
template <class R>
Operator<Div<typename Common<R, float>::type>> operator/(float left, const Subexpr2<R>& right)
{
    return {make_subexpr<Value<float>>(left), right.clone()};
}
template <class R>
Operator<Div<typename Common<R, int>::type>> operator/(int left, const Subexpr2<R>& right)
{
    return {make_subexpr<Value<int>>(left), right.clone()};
}
template <class R>
Operator<Div<typename Common<R, int64_t>::type>> operator/(int64_t left, const Subexpr2<R>& right)
{
    return {make_subexpr<Value<int64_t>>(left), right.clone()};
}

// Unary operators
template <class T>
UnaryOperator<Pow<T>> power(const Subexpr2<T>& left)
{
    return {left.clone()};
}

// Classes used for LinkMap (see below).
struct LinkMapFunction {
    // Your consume() method is given row index of the linked-to table as argument, and you must return whether or
    // not you want the LinkMapFunction to exit (return false) or continue (return true) harvesting the link tree
    // for the current main table row index (it will be a link tree if you have multiple type_LinkList columns
    // in a link()->link() query.
    virtual bool consume(size_t row_index) = 0;
};

struct FindNullLinks : public LinkMapFunction {
    bool consume(size_t row_index) override
    {
        static_cast<void>(row_index);
        m_has_link = true;
        return false; // we've found a row index, so this can't be a null-link, so exit link harvesting
    }

    bool m_has_link = false;
};

struct MakeLinkVector : public LinkMapFunction {
    MakeLinkVector(std::vector<size_t>& result)
        : m_links(result)
    {
    }

    bool consume(size_t row_index) override
    {
        m_links.push_back(row_index);
        return true; // continue evaluation
    }
    std::vector<size_t>& m_links;
};

struct CountLinks : public LinkMapFunction {
    bool consume(size_t) override
    {
        m_link_count++;
        return true;
    }

    size_t result() const
    {
        return m_link_count;
    }

    size_t m_link_count = 0;
};

struct CountBacklinks : public LinkMapFunction {
    CountBacklinks(const Table* t)
        : m_table(t)
    {
    }

    bool consume(size_t row_index) override
    {
        m_link_count += m_table->get_backlink_count(row_index);
        return true;
    }

    size_t result() const
    {
        return m_link_count;
    }

    const Table* m_table;
    size_t m_link_count = 0;
};


/*
The LinkMap and LinkMapFunction classes are used for query conditions on links themselves (contrary to conditions on
the value payload they point at).

MapLink::map_links() takes a row index of the link column as argument and follows any link chain stated in the query
(through the link()->link() methods) until the final payload table is reached, and then applies LinkMapFunction on
the linked-to row index(es).

If all link columns are type_Link, then LinkMapFunction is only invoked for a single row index. If one or more
columns are type_LinkList, then it may result in multiple row indexes.

The reason we use this map pattern is that we can exit the link-tree-traversal as early as possible, e.g. when we've
found the first link that points to row '5'. Other solutions could be a std::vector<size_t> harvest_all_links(), or an
iterator pattern. First solution can't exit, second solution requires internal state.
*/
class LinkMap {
public:
    LinkMap() = default;
    LinkMap(const Table* table, std::vector<size_t> columns)
        : m_link_column_indexes(std::move(columns))
    {
        set_base_table(table);
    }

    LinkMap(LinkMap const& other, QueryNodeHandoverPatches* patches)
        : LinkMap(other)
    {
        if (!patches)
            return;

        m_link_column_indexes.clear();
        const Table* table = base_table();
        m_tables.clear();
        for (auto column : m_link_columns) {
            m_link_column_indexes.push_back(column->get_column_index());
            if (table->get_real_column_type(m_link_column_indexes.back()) == col_type_BackLink)
                table = &static_cast<const BacklinkColumn*>(column)->get_origin_table();
            else
                table = &static_cast<const LinkColumnBase*>(column)->get_target_table();
        }
    }

    void set_base_table(const Table* table)
    {
        if (table == base_table())
            return;

        m_tables.clear();
        m_tables.push_back(table);
        m_link_columns.clear();
        m_link_types.clear();
        m_only_unary_links = true;

        for (size_t link_column_index : m_link_column_indexes) {
            // Link column can be either LinkList or single Link
            const Table* t = m_tables.back();
            ColumnType type = t->get_real_column_type(link_column_index);
            REALM_ASSERT(Table::is_link_type(type) || type == col_type_BackLink);
            m_link_types.push_back(type);

            if (type == col_type_LinkList) {
                const LinkListColumn& cll = t->get_column_link_list(link_column_index);
                m_link_columns.push_back(&cll);
                m_only_unary_links = false;
                m_tables.push_back(&cll.get_target_table());
            }
            else if (type == col_type_Link) {
                const LinkColumn& cl = t->get_column_link(link_column_index);
                m_link_columns.push_back(&cl);
                m_tables.push_back(&cl.get_target_table());
            }
            else if (type == col_type_BackLink) {
                const BacklinkColumn& bl = t->get_column_backlink(link_column_index);
                m_link_columns.push_back(&bl);
                m_only_unary_links = false;
                m_tables.push_back(&bl.get_origin_table());
            }
        }
    }

    void verify_columns() const
    {
        for (size_t i = 0; i < m_link_column_indexes.size(); i++) {
            m_tables[i]->verify_column(m_link_column_indexes[i], m_link_columns[i]);
        }
    }

    virtual std::string description(util::serializer::SerialisationState& state) const
    {
        std::string s;
        for (size_t i = 0; i < m_link_column_indexes.size(); ++i) {
            if (i < m_tables.size() && m_tables[i]) {
                s += state.get_column_name(m_tables[i]->get_table_ref(), m_link_column_indexes[i]);
                if (i != m_link_column_indexes.size() - 1) {
                    s += util::serializer::value_separator;
                }
            }
        }
        return s;
    }

    std::vector<size_t> get_links(size_t index)
    {
        std::vector<size_t> res;
        get_links(index, res);
        return res;
    }

    size_t count_links(size_t row)
    {
        CountLinks counter;
        map_links(row, counter);
        return counter.result();
    }

    size_t count_all_backlinks(size_t row)
    {
        CountBacklinks counter(target_table());
        map_links(row, counter);
        return counter.result();
    }

    void map_links(size_t row, LinkMapFunction& lm)
    {
        map_links(0, row, lm);
    }

    bool only_unary_links() const
    {
        return m_only_unary_links;
    }

    const Table* base_table() const
    {
        return m_tables.empty() ? nullptr : m_tables[0];
    }

    const Table* target_table() const
    {
        REALM_ASSERT(!m_tables.empty());
        return m_tables.back();
    }

    bool links_exist() const
    {
        return !m_link_columns.empty();
    }

    std::vector<const ColumnBase*> m_link_columns;

private:
    void map_links(size_t column, size_t row, LinkMapFunction& lm)
    {
        bool last = (column + 1 == m_link_columns.size());
        ColumnType type = m_link_types[column];
        if (type == col_type_Link) {
            const LinkColumn& cl = *static_cast<const LinkColumn*>(m_link_columns[column]);
            size_t r = to_size_t(cl.get(row));
            if (r == 0)
                return;
            r--; // LinkColumn stores link to row N as N + 1
            if (last) {
                bool continue2 = lm.consume(r);
                if (!continue2)
                    return;
            }
            else
                map_links(column + 1, r, lm);
        }
        else if (type == col_type_LinkList) {
            const LinkListColumn& cll = *static_cast<const LinkListColumn*>(m_link_columns[column]);
            ConstLinkViewRef lvr = cll.get(row);
            for (size_t t = 0; t < lvr->size(); t++) {
                size_t r = lvr->get(t).get_index();
                if (last) {
                    bool continue2 = lm.consume(r);
                    if (!continue2)
                        return;
                }
                else
                    map_links(column + 1, r, lm);
            }
        }
        else if (type == col_type_BackLink) {
            const BacklinkColumn& bl = *static_cast<const BacklinkColumn*>(m_link_columns[column]);
            size_t count = bl.get_backlink_count(row);
            for (size_t i = 0; i < count; ++i) {
                size_t r = bl.get_backlink(row, i);
                if (last) {
                    bool continue2 = lm.consume(r);
                    if (!continue2)
                        return;
                }
                else
                    map_links(column + 1, r, lm);
            }
        }
    }


    void get_links(size_t row, std::vector<size_t>& result)
    {
        MakeLinkVector mlv = MakeLinkVector(result);
        map_links(row, mlv);
    }

    std::vector<size_t> m_link_column_indexes;
    std::vector<ColumnType> m_link_types;
    std::vector<const Table*> m_tables;
    bool m_only_unary_links = true;

    template <class>
    friend Query compare(const Subexpr2<Link>&, const ConstRow&);
};

template <class T, class S, class I>
Query string_compare(const Subexpr2<StringData>& left, T right, bool case_insensitive);
template <class S, class I>
Query string_compare(const Subexpr2<StringData>& left, const Subexpr2<StringData>& right, bool case_insensitive);

template <class T>
Value<T> make_value_for_link(bool only_unary_links, size_t size)
{
    Value<T> value;
    if (only_unary_links) {
        REALM_ASSERT(size <= 1);
        value.init(false, 1);
        value.m_storage.set_null(0);
    }
    else {
        value.init(true, size);
    }
    return value;
}


// If we add a new Realm type T and quickly want Query support for it, then simply inherit from it like
// `template <> class Columns<T> : public SimpleQuerySupport<T>` and you're done. Any operators of the set
// { ==, >=, <=, !=, >, < } that are supported by T will be supported by the "query expression syntax"
// automatically. NOTE: This method of Query support will be slow because it goes through Table::get<T>.
// To get faster Query support, either add SequentialGetter support (faster) or create a query_engine.hpp
// node for it (super fast).

template <class T>
class SimpleQuerySupport : public Subexpr2<T> {
public:
    SimpleQuerySupport(size_t column, const Table* table, std::vector<size_t> links = {})
        : m_column_ndx(column)
        , m_link_map(table, std::move(links))
    {
        m_column = &m_link_map.target_table()->get_column_base(m_column_ndx);
    }

    bool is_nullable() const noexcept
    {
        return m_link_map.base_table()->is_nullable(m_column->get_column_index());
    }

    const Table* get_base_table() const override
    {
        return m_link_map.base_table();
    }

    void set_base_table(const Table* table) override
    {
        if (table != get_base_table()) {
            m_link_map.set_base_table(table);
            m_column = &m_link_map.target_table()->get_column_base(m_column_ndx);
        }
    }

    void verify_column() const override
    {
        // verify links
        m_link_map.verify_columns();
        // verify target table
        const Table* target_table = m_link_map.target_table();
        if (target_table && m_column_ndx != npos) {
            target_table->verify_column(m_column_ndx, m_column);
        }
    }

    void evaluate(size_t index, ValueBase& destination) override
    {
        Value<T>& d = static_cast<Value<T>&>(destination);
        size_t col = column_ndx();

        if (links_exist()) {
            std::vector<size_t> links = m_link_map.get_links(index);
            Value<T> v = make_value_for_link<T>(m_link_map.only_unary_links(), links.size());

            for (size_t t = 0; t < links.size(); t++) {
                size_t link_to = links[t];
                v.m_storage.set(t, m_link_map.target_table()->template get<T>(col, link_to));
            }
            destination.import(v);
        }
        else {
            // Not a link column
            const Table* target_table = m_link_map.target_table();
            for (size_t t = 0; t < destination.m_values && index + t < target_table->size(); t++) {
                d.m_storage.set(t, target_table->get<T>(col, index + t));
            }
        }
    }

    bool links_exist() const
    {
        return m_link_map.m_link_columns.size() > 0;
    }

    virtual std::string description(util::serializer::SerialisationState& state) const override
    {
        return state.describe_columns(m_link_map, m_column_ndx);
    }

    std::unique_ptr<Subexpr> clone(QueryNodeHandoverPatches* patches = nullptr) const override
    {
        return make_subexpr<Columns<T>>(static_cast<const Columns<T>&>(*this), patches);
    }

    SimpleQuerySupport(SimpleQuerySupport const& other, QueryNodeHandoverPatches* patches)
        : Subexpr2<T>(other)
        , m_column_ndx(other.m_column_ndx)
        , m_column(other.m_column)
        , m_link_map(other.m_link_map, patches)
    {
        if (patches && m_column) {
            m_column_ndx = column_ndx();
            m_column = nullptr;
        }
    }

    size_t column_ndx() const
    {
        return m_column->get_column_index();
    }

    SizeOperator<Size<T>> size()
    {
        return SizeOperator<Size<T>>(this->clone(nullptr));
    }

private:
    // Column index of payload column of m_table
    mutable size_t m_column_ndx;
    const ColumnBase* m_column;
    LinkMap m_link_map;
};


template <>
class Columns<Timestamp> : public SimpleQuerySupport<Timestamp> {
    using SimpleQuerySupport::SimpleQuerySupport;
};

template <>
class Columns<BinaryData> : public SimpleQuerySupport<BinaryData> {
    using SimpleQuerySupport::SimpleQuerySupport;
};

template <>
class Columns<StringData> : public SimpleQuerySupport<StringData> {
public:
    Columns(size_t column, const Table* table, std::vector<size_t> links = {})
        : SimpleQuerySupport(column, table, links)
    {
    }

    Columns(Columns const& other, QueryNodeHandoverPatches* patches = nullptr)
        : SimpleQuerySupport(other, patches)
    {
    }

    Columns(Columns&& other)
        : SimpleQuerySupport(other)
    {
    }
};

template <class T, class S, class I>
Query string_compare(const Subexpr2<StringData>& left, T right, bool case_sensitive)
{
    StringData sd(right);
    if (case_sensitive)
        return create<S>(sd, left);
    else
        return create<I>(sd, left);
}

template <class S, class I>
Query string_compare(const Subexpr2<StringData>& left, const Subexpr2<StringData>& right, bool case_sensitive)
{
    if (case_sensitive)
        return make_expression<Compare<S, StringData>>(right.clone(), left.clone());
    else
        return make_expression<Compare<I, StringData>>(right.clone(), left.clone());
}

template <class T, class S, class I>
Query binary_compare(const Subexpr2<BinaryData>& left, T right, bool case_sensitive)
{
    BinaryData data(right);
    if (case_sensitive)
        return create<S>(data, left);
    else
        return create<I>(data, left);
}

template <class S, class I>
Query binary_compare(const Subexpr2<BinaryData>& left, const Subexpr2<BinaryData>& right, bool case_sensitive)
{
    if (case_sensitive)
        return make_expression<Compare<S, BinaryData>>(right.clone(), left.clone());
    else
        return make_expression<Compare<I, BinaryData>>(right.clone(), left.clone());
}


// Columns<String> == Columns<String>
inline Query operator==(const Columns<StringData>& left, const Columns<StringData>& right)
{
    return string_compare<Equal, EqualIns>(left, right, true);
}

// Columns<String> != Columns<String>
inline Query operator!=(const Columns<StringData>& left, const Columns<StringData>& right)
{
    return string_compare<NotEqual, NotEqualIns>(left, right, true);
}

// String == Columns<String>
template <class T>
Query operator==(T left, const Columns<StringData>& right)
{
    return operator==(right, left);
}

// String != Columns<String>
template <class T>
Query operator!=(T left, const Columns<StringData>& right)
{
    return operator!=(right, left);
}

// Columns<String> == String
template <class T>
Query operator==(const Columns<StringData>& left, T right)
{
    return string_compare<T, Equal, EqualIns>(left, right, true);
}

// Columns<String> != String
template <class T>
Query operator!=(const Columns<StringData>& left, T right)
{
    return string_compare<T, NotEqual, NotEqualIns>(left, right, true);
}


inline Query operator==(const Columns<BinaryData>& left, BinaryData right)
{
    return create<Equal>(right, left);
}

inline Query operator==(BinaryData left, const Columns<BinaryData>& right)
{
    return create<Equal>(left, right);
}

inline Query operator!=(const Columns<BinaryData>& left, BinaryData right)
{
    return create<NotEqual>(right, left);
}

inline Query operator!=(BinaryData left, const Columns<BinaryData>& right)
{
    return create<NotEqual>(left, right);
}


// This class is intended to perform queries on the *pointers* of links, contrary to performing queries on *payload*
// in linked-to tables. Queries can be "find first link that points at row X" or "find first null-link". Currently
// only "find first null link" and "find first non-null link" is supported. More will be added later. When we add
// more, I propose to remove the <bool has_links> template argument from this class and instead template it by
// a criteria-class (like the FindNullLinks class below in find_first()) in some generalized fashion.
template <bool has_links>
class UnaryLinkCompare : public Expression {
public:
    UnaryLinkCompare(LinkMap lm)
        : m_link_map(std::move(lm))
    {
    }

    void set_base_table(const Table* table) override
    {
        m_link_map.set_base_table(table);
    }

    void verify_column() const override
    {
        m_link_map.verify_columns();
    }

    // Return main table of query (table on which table->where()... is invoked). Note that this is not the same as
    // any linked-to payload tables
    const Table* get_base_table() const override
    {
        return m_link_map.base_table();
    }

    size_t find_first(size_t start, size_t end) const override
    {
        for (; start < end;) {
            FindNullLinks fnl;
            m_link_map.map_links(start, fnl);
            if (fnl.m_has_link == has_links)
                return start;

            start++;
        }

        return not_found;
    }

    virtual std::string description(util::serializer::SerialisationState& state) const override
    {
        return state.describe_columns(m_link_map, realm::npos) + (has_links ? " != NULL" : " == NULL");
    }

    std::unique_ptr<Expression> clone(QueryNodeHandoverPatches* patches) const override
    {
        return std::unique_ptr<Expression>(new UnaryLinkCompare(*this, patches));
    }

private:
    UnaryLinkCompare(const UnaryLinkCompare& other, QueryNodeHandoverPatches* patches = nullptr)
        : Expression(other)
        , m_link_map(other.m_link_map, patches)
    {
    }

    mutable LinkMap m_link_map;
};

class LinkCount : public Subexpr2<Int> {
public:
    LinkCount(LinkMap link_map)
        : m_link_map(std::move(link_map))
    {
    }
    LinkCount(LinkCount const& other, QueryNodeHandoverPatches* patches)
        : Subexpr2<Int>(other)
        , m_link_map(other.m_link_map, patches)
    {
    }

    std::unique_ptr<Subexpr> clone(QueryNodeHandoverPatches* patches) const override
    {
        return make_subexpr<LinkCount>(*this, patches);
    }

    const Table* get_base_table() const override
    {
        return m_link_map.base_table();
    }

    void set_base_table(const Table* table) override
    {
        m_link_map.set_base_table(table);
    }

    void verify_column() const override
    {
        m_link_map.verify_columns();
    }

    void evaluate(size_t index, ValueBase& destination) override
    {
        size_t count = m_link_map.count_links(index);
        destination.import(Value<Int>(false, 1, count));
    }

    virtual std::string description(util::serializer::SerialisationState& state) const override
    {
        return state.describe_columns(m_link_map, realm::npos) + util::serializer::value_separator + "@count";
    }

private:
    LinkMap m_link_map;
};

// Gives a count of all backlinks across all columns for the specified row.
// The unused template parameter is a hack to avoid a circular dependency between table.hpp and query_expression.hpp.
template <class>
class BacklinkCount : public Subexpr2<Int> {
public:
    BacklinkCount(LinkMap link_map)
    : m_link_map(std::move(link_map))
    {
    }
    BacklinkCount(const Table* table, std::vector<size_t> links = {})
    : m_link_map(table, std::move(links))
    {
    }
    BacklinkCount(BacklinkCount const& other, QueryNodeHandoverPatches* patches)
    : Subexpr2<Int>(other)
    , m_link_map(other.m_link_map, patches)
    {
    }

    std::unique_ptr<Subexpr> clone(QueryNodeHandoverPatches* patches) const override
    {
        return make_subexpr<BacklinkCount<Int> >(*this, patches);
    }

    const Table* get_base_table() const override
    {
        return m_link_map.base_table();
    }

    void set_base_table(const Table* table) override
    {
        m_link_map.set_base_table(table);
    }

    void verify_column() const override
    {
        m_link_map.verify_columns();
    }

    void evaluate(size_t index, ValueBase& destination) override
    {
        size_t count;
        if (m_link_map.links_exist()) {
            count = m_link_map.count_all_backlinks(index);
        }
        else {
            count = m_link_map.target_table()->get_backlink_count(index);
        }
        destination.import(Value<Int>(false, 1, count));
    }

    virtual std::string description(util::serializer::SerialisationState& state) const override
    {
        std::string s;
        if (m_link_map.links_exist()) {
            s += state.describe_columns(m_link_map, realm::npos) + util::serializer::value_separator;
        }
        s += "@links.@count";
        return s;
    }
private:
    LinkMap m_link_map;
};


template <class oper, class TExpr>
class SizeOperator : public Subexpr2<Int> {
public:
    SizeOperator(std::unique_ptr<TExpr> left)
        : m_expr(std::move(left))
    {
    }

    // See comment in base class
    void set_base_table(const Table* table) override
    {
        m_expr->set_base_table(table);
    }

    void verify_column() const override
    {
        m_expr->verify_column();
    }

    // Recursively fetch tables of columns in expression tree. Used when user first builds a stand-alone expression
    // and binds it to a Query at a later time
    const Table* get_base_table() const override
    {
        return m_expr->get_base_table();
    }

    // destination = operator(left)
    void evaluate(size_t index, ValueBase& destination) override
    {
        REALM_ASSERT_DEBUG(dynamic_cast<Value<Int>*>(&destination) != nullptr);
        Value<Int>* d = static_cast<Value<Int>*>(&destination);
        REALM_ASSERT(d);

        Value<T> v;
        m_expr->evaluate(index, v);

        size_t sz = v.m_values;
        d->init(v.m_from_link_list, sz);

        for (size_t i = 0; i < sz; i++) {
            auto elem = v.m_storage.get(i);
            if (!elem) {
                d->m_storage.set_null(i);
            }
            else {
                d->m_storage.set(i, oper()(*elem));
            }
        }
    }

    std::string description(util::serializer::SerialisationState& state) const override
    {
        if (m_expr) {
            return m_expr->description(state) + util::serializer::value_separator + "@size";
        }
        return "@size";
    }

    std::unique_ptr<Subexpr> clone(QueryNodeHandoverPatches* patches) const override
    {
        return std::unique_ptr<Subexpr>(new SizeOperator(*this, patches));
    }

    void apply_handover_patch(QueryNodeHandoverPatches& patches, Group& group) override
    {
        m_expr->apply_handover_patch(patches, group);
    }

private:
    SizeOperator(const SizeOperator& other, QueryNodeHandoverPatches* patches)
        : m_expr(other.m_expr->clone(patches))
    {
    }

    typedef typename oper::type T;
    std::unique_ptr<TExpr> m_expr;
};

struct ConstantRowValueHandoverPatch : public QueryNodeHandoverPatch {
    std::unique_ptr<RowBaseHandoverPatch> row_patch;
};

class ConstantRowValue : public Subexpr2<Link> {
public:
    ConstantRowValue(const ConstRow& row)
        : m_row(row)
    {
    }

    void set_base_table(const Table*) override
    {
    }

    void verify_column() const override
    {
    }

    const Table* get_base_table() const override
    {
        return nullptr;
    }

    void evaluate(size_t, ValueBase& destination) override
    {
        if (m_row.is_attached()) {
            Value<RowIndex> v(RowIndex(m_row.get_index()));
            destination.import(v);
        }
        else {
            Value<RowIndex> v(RowIndex::Detached);
            destination.import(v);
        }
    }

    virtual std::string description(util::serializer::SerialisationState&) const override
    {
        throw SerialisationError("Serialising a query which links to an object is currently unsupported.");
        // TODO: we can do something like the following when core gets stable keys:
        //if (!m_row.is_attached()) {
        //    return util::serializer::print_value("detached object");
        //}
        //return util::serializer::print_value(m_row.get_index());
    }

    std::unique_ptr<Subexpr> clone(QueryNodeHandoverPatches* patches) const override
    {
        return std::unique_ptr<Subexpr>(new ConstantRowValue(*this, patches));
    }

    void apply_handover_patch(QueryNodeHandoverPatches& patches, Group& group) override
    {
        REALM_ASSERT(patches.size());
        std::unique_ptr<QueryNodeHandoverPatch> abstract_patch = std::move(patches.back());
        patches.pop_back();

        auto patch = dynamic_cast<ConstantRowValueHandoverPatch*>(abstract_patch.get());
        REALM_ASSERT(patch);

        m_row.apply_and_consume_patch(patch->row_patch, group);
    }

private:
    ConstantRowValue(const ConstantRowValue& source, QueryNodeHandoverPatches* patches)
        : m_row(patches ? ConstRow() : source.m_row)
    {
        if (!patches)
            return;

        std::unique_ptr<ConstantRowValueHandoverPatch> patch(new ConstantRowValueHandoverPatch);
        ConstRow::generate_patch(source.m_row, patch->row_patch);
        patches->emplace_back(patch.release());
    }

    ConstRow m_row;
};

template <typename T>
class SubColumns;

// This is for LinkList and BackLink too since they're declared as typedefs of Link.
template <>
class Columns<Link> : public Subexpr2<Link> {
public:
    Query is_null()
    {
        if (m_link_map.m_link_columns.size() > 1)
            throw std::runtime_error("Combining link() and is_null() is currently not supported");
        // Todo, it may be useful to support the above, but we would need to figure out an intuitive behaviour
        return make_expression<UnaryLinkCompare<false>>(m_link_map);
    }

    Query is_not_null()
    {
        if (m_link_map.m_link_columns.size() > 1)
            throw std::runtime_error("Combining link() and is_not_null() is currently not supported");
        // Todo, it may be useful to support the above, but we would need to figure out an intuitive behaviour
        return make_expression<UnaryLinkCompare<true>>(m_link_map);
    }

    LinkCount count() const
    {
        return LinkCount(m_link_map);
    }

    template <class T>
    BacklinkCount<T> backlink_count() const
    {
        return BacklinkCount<T>(m_link_map);
    }

    template <typename C>
    SubColumns<C> column(size_t column_ndx) const
    {
        return SubColumns<C>(Columns<C>(column_ndx, m_link_map.target_table()), m_link_map);
    }

    const LinkMap& link_map() const
    {
        return m_link_map;
    }

    const Table* get_base_table() const override
    {
        return m_link_map.base_table();
    }
    void set_base_table(const Table* table) override
    {
        m_link_map.set_base_table(table);
    }

    void verify_column() const override
    {
        m_link_map.verify_columns();
    }

    std::string description(util::serializer::SerialisationState& state) const override
    {
        return state.describe_columns(m_link_map, realm::npos);
    }

    std::unique_ptr<Subexpr> clone(QueryNodeHandoverPatches* patches) const override
    {
        return std::unique_ptr<Subexpr>(new Columns<Link>(*this, patches));
    }

    void evaluate(size_t index, ValueBase& destination) override;


private:
    LinkMap m_link_map;
    friend class Table;

    Columns(size_t column_ndx, const Table* table, const std::vector<size_t>& links = {})
        : m_link_map(table, links)
    {
        static_cast<void>(column_ndx);
    }
    Columns(const Columns& other, QueryNodeHandoverPatches* patches)
        : Subexpr2<Link>(other)
        , m_link_map(other.m_link_map, patches)
    {
    }
};

template <typename T>
class ListColumns;
template <typename T, typename Operation>
class ListColumnAggregate;
namespace aggregate_operations {
template <typename T>
class Minimum;
template <typename T>
class Maximum;
template <typename T>
class Sum;
template <typename T>
class Average;
}

template <>
class Columns<SubTable> : public Subexpr2<SubTable> {
public:
    const Table* get_base_table() const override
    {
        return m_link_map.base_table();
    }

    void set_base_table(const Table* table) override
    {
        m_link_map.set_base_table(table);
        m_column = &m_link_map.target_table()->get_column_table(m_column_ndx);
    }

    void verify_column() const override
    {
        m_link_map.verify_columns();
        m_link_map.target_table()->verify_column(m_column_ndx, m_column);
    }

    std::string description(util::serializer::SerialisationState&) const override
    {
        throw SerialisationError("Serialisation of query expressions involving subtables is not yet supported.");
    }

    std::unique_ptr<Subexpr> clone(QueryNodeHandoverPatches* patches) const override
    {
        return std::unique_ptr<Subexpr>(new Columns<SubTable>(*this, patches));
    }

    void evaluate(size_t index, ValueBase& destination) override
    {
        evaluate_internal(index, destination, ValueBase::default_size);
    }

    void evaluate_internal(size_t index, ValueBase& destination, size_t nb_elements);

    template <typename T>
    ListColumns<T> column(size_t ndx) const
    {
        return ListColumns<T>(ndx, Columns<SubTable>(*this, nullptr));
    }

    template <typename T>
    ListColumns<T> list() const
    {
        return column<T>(0);
    }

    SizeOperator<Size<ConstTableRef>> size()
    {
        return SizeOperator<Size<ConstTableRef>>(this->clone(nullptr));
    }

private:
    LinkMap m_link_map;
    size_t m_column_ndx;
    const SubtableColumn* m_column = nullptr;
    friend class Table;
    template <class T>
    friend class ListColumnsBase;
    template <class T, class U>
    friend class ListColumnAggregate;

    Columns(size_t column_ndx, const Table* table, const std::vector<size_t>& links = {})
        : m_link_map(table, links)
        , m_column_ndx(column_ndx)
        , m_column(&m_link_map.target_table()->get_column_table(column_ndx))
    {
    }

    Columns(const Columns<SubTable>& other, QueryNodeHandoverPatches* patches)
        : Subexpr2<SubTable>(other)
        , m_link_map(other.m_link_map, patches)
        , m_column_ndx(other.m_column_ndx)
        , m_column(other.m_column)
    {
        if (m_column && patches)
            m_column_ndx = m_column->get_column_index();
    }
};

template <typename T>
class ListColumnsBase : public Subexpr2<T> {
public:
    ListColumnsBase(size_t column_ndx, Columns<SubTable> column)
        : m_column_ndx(column_ndx)
        , m_subtable_column(std::move(column))
    {
    }

    ListColumnsBase(const ListColumnsBase& other, QueryNodeHandoverPatches* patches)
        : m_column_ndx(other.m_column_ndx)
        , m_subtable_column(other.m_subtable_column, patches)
    {
    }

    std::unique_ptr<Subexpr> clone(QueryNodeHandoverPatches* patches) const override
    {
        return make_subexpr<ListColumns<T>>(*this, patches);
    }

    const Table* get_base_table() const override
    {
        return m_subtable_column.get_base_table();
    }

    void set_base_table(const Table* table) override
    {
        m_subtable_column.set_base_table(table);
    }

    void verify_column() const override
    {
        m_subtable_column.verify_column();
    }

    void evaluate(size_t index, ValueBase& destination) override
    {
        Value<ConstTableRef> subtables;
        m_subtable_column.evaluate_internal(index, subtables, 1);
        size_t sz = 0;
        for (size_t i = 0; i < subtables.m_values; i++) {
            auto val = subtables.m_storage[i];
            if (val)
                sz += val->size();
        }
        auto v = make_value_for_link<typename util::RemoveOptional<T>::type>(false, sz);
        size_t k = 0;
        for (size_t i = 0; i < subtables.m_values; i++) {
            auto table = subtables.m_storage[i];
            if (table) {
                size_t s = table->size();
                for (size_t j = 0; j < s; j++) {
                    if (!table->is_null(m_column_ndx, j)) {
                        v.m_storage.set(k++, table->get<T>(m_column_ndx, j));
                    }
                }
            }
        }
        destination.import(v);
    }

    virtual std::string description(util::serializer::SerialisationState&) const override
    {
        throw SerialisationError("Serialisation of subtable expressions is not yet supported.");
    }

    ListColumnAggregate<T, aggregate_operations::Minimum<T>> min() const
    {
        return {m_column_ndx, m_subtable_column};
    }

    ListColumnAggregate<T, aggregate_operations::Maximum<T>> max() const
    {
        return {m_column_ndx, m_subtable_column};
    }

    ListColumnAggregate<T, aggregate_operations::Sum<T>> sum() const
    {
        return {m_column_ndx, m_subtable_column};
    }

    ListColumnAggregate<T, aggregate_operations::Average<T>> average() const
    {
        return {m_column_ndx, m_subtable_column};
    }


private:
    // Storing the column index here could be a potential problem if the column
    // changes id due to insertion/deletion.
    size_t m_column_ndx;
    Columns<SubTable> m_subtable_column;
};

template <class T>
class ListColumns : public ListColumnsBase<T> {
public:
    using ListColumnsBase<T>::ListColumnsBase;
};

template <>
class ListColumns<StringData> : public ListColumnsBase<StringData> {
public:
    ListColumns(size_t column_ndx, Columns<SubTable> column)
        : ListColumnsBase(column_ndx, column)
    {
    }

    ListColumns(const ListColumnsBase& other, QueryNodeHandoverPatches* patches)
        : ListColumnsBase(other, patches)
    {
    }

    ListColumns(ListColumns&& other)
        : ListColumnsBase(other)
    {
    }
};

template <typename T, typename Operation>
class ListColumnAggregate : public Subexpr2<typename Operation::ResultType> {
public:
    using R = typename Operation::ResultType;

    ListColumnAggregate(size_t column_ndx, Columns<SubTable> column)
        : m_column_ndx(column_ndx)
        , m_subtable_column(std::move(column))
    {
    }

    ListColumnAggregate(const ListColumnAggregate& other, QueryNodeHandoverPatches* patches)
        : m_column_ndx(other.m_column_ndx)
        , m_subtable_column(other.m_subtable_column, patches)
    {
    }

    std::unique_ptr<Subexpr> clone(QueryNodeHandoverPatches* patches) const override
    {
        return make_subexpr<ListColumnAggregate>(*this, patches);
    }

    const Table* get_base_table() const override
    {
        return m_subtable_column.get_base_table();
    }

    void set_base_table(const Table* table) override
    {
        m_subtable_column.set_base_table(table);
    }

    void verify_column() const override
    {
        m_subtable_column.verify_column();
    }

    void evaluate(size_t index, ValueBase& destination) override
    {
        Value<ConstTableRef> subtables;
        m_subtable_column.evaluate_internal(index, subtables, 1);
        REALM_ASSERT_DEBUG(subtables.m_values > 0 || subtables.m_from_link_list);
        size_t sz = subtables.m_values;
        // The result is an aggregate value for each table
        auto v = make_value_for_link<R>(!subtables.m_from_link_list, sz);
        for (unsigned i = 0; i < sz; i++) {
            auto table = subtables.m_storage[i];
            Operation op;
            if (table) {
                size_t s = table->size();
                for (unsigned j = 0; j < s; j++) {
                    op.accumulate(table->get<T>(m_column_ndx, j));
                }
            }
            if (op.is_null()) {
                v.m_storage.set_null(i);
            }
            else {
                v.m_storage.set(i, op.result());
            }
        }
        destination.import(v);
    }

    virtual std::string description(util::serializer::SerialisationState&) const override
    {
        throw SerialisationError("Serialisation of queries involving subtable expressions is not yet supported.");
    }

private:
    size_t m_column_ndx;
    Columns<SubTable> m_subtable_column;
};

template <class Operator>
Query compare(const Subexpr2<Link>& left, const ConstRow& row)
{
    static_assert(std::is_same<Operator, Equal>::value || std::is_same<Operator, NotEqual>::value,
                  "Links can only be compared for equality.");
    const Columns<Link>* column = dynamic_cast<const Columns<Link>*>(&left);
    if (column) {
        const LinkMap& link_map = column->link_map();
        REALM_ASSERT(link_map.target_table() == row.get_table() || !row.is_attached());
#ifdef REALM_OLDQUERY_FALLBACK
        if (link_map.m_link_columns.size() == 1) {
            // We can fall back to Query::links_to for != and == operations on links, but only
            // for == on link lists. This is because negating query.links_to() is equivalent to
            // to "ALL linklist != row" rather than the "ANY linklist != row" semantics we're after.
            if (link_map.m_link_types[0] == col_type_Link ||
                (link_map.m_link_types[0] == col_type_LinkList && std::is_same<Operator, Equal>::value)) {
                const Table* t = column->get_base_table();
                Query query(*t);

                if (std::is_same<Operator, NotEqual>::value) {
                    // Negate the following `links_to`.
                    query.Not();
                }
                query.links_to(link_map.m_link_column_indexes[0], row);
                return query;
            }
        }
#endif
    }
    return make_expression<Compare<Operator, RowIndex>>(left.clone(), make_subexpr<ConstantRowValue>(row));
}

inline Query operator==(const Subexpr2<Link>& left, const ConstRow& row)
{
    return compare<Equal>(left, row);
}
inline Query operator!=(const Subexpr2<Link>& left, const ConstRow& row)
{
    return compare<NotEqual>(left, row);
}
inline Query operator==(const ConstRow& row, const Subexpr2<Link>& right)
{
    return compare<Equal>(right, row);
}
inline Query operator!=(const ConstRow& row, const Subexpr2<Link>& right)
{
    return compare<NotEqual>(right, row);
}

template <class Operator>
Query compare(const Subexpr2<Link>& left, null)
{
    static_assert(std::is_same<Operator, Equal>::value || std::is_same<Operator, NotEqual>::value,
                  "Links can only be compared for equality.");
    return make_expression<Compare<Operator, RowIndex>>(left.clone(), make_subexpr<Value<RowIndex>>());
}

inline Query operator==(const Subexpr2<Link>& left, null)
{
    return compare<Equal>(left, null());
}
inline Query operator!=(const Subexpr2<Link>& left, null)
{
    return compare<NotEqual>(left, null());
}
inline Query operator==(null, const Subexpr2<Link>& right)
{
    return compare<Equal>(right, null());
}
inline Query operator!=(null, const Subexpr2<Link>& right)
{
    return compare<NotEqual>(right, null());
}


template <class T>
class Columns : public Subexpr2<T> {
public:
    using ColType = typename ColumnTypeTraits<T>::column_type;

    Columns(size_t column, const Table* table, std::vector<size_t> links = {})
        : m_link_map(table, std::move(links))
        , m_column_ndx(column)
        , m_nullable(m_link_map.target_table()->is_nullable(m_column_ndx))
    {
    }

    Columns(const Columns& other, QueryNodeHandoverPatches* patches = nullptr)
        : m_link_map(other.m_link_map, patches)
        , m_column_ndx(other.m_column_ndx)
        , m_nullable(other.m_nullable)
    {
        if (!other.m_sg)
            return;

        if (patches) {
            m_column_ndx = other.get_column_base().get_column_index();
        }
        else {
            if (m_nullable && std::is_same<typename ColType::value_type, int64_t>::value) {
                init<IntNullColumn>(&other.get_column_base());
            }
            else {
                init<ColType>(&other.get_column_base());
            }
        }
    }

    Columns& operator=(const Columns& other)
    {
        if (this != &other) {
            m_link_map = other.m_link_map;
            m_sg.reset();
            m_column_ndx = other.m_column_ndx;
            m_nullable = other.m_nullable;
        }
        return *this;
    }

    std::unique_ptr<Subexpr> clone(QueryNodeHandoverPatches* patches) const override
    {
        return make_subexpr<Columns<T>>(*this, patches);
    }

    // See comment in base class
    void set_base_table(const Table* table) override
    {
        if (m_sg && table == get_base_table())
            return;

        m_link_map.set_base_table(table);
        m_nullable = m_link_map.target_table()->is_nullable(m_column_ndx);

        const ColumnBase* c = &m_link_map.target_table()->get_column_base(m_column_ndx);
        if (m_nullable && std::is_same<typename ColType::value_type, int64_t>::value) {
            init<IntNullColumn>(c);
        }
        else {
            init<ColType>(c);
        }
    }

    void verify_column() const override
    {
        // verify links
        m_link_map.verify_columns();
        // verify target table
        const Table* target_table = m_link_map.target_table();
        if (target_table && m_column_ndx != npos) {
            target_table->verify_column(m_column_ndx, &get_column_base());
        }
    }

    template <class ActualColType>
    void init(const ColumnBase* c)
    {
        REALM_ASSERT_DEBUG(dynamic_cast<const ActualColType*>(c));
        if (m_sg == nullptr) {
            m_sg.reset(new SequentialGetter<ActualColType>());
        }
        static_cast<SequentialGetter<ActualColType>&>(*m_sg).init(static_cast<const ActualColType*>(c));
    }

    // Recursively fetch tables of columns in expression tree. Used when user first builds a stand-alone expression
    // and binds it to a Query at a later time
    const Table* get_base_table() const override
    {
        return m_link_map.base_table();
    }

    template <class ColType2 = ColType>
    void evaluate_internal(size_t index, ValueBase& destination)
    {
        REALM_ASSERT_DEBUG(m_sg.get());
        REALM_ASSERT_DEBUG(dynamic_cast<SequentialGetter<ColType2>*>(m_sg.get()));

        using U = typename ColType2::value_type;
        auto sgc = static_cast<SequentialGetter<ColType2>*>(m_sg.get());
        REALM_ASSERT_DEBUG(sgc->m_column);

        if (links_exist()) {
            // LinkList with more than 0 values. Create Value with payload for all fields

            std::vector<size_t> links = m_link_map.get_links(index);
            auto v = make_value_for_link<typename util::RemoveOptional<U>::type>(m_link_map.only_unary_links(),
                                                                                 links.size());

            for (size_t t = 0; t < links.size(); t++) {
                size_t link_to = links[t];
                sgc->cache_next(link_to);

                if (sgc->m_column->is_null(link_to))
                    v.m_storage.set_null(t);
                else
                    v.m_storage.set(t, sgc->get_next(link_to));
            }
            destination.import(v);
        }
        else {
            // Not a Link column
            // make sequential getter load the respective leaf to access data at column row 'index'
            sgc->cache_next(index);
            size_t colsize = sgc->m_column->size();

            // Now load `ValueBase::default_size` rows from from the leaf into m_storage. If it's an integer
            // leaf, then it contains the method get_chunk() which copies these values in a super fast way (first
            // case of the `if` below. Otherwise, copy the values one by one in a for-loop (the `else` case).
            if (std::is_same<U, int64_t>::value && index + ValueBase::default_size <= sgc->m_leaf_end) {
                Value<int64_t> v;

                // If you want to modify 'default_size' then update Array::get_chunk()
                REALM_ASSERT_3(ValueBase::default_size, ==, 8);

                auto sgc_2 = static_cast<SequentialGetter<ColType>*>(m_sg.get());
                sgc_2->m_leaf_ptr->get_chunk(index - sgc->m_leaf_start, v.m_storage.m_first);

                destination.import(v);
            }
            else {
                size_t rows = colsize - index;
                if (rows > ValueBase::default_size)
                    rows = ValueBase::default_size;
                Value<typename util::RemoveOptional<U>::type> v(false, rows);

                for (size_t t = 0; t < rows; t++)
                    v.m_storage.set(t, sgc->get_next(index + t));

                destination.import(v);
            }
        }
    }

    virtual std::string description(util::serializer::SerialisationState& state) const override
    {
        return state.describe_columns(m_link_map, m_column_ndx);
    }

    // Load values from Column into destination
    void evaluate(size_t index, ValueBase& destination) override
    {
        if (m_nullable && std::is_same<typename ColType::value_type, int64_t>::value) {
            evaluate_internal<IntNullColumn>(index, destination);
        }
        else {
            evaluate_internal<ColType>(index, destination);
        }
    }

    bool links_exist() const
    {
        return m_link_map.m_link_columns.size() > 0;
    }

    bool is_nullable() const
    {
        return m_nullable;
    }

    size_t column_ndx() const noexcept
    {
        return m_sg ? get_column_base().get_column_index() : m_column_ndx;
    }

private:
    LinkMap m_link_map;

    // Fast (leaf caching) value getter for payload column (column in table on which query condition is executed)
    std::unique_ptr<SequentialGetterBase> m_sg;

    // Column index of payload column of m_table
    size_t m_column_ndx;

    // set to false by default for stand-alone Columns declaration that are not yet associated with any table
    // or oclumn. Call init() to update it or use a constructor that takes table + column index as argument.
    bool m_nullable = false;

    const ColumnBase& get_column_base() const noexcept
    {
        if (m_nullable && std::is_same<int64_t, T>::value)
            return *static_cast<SequentialGetter<IntNullColumn>&>(*m_sg).m_column;
        else
            return *static_cast<SequentialGetter<ColType>&>(*m_sg).m_column;
    }
};

template <typename T, typename Operation>
class SubColumnAggregate;

template <typename T>
class SubColumns : public Subexpr {
public:
    SubColumns(Columns<T> column, LinkMap link_map)
        : m_column(std::move(column))
        , m_link_map(std::move(link_map))
    {
    }

    std::unique_ptr<Subexpr> clone(QueryNodeHandoverPatches*) const override
    {
        return make_subexpr<SubColumns<T>>(*this);
    }

    const Table* get_base_table() const override
    {
        return m_link_map.base_table();
    }

    void set_base_table(const Table* table) override
    {
        m_link_map.set_base_table(table);
        m_column.set_base_table(m_link_map.target_table());
    }

    void verify_column() const override
    {
        m_link_map.verify_columns();
        m_column.verify_column();
    }

    void evaluate(size_t, ValueBase&) override
    {
        // SubColumns can only be used in an expression in conjunction with its aggregate methods.
        REALM_ASSERT(false);
    }

    virtual std::string description(util::serializer::SerialisationState&) const override
    {
        return ""; // by itself there are no conditions, see SubColumnAggregate
    }

    SubColumnAggregate<T, aggregate_operations::Minimum<T>> min() const
    {
        return {m_column, m_link_map};
    }

    SubColumnAggregate<T, aggregate_operations::Maximum<T>> max() const
    {
        return {m_column, m_link_map};
    }

    SubColumnAggregate<T, aggregate_operations::Sum<T>> sum() const
    {
        return {m_column, m_link_map};
    }

    SubColumnAggregate<T, aggregate_operations::Average<T>> average() const
    {
        return {m_column, m_link_map};
    }

private:
    Columns<T> m_column;
    LinkMap m_link_map;
};

template <typename T, typename Operation>
class SubColumnAggregate : public Subexpr2<typename Operation::ResultType> {
public:
    SubColumnAggregate(Columns<T> column, LinkMap link_map)
        : m_column(std::move(column))
        , m_link_map(std::move(link_map))
    {
    }
    SubColumnAggregate(SubColumnAggregate const& other, QueryNodeHandoverPatches* patches)
        : m_column(other.m_column, patches)
        , m_link_map(other.m_link_map, patches)
    {
    }

    std::unique_ptr<Subexpr> clone(QueryNodeHandoverPatches* patches) const override
    {
        return make_subexpr<SubColumnAggregate>(*this, patches);
    }

    const Table* get_base_table() const override
    {
        return m_link_map.base_table();
    }

    void set_base_table(const Table* table) override
    {
        m_link_map.set_base_table(table);
        m_column.set_base_table(m_link_map.target_table());
    }

    void verify_column() const override
    {
        m_link_map.verify_columns();
        m_column.verify_column();
    }

    void evaluate(size_t index, ValueBase& destination) override
    {
        std::vector<size_t> links = m_link_map.get_links(index);
        std::sort(links.begin(), links.end());

        Operation op;
        for (size_t link_index = 0; link_index < links.size();) {
            Value<T> value;
            size_t link = links[link_index];
            m_column.evaluate(link, value);

            // Columns<T>::evaluate fetches values in chunks of ValueBase::default_size. Process all values
            // within the chunk that came from rows that we link to.
            const auto& value_storage = value.m_storage;
            for (size_t value_index = 0; value_index < value.m_values;) {
                if (!value_storage.is_null(value_index)) {
                    op.accumulate(value_storage[value_index]);
                }
                if (++link_index >= links.size()) {
                    break;
                }

                size_t previous_link = link;
                link = links[link_index];
                value_index += link - previous_link;
            }
        }
        if (op.is_null()) {
            destination.import(Value<null>(false, 1, null()));
        }
        else {
            destination.import(Value<typename Operation::ResultType>(false, 1, op.result()));
        }
    }

    virtual std::string description(util::serializer::SerialisationState& state) const override
    {
        util::serializer::SerialisationState empty_state;
        return state.describe_columns(m_link_map, realm::npos) + util::serializer::value_separator + Operation::description() + util::serializer::value_separator + m_column.description(empty_state);
    }

private:
    Columns<T> m_column;
    LinkMap m_link_map;
};

struct SubQueryCountHandoverPatch : QueryNodeHandoverPatch {
    QueryHandoverPatch m_query;
};

class SubQueryCount : public Subexpr2<Int> {
public:
    SubQueryCount(Query q, LinkMap link_map)
        : m_query(std::move(q))
        , m_link_map(std::move(link_map))
    {
    }

    const Table* get_base_table() const override
    {
        return m_link_map.base_table();
    }

    void set_base_table(const Table* table) override
    {
        m_link_map.set_base_table(table);
    }

    void verify_column() const override
    {
        m_link_map.verify_columns();
    }

    void evaluate(size_t index, ValueBase& destination) override
    {
        std::vector<size_t> links = m_link_map.get_links(index);
        std::sort(links.begin(), links.end());

        size_t count = std::accumulate(links.begin(), links.end(), size_t(0), [this](size_t running_count, size_t link) {
            return running_count + m_query.count(link, link + 1, 1);
        });

        destination.import(Value<Int>(false, 1, size_t(count)));
    }

    virtual std::string description(util::serializer::SerialisationState& state) const override
    {
        REALM_ASSERT(m_link_map.base_table() != nullptr);
        std::string target = state.describe_columns(m_link_map, realm::npos);
        std::string var_name = state.get_variable_name(m_link_map.base_table()->get_table_ref());
        state.subquery_prefix_list.push_back(var_name);
        std::string desc = "SUBQUERY(" + target + ", " + var_name + ", " + m_query.get_description(state) + ")"
            + util::serializer::value_separator + "@count";
        state.subquery_prefix_list.pop_back();
        return desc;
    }

    std::unique_ptr<Subexpr> clone(QueryNodeHandoverPatches* patches) const override
    {
        if (patches)
            return std::unique_ptr<Subexpr>(new SubQueryCount(*this, patches));

        return make_subexpr<SubQueryCount>(*this);
    }

    void apply_handover_patch(QueryNodeHandoverPatches& patches, Group& group) override
    {
        REALM_ASSERT(patches.size());
        std::unique_ptr<QueryNodeHandoverPatch> abstract_patch = std::move(patches.back());
        patches.pop_back();

        auto patch = dynamic_cast<SubQueryCountHandoverPatch*>(abstract_patch.get());
        REALM_ASSERT(patch);

        m_query.apply_patch(patch->m_query, group);
    }

private:
    SubQueryCount(const SubQueryCount& other, QueryNodeHandoverPatches* patches)
        : m_link_map(other.m_link_map, patches)
    {
        std::unique_ptr<SubQueryCountHandoverPatch> patch(new SubQueryCountHandoverPatch);
        m_query = Query(other.m_query, patch->m_query, ConstSourcePayload::Copy);
        patches->emplace_back(patch.release());
    }

    Query m_query;
    LinkMap m_link_map;
};

// The unused template parameter is a hack to avoid a circular dependency between table.hpp and query_expression.hpp.
template <class>
class SubQuery {
public:
    SubQuery(Columns<Link> link_column, Query query)
        : m_query(std::move(query))
        , m_link_map(link_column.link_map())
    {
        REALM_ASSERT(m_link_map.target_table() == m_query.get_table());
    }

    SubQueryCount count() const
    {
        return SubQueryCount(m_query, m_link_map);
    }

private:
    Query m_query;
    LinkMap m_link_map;
};

namespace aggregate_operations {
template <typename T, typename Derived, typename R = T>
class BaseAggregateOperation {
    static_assert(std::is_same<T, Int>::value || std::is_same<T, Float>::value || std::is_same<T, Double>::value,
                  "Numeric aggregates can only be used with subcolumns of numeric types");

public:
    using ResultType = R;

    void accumulate(T value)
    {
        m_count++;
        m_result = Derived::apply(m_result, value);
    }

    bool is_null() const
    {
        return m_count == 0;
    }
    ResultType result() const
    {
        return m_result;
    }

protected:
    size_t m_count = 0;
    ResultType m_result = Derived::initial_value();
};

template <typename T>
class Minimum : public BaseAggregateOperation<T, Minimum<T>> {
public:
    static T initial_value()
    {
        return std::numeric_limits<T>::max();
    }
    static T apply(T a, T b)
    {
        return std::min(a, b);
    }
    static std::string description()
    {
        return "@min";
    }
};

template <typename T>
class Maximum : public BaseAggregateOperation<T, Maximum<T>> {
public:
    static T initial_value()
    {
        return std::numeric_limits<T>::min();
    }
    static T apply(T a, T b)
    {
        return std::max(a, b);
    }
    static std::string description()
    {
        return "@max";
    }
};

template <typename T>
class Sum : public BaseAggregateOperation<T, Sum<T>> {
public:
    static T initial_value()
    {
        return T();
    }
    static T apply(T a, T b)
    {
        return a + b;
    }
    bool is_null() const
    {
        return false;
    }
    static std::string description()
    {
        return "@sum";
    }
};

template <typename T>
class Average : public BaseAggregateOperation<T, Average<T>, double> {
    using Base = BaseAggregateOperation<T, Average<T>, double>;

public:
    static double initial_value()
    {
        return 0;
    }
    static double apply(double a, T b)
    {
        return a + b;
    }
    double result() const
    {
        return Base::m_result / Base::m_count;
    }
    static std::string description()
    {
        return "@avg";
    }

};
}

template <class oper, class TLeft>
class UnaryOperator : public Subexpr2<typename oper::type> {
public:
    UnaryOperator(std::unique_ptr<TLeft> left)
        : m_left(std::move(left))
    {
    }

    UnaryOperator(const UnaryOperator& other, QueryNodeHandoverPatches* patches)
        : m_left(other.m_left->clone(patches))
    {
    }

    UnaryOperator& operator=(const UnaryOperator& other)
    {
        if (this != &other) {
            m_left = other.m_left->clone();
        }
        return *this;
    }

    UnaryOperator(UnaryOperator&&) = default;
    UnaryOperator& operator=(UnaryOperator&&) = default;

    // See comment in base class
    void set_base_table(const Table* table) override
    {
        m_left->set_base_table(table);
    }

    void verify_column() const override
    {
        m_left->verify_column();
    }

    // Recursively fetch tables of columns in expression tree. Used when user first builds a stand-alone expression
    // and binds it to a Query at a later time
    const Table* get_base_table() const override
    {
        return m_left->get_base_table();
    }

    // destination = operator(left)
    void evaluate(size_t index, ValueBase& destination) override
    {
        Value<T> result;
        Value<T> left;
        m_left->evaluate(index, left);
        result.template fun<oper>(&left);
        destination.import(result);
    }

    virtual std::string description(util::serializer::SerialisationState& state) const override
    {
        if (m_left) {
            return m_left->description(state);
        }
        return "";
    }

    std::unique_ptr<Subexpr> clone(QueryNodeHandoverPatches* patches) const override
    {
        return make_subexpr<UnaryOperator>(*this, patches);
    }

    void apply_handover_patch(QueryNodeHandoverPatches& patches, Group& group) override
    {
        m_left->apply_handover_patch(patches, group);
    }

private:
    typedef typename oper::type T;
    std::unique_ptr<TLeft> m_left;
};


template <class oper, class TLeft, class TRight>
class Operator : public Subexpr2<typename oper::type> {
public:
    Operator(std::unique_ptr<TLeft> left, std::unique_ptr<TRight> right)
        : m_left(std::move(left))
        , m_right(std::move(right))
    {
    }

    Operator(const Operator& other, QueryNodeHandoverPatches* patches)
        : m_left(other.m_left->clone(patches))
        , m_right(other.m_right->clone(patches))
    {
    }

    Operator& operator=(const Operator& other)
    {
        if (this != &other) {
            m_left = other.m_left->clone();
            m_right = other.m_right->clone();
        }
        return *this;
    }

    Operator(Operator&&) = default;
    Operator& operator=(Operator&&) = default;

    // See comment in base class
    void set_base_table(const Table* table) override
    {
        m_left->set_base_table(table);
        m_right->set_base_table(table);
    }

    void verify_column() const override
    {
        m_left->verify_column();
        m_right->verify_column();
    }

    // Recursively fetch tables of columns in expression tree. Used when user first builds a stand-alone expression
    // and
    // binds it to a Query at a later time
    const Table* get_base_table() const override
    {
        const Table* l = m_left->get_base_table();
        const Table* r = m_right->get_base_table();

        // Queries do not support multiple different tables; all tables must be the same.
        REALM_ASSERT(l == nullptr || r == nullptr || l == r);

        // nullptr pointer means expression which isn't yet associated with any table, or is a Value<T>
        return l ? l : r;
    }

    // destination = operator(left, right)
    void evaluate(size_t index, ValueBase& destination) override
    {
        Value<T> result;
        Value<T> left;
        Value<T> right;
        m_left->evaluate(index, left);
        m_right->evaluate(index, right);
        result.template fun<oper>(&left, &right);
        destination.import(result);
    }

    virtual std::string description(util::serializer::SerialisationState& state) const override
    {
        std::string s;
        if (m_left) {
            s += m_left->description(state);
        }
        s += (" " + oper::description() + " ");
        if (m_right) {
            s += m_right->description(state);
        }
        return s;
    }

    std::unique_ptr<Subexpr> clone(QueryNodeHandoverPatches* patches) const override
    {
        return make_subexpr<Operator>(*this, patches);
    }

    void apply_handover_patch(QueryNodeHandoverPatches& patches, Group& group) override
    {
        m_right->apply_handover_patch(patches, group);
        m_left->apply_handover_patch(patches, group);
    }

private:
    typedef typename oper::type T;
    std::unique_ptr<TLeft> m_left;
    std::unique_ptr<TRight> m_right;
};


template <class TCond, class T, class TLeft, class TRight>
class Compare : public Expression {
public:
    Compare(std::unique_ptr<TLeft> left, std::unique_ptr<TRight> right)
        : m_left(std::move(left))
        , m_right(std::move(right))
    {
    }

    // See comment in base class
    void set_base_table(const Table* table) override
    {
        m_left->set_base_table(table);
        m_right->set_base_table(table);
    }

    void verify_column() const override
    {
        m_left->verify_column();
        m_right->verify_column();
    }

    // Recursively fetch tables of columns in expression tree. Used when user first builds a stand-alone expression
    // and
    // binds it to a Query at a later time
    const Table* get_base_table() const override
    {
        const Table* l = m_left->get_base_table();
        const Table* r = m_right->get_base_table();

        // All main tables in each subexpression of a query (table.columns() or table.link()) must be the same.
        REALM_ASSERT(l == nullptr || r == nullptr || l == r);

        // nullptr pointer means expression which isn't yet associated with any table, or is a Value<T>
        return l ? l : r;
    }

    size_t find_first(size_t start, size_t end) const override
    {
        size_t match;
        Value<T> right;
        Value<T> left;

        for (; start < end;) {
            m_left->evaluate(start, left);
            m_right->evaluate(start, right);
            match = Value<T>::template compare<TCond>(&left, &right);

            if (match != not_found && match + start < end)
                return start + match;

            size_t rows =
                (left.m_from_link_list || right.m_from_link_list) ? 1 : minimum(right.m_values, left.m_values);
            start += rows;
        }

        return not_found; // no match
    }

    virtual std::string description(util::serializer::SerialisationState& state) const override
    {
        if (std::is_same<TCond, BeginsWith>::value
            || std::is_same<TCond, BeginsWithIns>::value
            || std::is_same<TCond, EndsWith>::value
            || std::is_same<TCond, EndsWithIns>::value
            || std::is_same<TCond, Contains>::value
            || std::is_same<TCond, ContainsIns>::value
            || std::is_same<TCond, Like>::value
            || std::is_same<TCond, LikeIns>::value) {
            // these string conditions have the arguments reversed but the order is important
            // operations ==, and != can be reversed because the produce the same results both ways
            return util::serializer::print_value(m_right->description(state) + " " + TCond::description()
                                                 + " " + m_left->description(state));
        }
        return util::serializer::print_value(m_left->description(state) + " " + TCond::description()
                                             + " " + m_right->description(state));
    }

    std::unique_ptr<Expression> clone(QueryNodeHandoverPatches* patches) const override
    {
        return std::unique_ptr<Expression>(new Compare(*this, patches));
    }

    void apply_handover_patch(QueryNodeHandoverPatches& patches, Group& group) override
    {
        m_right->apply_handover_patch(patches, group);
        m_left->apply_handover_patch(patches, group);
    }

private:
    Compare(const Compare& other, QueryNodeHandoverPatches* patches)
        : m_left(other.m_left->clone(patches))
        , m_right(other.m_right->clone(patches))
    {
    }

    std::unique_ptr<TLeft> m_left;
    std::unique_ptr<TRight> m_right;
};
}
#endif // REALM_QUERY_EXPRESSION_HPP
