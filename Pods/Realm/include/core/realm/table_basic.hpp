/*************************************************************************
 *
 * REALM CONFIDENTIAL
 * __________________
 *
 *  [2011] - [2015] Realm Inc
 *  All Rights Reserved.
 *
 * NOTICE:  All information contained herein is, and remains
 * the property of Realm Incorporated and its suppliers,
 * if any.  The intellectual and technical concepts contained
 * herein are proprietary to Realm Incorporated
 * and its suppliers and may be covered by U.S. and Foreign Patents,
 * patents in process, and are protected by trade secret or copyright law.
 * Dissemination of this information or reproduction of this material
 * is strictly forbidden unless prior written permission is obtained
 * from Realm Incorporated.
 *
 **************************************************************************/
#ifndef REALM_TABLE_BASIC_HPP
#define REALM_TABLE_BASIC_HPP

#include <stdint.h> // unint8_t etc
#include <cstddef>
#include <utility>

#include <realm/util/meta.hpp>
#include <realm/util/tuple.hpp>
#include <realm/table.hpp>
#include <realm/descriptor.hpp>
#include <realm/query.hpp>
#include <realm/table_accessors.hpp>
#include <realm/table_view_basic.hpp>

namespace realm {


namespace _impl {

template<class Type, int col_idx>
struct AddCol;
template<class Type, int col_idx>
struct CmpColType;
template<class Type, int col_idx>
struct AssignIntoCol;

} // namespace _impl



/// This class is non-polymorphic, that is, it has no virtual
/// functions. Further more, it has no destructor, and it adds no new
/// data-members. These properties are important, because it ensures
/// that there is no run-time distinction between a Table instance and
/// an instance of any variation of this class, and therefore it is
/// valid to cast a pointer from Table to BasicTable<Spec> even when
/// the instance is constructed as a Table. Of couse, this also
/// assumes that Table is non-polymorphic. Further more, accessing the
/// Table via a pointer or reference to a BasicTable is not in
/// violation of the strict aliasing rule.
template<class Spec>
class BasicTable: private Table, public Spec::ConvenienceMethods {
public:
    typedef Spec spec_type;
    typedef typename Spec::Columns Columns;

    typedef BasicTableRef<BasicTable> Ref;
    typedef BasicTableRef<const BasicTable> ConstRef;

    typedef BasicTableView<BasicTable> View;
    typedef BasicTableView<const BasicTable> ConstView;

    using Table::is_attached;
    using Table::has_shared_type;
    using Table::is_empty;
    using Table::size;
    using Table::clear;
    using Table::remove;
    using Table::remove_last;
    using Table::move_last_over;
    using Table::optimize;
    using Table::add_empty_row;
    using Table::insert_empty_row;
    using Table::aggregate;

    using Table::get_backlink_count;
    using Table::get_backlink;

    using Table::is_group_level;
    using Table::get_index_in_group;

    BasicTable(Allocator& alloc = Allocator::get_default()): Table(alloc)
    {
        set_dynamic_type(*this);
    }

    BasicTable(const BasicTable& t, Allocator& alloc = Allocator::get_default()): Table(t, alloc) {}

    ~BasicTable() noexcept {}

    static Ref create(Allocator& = Allocator::get_default());

    Ref copy(Allocator& = Allocator::get_default()) const;

    static int get_column_count() noexcept
    {
        return util::TypeCount<typename Spec::Columns>::value;
    }

    Ref get_table_ref() { return Ref(this); }

    ConstRef get_table_ref() const { return ConstRef(this); }

private:
    template<int col_idx>
    struct Col {
        typedef typename util::TypeAt<typename Spec::Columns, col_idx>::type value_type;
        typedef _impl::ColumnAccessor<BasicTable, col_idx, value_type> type;
    };
    typedef typename Spec::template ColNames<Col, BasicTable*> ColsAccessor;

    template<int col_idx>
    struct ConstCol {
        typedef typename util::TypeAt<typename Spec::Columns, col_idx>::type value_type;
        typedef _impl::ColumnAccessor<const BasicTable, col_idx, value_type> type;
    };
    typedef typename Spec::template ColNames<ConstCol, const BasicTable*> ConstColsAccessor;

public:
    ColsAccessor column() noexcept { return ColsAccessor(this); }
    ConstColsAccessor column() const noexcept { return ConstColsAccessor(this); }

private:
    template<int col_idx>
    struct Field {
        typedef typename util::TypeAt<typename Spec::Columns, col_idx>::type value_type;
        typedef _impl::FieldAccessor<BasicTable, col_idx, value_type, false> type;
    };
    typedef std::pair<BasicTable*, size_t> FieldInit;

    template<int col_idx>
    struct ConstField {
        typedef typename util::TypeAt<typename Spec::Columns, col_idx>::type value_type;
        typedef _impl::FieldAccessor<const BasicTable, col_idx, value_type, true> type;
    };
    typedef std::pair<const BasicTable*, size_t> ConstFieldInit;

public:
    typedef typename Spec::template ColNames<Field, FieldInit> RowAccessor;
    typedef typename Spec::template ColNames<ConstField, ConstFieldInit> ConstRowAccessor;

    RowAccessor operator[](size_t row_idx) noexcept
    {
        return RowAccessor(std::make_pair(this, row_idx));
    }

    ConstRowAccessor operator[](size_t row_idx) const noexcept
    {
        return ConstRowAccessor(std::make_pair(this, row_idx));
    }

    RowAccessor front() noexcept
    {
        return RowAccessor(std::make_pair(this, 0));
    }

    ConstRowAccessor front() const noexcept
    {
        return ConstRowAccessor(std::make_pair(this, 0));
    }

    /// Access the last row, or one of its predecessors.
    ///
    /// \param rel_idx An optional index of the row specified relative
    /// to the end. Thus, <tt>table.back(rel_idx)</tt> is the same as
    /// <tt>table[table.size() + rel_idx]</tt>.
    ///
    RowAccessor back(int rel_idx = -1) noexcept
    {
        return RowAccessor(std::make_pair(this, size()+rel_idx));
    }

    ConstRowAccessor back(int rel_idx = -1) const noexcept
    {
        return ConstRowAccessor(std::make_pair(this, size()+rel_idx));
    }

    RowAccessor add() { return RowAccessor(std::make_pair(this, add_empty_row())); }

    template<class L>
    void add(const util::Tuple<L>& tuple)
    {
        size_t i = size();
        insert(i, tuple);
    }

    void insert(size_t i) { insert_empty_row(i); }

    template<class L>
    void insert(size_t i, const util::Tuple<L>& tuple)
    {
        insert(i);
        set(i, tuple);
    }

    template<class L>
    void set(size_t i, const util::Tuple<L>& tuple)
    {
        using namespace realm::util;
        static_assert(TypeCount<L>::value == TypeCount<Columns>::value,
                      "Wrong number of tuple elements");
        ForEachType<Columns, _impl::AssignIntoCol>::exec(static_cast<Table*>(this), i, tuple);
    }

    // FIXME: This probably fails if Spec::ConvenienceMethods has no add().
    using Spec::ConvenienceMethods::add;
    // FIXME: This probably fails if Spec::ConvenienceMethods has no insert().
    using Spec::ConvenienceMethods::insert;
    // FIXME: This probably fails if Spec::ConvenienceMethods has no set().
    using Spec::ConvenienceMethods::set;

    // FIXME: A cursor must be a distinct class that can be constructed from a RowAccessor
    typedef RowAccessor Cursor;
    typedef ConstRowAccessor ConstCursor;


    class Query;
    Query       where(typename BasicTable<Spec>::View* tv = nullptr) { return Query(*this, tv ? tv->get_impl() : nullptr); }
    Query where(typename BasicTable<Spec>::View* tv = nullptr) const { return Query(*this, tv ? tv->get_impl() : nullptr); }

    /// Compare two tables for equality. Two tables are equal if, and
    /// only if, they contain the same rows in the same order, that
    /// is, for each value V at column index C and row index R in one
    /// of the tables, there is a value at column index C and row
    /// index R in the other table that is equal to V.
    bool operator==(const BasicTable& t) const { return compare_rows(t); }

    /// Compare two tables for inequality. See operator==().
    bool operator!=(const BasicTable& t) const { return !compare_rows(t); }

    /// Checks whether the dynamic type of the specified table matches
    /// the statically specified table type. The two types (or specs)
    /// must have the same columns, and in the same order. Two columns
    /// are considered equal if, and only if they have the same name
    /// and the same type. The type is understood as the value encoded
    /// by the DataType enumeration. This check proceeds recursively
    /// for subtable columns.
    ///
    /// \tparam T The static table type. It makes no difference
    /// whether it is const-qualified or not.
    ///
    /// FIXME: Consider dropping the requirement that column names
    /// must be equal. There does not seem to be any value for the
    /// user in that requirement. Further more, there may be cases
    /// where it is desirable to be able to cast to a table type with
    /// different column names. Similar changes are needed in the Java
    /// and Objective-C language bindings.
    template<class T>
    friend bool is_a(const Table&) noexcept;

    //@{
    /// These functions return null if the specified table is not
    /// compatible with the specified table type.
    template<class T>
    friend BasicTableRef<T> checked_cast(TableRef) noexcept;

    template<class T>
    friend BasicTableRef<const T> checked_cast(ConstTableRef) noexcept;
    //@}

    using Table::verify;

#ifdef REALM_DEBUG
    using Table::print;
    using Table::dump_node_structure;
#endif

private:
    template<int col_idx>
    struct QueryCol {
        typedef typename util::TypeAt<typename Spec::Columns, col_idx>::type value_type;
        typedef _impl::QueryColumn<BasicTable, col_idx, value_type> type;
    };

    // These are intende to be used only by accessor classes
    Table* get_impl() noexcept { return this; }
    const Table* get_impl() const noexcept { return this; }

    template<class Subtab>
    Subtab* get_subtable_ptr(size_t col_idx, size_t row_idx)
    {
        return static_cast<Subtab*>(Table::get_subtable_ptr(col_idx, row_idx));
    }

    template<class Subtab>
    const Subtab* get_subtable_ptr(size_t col_idx, size_t row_idx) const
    {
        return static_cast<const Subtab*>(Table::get_subtable_ptr(col_idx, row_idx));
    }

    static void set_dynamic_type(Table& table)
    {
        using namespace realm::util;
        DescriptorRef desc = table.get_descriptor(); // Throws
        const int num_cols = TypeCount<typename Spec::Columns>::value;
        StringData dyn_col_names[num_cols];
        Spec::dyn_col_names(dyn_col_names);
        ForEachType<typename Spec::Columns, _impl::AddCol>::exec(&*desc, dyn_col_names); // Throws
    }

    static bool matches_dynamic_type(const realm::Spec& spec) noexcept
    {
        using namespace realm::util;
        const int num_cols = util::TypeCount<typename Spec::Columns>::value;
        StringData dyn_col_names[num_cols];
        Spec::dyn_col_names(dyn_col_names);
        return !HasType<typename Spec::Columns,
                        _impl::CmpColType>::exec(&spec, dyn_col_names);
    }

    // This one allows a BasicTable to know that BasicTables with
    // other Specs are also derived from Table.
    template<class>
    friend class BasicTable;

    // This one allows util::bind_ptr to know that all BasicTable template
    // instantiations are derived from Table.
    template<class>
    friend class util::bind_ptr;

    // These allow BasicTableRef to refer to RowAccessor and
    // ConstRowAccessor.
    friend class BasicTableRef<BasicTable>;
    friend class BasicTableRef<const BasicTable>;

    // These allow BasicTableView to call get_subtable_ptr().
    friend class BasicTableView<BasicTable>;
    friend class BasicTableView<const BasicTable>;

    template<class, int>
    friend struct _impl::CmpColType;

    template<class, int, class, bool>
    friend class _impl::FieldAccessor;

    template<class, int, class>
    friend class _impl::MixedFieldAccessorBase;

    template<class, int, class>
    friend class _impl::ColumnAccessorBase;

    template<class, int, class>
    friend class _impl::ColumnAccessor;

    template<class, int, class>
    friend class _impl::QueryColumn;

    friend class Group;
};


#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4355)
#endif

template<class Spec>
class BasicTable<Spec>::Query:
        public Spec::template ColNames<QueryCol, Query*> {
public:
    Query(const Query& q): Spec::template ColNames<QueryCol, Query*>(this), m_impl(q.m_impl) {}
    virtual ~Query() noexcept {}

    Query& group() { m_impl.group(); return *this; }

    Query& end_group() { m_impl.end_group(); return *this; }

    Query& end_subtable() { m_impl.end_subtable(); return *this; }

    Query& Or() { m_impl.Or(); return *this; }

    Query& Not() { m_impl.Not(); return *this; }

    size_t find(size_t begin_at_table_row = 0)
    {
        return m_impl.find(begin_at_table_row);
    }

    typename BasicTable<Spec>::View find_all(size_t start = 0,
                                             size_t end   = size_t(-1),
                                             size_t limit = size_t(-1))
    {
        return m_impl.find_all(start, end, limit);
    }

    typename BasicTable<Spec>::ConstView find_all(size_t start = 0,
                                                  size_t end   = size_t(-1),
                                                  size_t limit = size_t(-1)) const
    {
        return m_impl.find_all(start, end, limit);
    }

    size_t count(size_t start = 0,
                      size_t end   = size_t(-1),
                      size_t limit = size_t(-1)) const
    {
        return m_impl.count(start, end, limit);
    }

    size_t remove(size_t start = 0,
                       size_t end   = size_t(-1),
                       size_t limit = size_t(-1))
    {
        return m_impl.remove(start, end, limit);
    }

    std::string validate() { return m_impl.validate(); }

protected:
    Query(const BasicTable<Spec>& table, TableViewBase* tv):
        Spec::template ColNames<QueryCol, Query*>(this), m_impl(table, tv) {}

    using HandoverPatch = QueryHandoverPatch;
    Query(const Query& source, HandoverPatch& patch, ConstSourcePayload mode) :
        Spec::template ColNames<QueryCol, Query*>(this),
        m_impl(source.m_impl, patch, mode)
    {
    }

    Query(Query& source, HandoverPatch& patch, MutableSourcePayload mode) :
        Spec::template ColNames<QueryCol, Query*>(this),
        m_impl(source.m_impl, patch, mode)
    {
    }

    void apply_patch(HandoverPatch& patch, Group& group)
    {
        m_impl.apply_patch(patch, group);
    }

    virtual std::unique_ptr<Query>
    clone_for_handover(std::unique_ptr<HandoverPatch>& patch, ConstSourcePayload mode) const
    {
        patch.reset(new HandoverPatch);
        std::unique_ptr<Query> retval( new Query(*this, *patch, mode));
        return retval;
    }

    virtual std::unique_ptr<Query>
    clone_for_handover(std::unique_ptr<HandoverPatch>& patch, MutableSourcePayload mode)
    {
        patch.reset(new HandoverPatch);
        std::unique_ptr<Query> retval( new Query(*this, *patch, mode));
        return retval;
    }

    virtual void apply_and_consume_patch(std::unique_ptr<HandoverPatch>& patch, Group& group)
    {
        apply_patch(*patch, group);
        patch.reset();
    }

private:
    realm::Query m_impl;

    friend class BasicTable;
    friend class SharedGroup;

    template<class, int, class>
    friend class _impl::QueryColumnBase;

    template<class, int, class>
    friend class _impl::QueryColumn;
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif




// Implementation:

namespace _impl {

template<class T>
struct GetColumnTypeId;

template<>
struct GetColumnTypeId<int64_t>
{
    static const DataType id = type_Int;
};
template<class E>
struct GetColumnTypeId<SpecBase::Enum<E>> {
    static const DataType id = type_Int;
};
template<>
struct GetColumnTypeId<bool>
{
    static const DataType id = type_Bool;
};
template<>
struct GetColumnTypeId<float>
{
    static const DataType id = type_Float;
};
template<>
struct GetColumnTypeId<double>
{
    static const DataType id = type_Double;
};
template<>
struct GetColumnTypeId<StringData>
{
    static const DataType id = type_String;
};
template<>
struct GetColumnTypeId<BinaryData>
{
    static const DataType id = type_Binary;
};
template<>
struct GetColumnTypeId<OldDateTime>
{
    static const DataType id = type_OldDateTime;
};
template<>
struct GetColumnTypeId<Mixed>
{
    static const DataType id = type_Mixed;
};
template<>
struct GetColumnTypeId<Timestamp>
{
        static const DataType id = type_Timestamp;
};

template<class Type, int col_idx>
struct AddCol {
    static void exec(Descriptor* desc, const StringData* col_names)
    {
        REALM_ASSERT(col_idx == desc->get_column_count());
        desc->add_column(GetColumnTypeId<Type>::id, col_names[col_idx]); // Throws
    }
};

// AddCol specialization for subtables
template<class Subtab, int col_idx>
struct AddCol<SpecBase::Subtable<Subtab>, col_idx> {
    static void exec(Descriptor* desc, const StringData* col_names)
    {
        REALM_ASSERT(col_idx == desc->get_column_count());
        DescriptorRef subdesc;
        desc->add_column(type_Table, col_names[col_idx], &subdesc); // Throws
        using namespace realm::util;
        const int num_subcols = TypeCount<typename Subtab::spec_type::Columns>::value;
        StringData subcol_names[num_subcols];
        Subtab::spec_type::dyn_col_names(subcol_names);
        typedef typename Subtab::Columns Subcolumns;
        ForEachType<Subcolumns, _impl::AddCol>::exec(&*subdesc, subcol_names); // Throws
    }
};



template<class Type, int col_idx>
struct CmpColType {
    static bool exec(const Spec* spec, const StringData* col_names)
    {
        return GetColumnTypeId<Type>::id != spec->get_public_column_type(col_idx) ||
            col_names[col_idx] != spec->get_column_name(col_idx);
    }
};

// CmpColType specialization for subtables
template<class Subtab, int col_idx>
struct CmpColType<SpecBase::Subtable<Subtab>, col_idx> {
    static bool exec(const Spec* spec, const StringData* col_names)
    {
        if (spec->get_column_type(col_idx) != col_type_Table ||
            col_names[col_idx] != spec->get_column_name(col_idx)) return true;
        const Spec subspec = const_cast<Spec*>(spec)->get_subtable_spec(col_idx);
        return !Subtab::matches_dynamic_type(subspec);
    }
};


// AssignIntoCol specialization for integers
template<int col_idx>
struct AssignIntoCol<int64_t, col_idx> {
    template<class L>
    static void exec(Table* t, size_t row_idx, util::Tuple<L> tuple)
    {
        t->set_int(col_idx, row_idx, util::at<col_idx>(tuple));
    }
};

// AssignIntoCol specialization for floats
template<int col_idx>
struct AssignIntoCol<float, col_idx> {
    template<class L>
    static void exec(Table* t, size_t row_idx, util::Tuple<L> tuple)
    {
        t->set_float(col_idx, row_idx, util::at<col_idx>(tuple));
    }
};

// AssignIntoCol specialization for doubles
template<int col_idx>
struct AssignIntoCol<double, col_idx> {
    template<class L>
    static void exec(Table* t, size_t row_idx, util::Tuple<L> tuple)
    {
        t->set_double(col_idx, row_idx, util::at<col_idx>(tuple));
    }
};

// AssignIntoCol specialization for booleans
template<int col_idx>
struct AssignIntoCol<bool, col_idx> {
    template<class L>
    static void exec(Table* t, size_t row_idx, util::Tuple<L> tuple)
    {
        t->set_bool(col_idx, row_idx, util::at<col_idx>(tuple));
    }
};

// AssignIntoCol specialization for strings
template<int col_idx>
struct AssignIntoCol<StringData, col_idx> {
    template<class L>
    static void exec(Table* t, size_t row_idx, util::Tuple<L> tuple)
    {
        t->set_string(col_idx, row_idx, util::at<col_idx>(tuple));
    }
};

// AssignIntoCol specialization for enumerations
template<class E, int col_idx>
struct AssignIntoCol<SpecBase::Enum<E>, col_idx> {
    template<class L>
    static void exec(Table* t, size_t row_idx, util::Tuple<L> tuple)
    {
        t->set_enum(col_idx, row_idx, util::at<col_idx>(tuple));
    }
};

// AssignIntoCol specialization for dates
template<int col_idx>
struct AssignIntoCol<OldDateTime, col_idx> {
    template<class L>
    static void exec(Table* t, size_t row_idx, util::Tuple<L> tuple)
    {
        t->set_olddatetime(col_idx, row_idx, util::at<col_idx>(tuple));
    }
};

// AssignIntoCol specialization for timestamps
template<int col_idx>
struct AssignIntoCol<Timestamp, col_idx> {
    template<class L>
    static void exec(Table* t, size_t row_idx, util::Tuple<L> tuple)
    {
        t->set_timestamp(col_idx, row_idx, util::at<col_idx>(tuple));
    }
};

// AssignIntoCol specialization for binary data
template<int col_idx>
struct AssignIntoCol<BinaryData, col_idx> {
    template<class L>
    static void exec(Table* t, size_t row_idx, util::Tuple<L> tuple)
    {
        t->set_binary(col_idx, row_idx, util::at<col_idx>(tuple));
    }
};

// AssignIntoCol specialization for subtables
template<class T, int col_idx>
struct AssignIntoCol<SpecBase::Subtable<T>, col_idx> {
    template<class L>
    static void exec(Table* t, size_t row_idx, util::Tuple<L> tuple)
    {
        // FIXME: unsafe reinterpret_cast to private base class
        auto subtable = reinterpret_cast<const Table*>(static_cast<const T*>(util::at<col_idx>(tuple)));
        t->set_subtable(col_idx, row_idx, subtable);
    }
};

// AssignIntoCol specialization for mixed type
template<int col_idx>
struct AssignIntoCol<Mixed, col_idx> {
    template<class L>
    static void exec(Table* t, size_t row_idx, util::Tuple<L> tuple)
    {
        t->set_mixed(col_idx, row_idx, util::at<col_idx>(tuple));
    }
};

} // namespace _impl


template<class Spec>
inline typename BasicTable<Spec>::Ref BasicTable<Spec>::create(Allocator& alloc)
{
    TableRef table = Table::create(alloc);
    set_dynamic_type(*table);
    return unchecked_cast<BasicTable<Spec>>(std::move(table));
}


template<class Spec>
inline typename BasicTable<Spec>::Ref BasicTable<Spec>::copy(Allocator& alloc) const
{
    return unchecked_cast<BasicTable<Spec>>(Table::copy(alloc));
}


template<class T>
inline bool is_a(const Table& t) noexcept
{
    typedef _impl::TableFriend tf;
    return T::matches_dynamic_type(tf::get_spec(t));
}


template<class T>
inline BasicTableRef<T> checked_cast(TableRef t) noexcept
{
    if (!is_a<T>(*t))
        return BasicTableRef<T>(); // Null
    return unchecked_cast<T>(t);
}


template<class T>
inline BasicTableRef<const T> checked_cast(ConstTableRef t) noexcept
{
    if (!is_a<T>(*t))
        return BasicTableRef<const T>(); // Null
    return unchecked_cast<T>(t);
}


} // namespace realm

#endif // REALM_TABLE_BASIC_HPP
