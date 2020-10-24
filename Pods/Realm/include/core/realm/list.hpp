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

#ifndef REALM_LIST_HPP
#define REALM_LIST_HPP

#include <realm/obj.hpp>
#include <realm/bplustree.hpp>
#include <realm/obj_list.hpp>
#include <realm/array_basic.hpp>
#include <realm/array_key.hpp>
#include <realm/array_bool.hpp>
#include <realm/array_string.hpp>
#include <realm/array_binary.hpp>
#include <realm/array_timestamp.hpp>

#ifdef _MSC_VER
#pragma warning(disable : 4250) // Suppress 'inherits ... via dominance' on MSVC
#endif

namespace realm {

class TableView;
class SortDescriptor;
class Group;

// To be used in query for size. Adds nullability to size so that
// it can be put in a NullableVector
struct SizeOfList {
    static constexpr size_t null_value = size_t(-1);

    SizeOfList(size_t s = null_value)
        : sz(s)
    {
    }
    bool is_null()
    {
        return sz == null_value;
    }
    void set_null()
    {
        sz = null_value;
    }
    size_t size() const
    {
        return sz;
    }
    size_t sz = null_value;
};

inline std::ostream& operator<<(std::ostream& ostr, SizeOfList size_of_list)
{
    if (size_of_list.is_null()) {
        ostr << "null";
    }
    else {
        ostr << size_of_list.sz;
    }
    return ostr;
}

class ConstLstBase : public ArrayParent {
public:
    ConstLstBase(ConstLstBase&&) = delete;
    virtual ~ConstLstBase();
    /*
     * Operations that makes sense without knowing the specific type
     * can be made virtual.
     */
    virtual size_t size() const = 0;
    virtual bool is_null(size_t ndx) const = 0;
    virtual Mixed get_any(size_t ndx) const = 0;

    virtual Mixed min(size_t* return_ndx = nullptr) const = 0;
    virtual Mixed max(size_t* return_ndx = nullptr) const = 0;
    virtual Mixed sum(size_t* return_cnt = nullptr) const = 0;
    virtual Mixed avg(size_t* return_cnt = nullptr) const = 0;

    // Modifies a vector of indices so that they refer to values sorted according
    // to the specified sort order
    virtual void sort(std::vector<size_t>& indices, bool ascending = true) const = 0;
    // Modifies a vector of indices so that they refer to distinct values.
    // If 'sort_order' is supplied, the indices will refer to values in sort order,
    // otherwise the indices will be in original order.
    virtual void distinct(std::vector<size_t>& indices, util::Optional<bool> sort_order = util::none) const = 0;

    bool is_empty() const
    {
        return size() == 0;
    }
    ObjKey get_key() const
    {
        return m_const_obj->get_key();
    }
    bool is_attached() const
    {
        return m_const_obj->is_valid();
    }
    bool has_changed() const
    {
        update_if_needed();
        if (m_last_content_version != m_content_version) {
            m_last_content_version = m_content_version;
            return true;
        }
        return false;
    }
    ConstTableRef get_table() const
    {
        return m_const_obj->get_table();
    }
    ColKey get_col_key() const
    {
        return m_col_key;
    }

    bool operator==(const ConstLstBase& other) const
    {
        return get_key() == other.get_key() && get_col_key() == other.get_col_key();
    }

protected:
    template <class>
    friend class LstIterator;
    friend class Transaction;


    const ConstObj* m_const_obj;
    ColKey m_col_key;
    bool m_nullable = false;

    mutable std::vector<size_t> m_deleted;
    mutable uint_fast64_t m_content_version = 0;
    mutable uint_fast64_t m_last_content_version = 0;

    ConstLstBase(ColKey col_key, const ConstObj* obj);
    virtual bool init_from_parent() const = 0;

    ref_type get_child_ref(size_t) const noexcept override;
    std::pair<ref_type, size_t> get_to_dot_parent(size_t) const override;

    void update_if_needed() const
    {
        auto content_version = m_const_obj->get_alloc().get_content_version();
        if (m_const_obj->update_if_needed() || content_version != m_content_version) {
            init_from_parent();
        }
    }
    void update_content_version() const
    {
        m_content_version = m_const_obj->get_alloc().get_content_version();
    }
    // Increase index by one. I we land on and index that is deleted, keep
    // increasing until we get to a valid entry.
    size_t incr(size_t ndx) const
    {
        ndx++;
        if (!m_deleted.empty()) {
            auto it = m_deleted.begin();
            auto end = m_deleted.end();
            while (it != end && *it < ndx) {
                ++it;
            }
            // If entry is deleted, increase further
            while (it != end && *it == ndx) {
                ++it;
                ++ndx;
            }
        }
        return ndx;
    }
    // Convert from virtual to real index
    size_t adjust(size_t ndx) const
    {
        if (!m_deleted.empty()) {
            // Optimized for the case where the iterator is past that last deleted entry
            auto it = m_deleted.rbegin();
            auto end = m_deleted.rend();
            while (it != end && *it >= ndx) {
                if (*it == ndx) {
                    throw std::out_of_range("Element was deleted");
                }
                ++it;
            }
            auto diff = end - it;
            ndx -= diff;
        }
        return ndx;
    }
    void adj_remove(size_t ndx)
    {
        auto it = m_deleted.begin();
        auto end = m_deleted.end();
        while (it != end && *it <= ndx) {
            ++ndx;
            ++it;
        }
        m_deleted.insert(it, ndx);
    }
    void erase_repl(Replication* repl, size_t ndx) const;
    void move_repl(Replication* repl, size_t from, size_t to) const;
    void swap_repl(Replication* repl, size_t ndx1, size_t ndx2) const;
    void clear_repl(Replication* repl) const;
};

/*
 * This class implements a forward iterator over the elements in a Lst.
 *
 * The iterator is stable against deletions in the list. If you try to
 * dereference an iterator that points to an element, that is deleted, the
 * call will throw.
 *
 * Values are read into a member variable (m_val). This is the only way to
 * implement operator-> and operator* returning a pointer and a reference resp.
 * There is no overhead compared to the alternative where operator* would have
 * to return T by value.
 */
template <class T>
class LstIterator {
public:
    typedef std::forward_iterator_tag iterator_category;
    typedef const T value_type;
    typedef ptrdiff_t difference_type;
    typedef const T* pointer;
    typedef const T& reference;

    LstIterator(const ConstLstIf<T>* l, size_t ndx)
        : m_list(l)
        , m_ndx(ndx)
    {
    }
    pointer operator->()
    {
        m_val = m_list->get(m_list->adjust(m_ndx));
        return &m_val;
    }
    reference operator*()
    {
        return *operator->();
    }
    LstIterator& operator++()
    {
        m_ndx = m_list->incr(m_ndx);
        return *this;
    }
    LstIterator operator++(int)
    {
        LstIterator tmp(*this);
        operator++();
        return tmp;
    }

    bool operator!=(const LstIterator& rhs)
    {
        return m_ndx != rhs.m_ndx;
    }

    bool operator==(const LstIterator& rhs)
    {
        return m_ndx == rhs.m_ndx;
    }

private:
    friend class Lst<T>;
    T m_val;
    const ConstLstIf<T>* m_list;
    size_t m_ndx;
};

template <class T>
inline void check_column_type(ColKey col)
{
    if (col && col.get_type() != ColumnTypeTraits<T>::column_id) {
        throw LogicError(LogicError::list_type_mismatch);
    }
}

template <>
inline void check_column_type<Int>(ColKey col)
{
    if (col && (col.get_type() != col_type_Int || col.get_attrs().test(col_attr_Nullable))) {
        throw LogicError(LogicError::list_type_mismatch);
    }
}

template <>
inline void check_column_type<util::Optional<Int>>(ColKey col)
{
    if (col && (col.get_type() != col_type_Int || !col.get_attrs().test(col_attr_Nullable))) {
        throw LogicError(LogicError::list_type_mismatch);
    }
}

template <>
inline void check_column_type<ObjKey>(ColKey col)
{
    if (col && col.get_type() != col_type_LinkList) {
        throw LogicError(LogicError::list_type_mismatch);
    }
}

/// This class defines the interface to ConstList, except for the constructor
/// The ConstList class has the ConstObj member m_obj, which should not be
/// inherited from Lst<T>.
template <class T>
class ConstLstIf : public virtual ConstLstBase {
public:
    /**
     * Only member functions not referring to an index in the list will check if
     * the object is up-to-date. The logic is that the user must always check the
     * size before referring to a particular index, and size() will check for update.
     */
    size_t size() const override
    {
        if (!is_attached())
            return 0;
        update_if_needed();
        if (!m_valid)
            return 0;

        return m_tree->size();
    }
    bool is_null(size_t ndx) const final
    {
        return m_nullable && get(ndx) == BPlusTree<T>::default_value(true);
    }
    Mixed get_any(size_t ndx) const final
    {
        return Mixed(get(ndx));
    }

    Mixed min(size_t* return_ndx = nullptr) const final;
    Mixed max(size_t* return_ndx = nullptr) const final;
    Mixed sum(size_t* return_cnt = nullptr) const final;
    Mixed avg(size_t* return_cnt = nullptr) const final;

    void sort(std::vector<size_t>& indices, bool ascending = true) const final;
    void distinct(std::vector<size_t>& indices, util::Optional<bool> sort_order = util::none) const final;

    T get(size_t ndx) const
    {
        if (ndx >= size()) {
            throw std::out_of_range("Index out of range");
        }
        return m_tree->get(ndx);
    }
    T operator[](size_t ndx) const
    {
        return get(ndx);
    }
    LstIterator<T> begin() const
    {
        return LstIterator<T>(this, 0);
    }
    LstIterator<T> end() const
    {
        return LstIterator<T>(this, size() + m_deleted.size());
    }
    size_t find_first(T value) const
    {
        if (!m_valid && !init_from_parent())
            return not_found;
        update_if_needed();
        return m_tree->find_first(value);
    }
    template <typename Func>
    void find_all(T value, Func&& func) const
    {
        if (m_valid && init_from_parent())
            m_tree->find_all(value, std::forward<Func>(func));
    }
    const BPlusTree<T>& get_tree() const
    {
        return *m_tree;
    }

protected:
    mutable std::unique_ptr<BPlusTree<T>> m_tree;
    mutable bool m_valid = false;

    ConstLstIf()
        : ConstLstBase(ColKey{}, nullptr)
    {
    }

    ConstLstIf(Allocator& alloc)
        : ConstLstBase(ColKey{}, nullptr)
        , m_tree(new BPlusTree<T>(alloc))
    {
        check_column_type<T>(m_col_key);

        m_tree->set_parent(this, 0); // ndx not used, implicit in m_owner
    }

    ConstLstIf(const ConstLstIf& other)
        : ConstLstBase(other.m_col_key, nullptr)
        , m_valid(other.m_valid)
    {
        if (other.m_tree) {
            Allocator& alloc = other.m_tree->get_alloc();
            m_tree = std::make_unique<BPlusTree<T>>(alloc);
            m_tree->set_parent(this, 0);
            if (m_valid)
                m_tree->init_from_ref(other.m_tree->get_ref());
        }
    }

    ConstLstIf(ConstLstIf&& other) noexcept
        : ConstLstBase(ColKey{}, nullptr)
        , m_tree(std::move(other.m_tree))
        , m_valid(other.m_valid)
    {
        m_tree->set_parent(this, 0);
    }

    ConstLstIf& operator=(const ConstLstIf& other)
    {
        if (this != &other) {
            m_valid = other.m_valid;
            m_col_key = other.m_col_key;
            m_deleted.clear();
            m_tree = nullptr;

            if (other.m_tree) {
                Allocator& alloc = other.m_tree->get_alloc();
                m_tree = std::make_unique<BPlusTree<T>>(alloc);
                m_tree->set_parent(this, 0);
                if (m_valid)
                    m_tree->init_from_ref(other.m_tree->get_ref());
            }
        }
        return *this;
    }

    bool init_from_parent() const final
    {
        m_valid = m_tree->init_from_parent();
        update_content_version();
        return m_valid;
    }
};

template <class T>
class ConstLst : public ConstLstIf<T> {
public:
    ConstLst(const ConstObj& owner, ColKey col_key);
    ConstLst(ConstLst&& other) noexcept
        : ConstLstBase(other.m_col_key, &m_obj)
        , ConstLstIf<T>(std::move(other))
        , m_obj(std::move(other.m_obj))
    {
        ConstLstBase::m_nullable = other.ConstLstBase::m_nullable;
    }

private:
    ConstObj m_obj;
    void update_child_ref(size_t, ref_type) override
    {
    }
};
/*
 * This class defines a virtual interface to a writable list
 */
class LstBase : public virtual ConstLstBase {
public:
    LstBase()
        : ConstLstBase(ColKey{}, nullptr)
    {
    }
    virtual ~LstBase()
    {
    }
    auto clone() const
    {
        return static_cast<const Obj*>(m_const_obj)->get_listbase_ptr(m_col_key);
    }
    virtual void set_null(size_t ndx) = 0;
    virtual void insert_null(size_t ndx) = 0;
    virtual void insert_any(size_t ndx, Mixed val) = 0;
    virtual void resize(size_t new_size) = 0;
    virtual void remove(size_t from, size_t to) = 0;
    virtual void move(size_t from, size_t to) = 0;
    virtual void swap(size_t ndx1, size_t ndx2) = 0;
    virtual void clear() = 0;
};

template <class T>
class Lst : public ConstLstIf<T>, public LstBase {
public:
    using ConstLstIf<T>::m_tree;
    using ConstLstIf<T>::get;

    Lst()
        : ConstLstBase({}, &m_obj)
    {
    }
    Lst(const Obj& owner, ColKey col_key);
    Lst(const Lst& other);
    Lst(Lst&& other) noexcept;

    Lst& operator=(const Lst& other);
    Lst& operator=(const BPlusTree<T>& other);

    void update_child_ref(size_t, ref_type new_ref) override
    {
        m_obj.set_int(ConstLstBase::m_col_key, from_ref(new_ref));
    }

    void create()
    {
        m_tree->create();
        ConstLstIf<T>::m_valid = true;
    }

    void set_null(size_t ndx) override
    {
        set(ndx, BPlusTree<T>::default_value(m_nullable));
    }

    void insert_null(size_t ndx) override
    {
        insert(ndx, BPlusTree<T>::default_value(m_nullable));
    }

    void insert_any(size_t ndx, Mixed val) override
    {
        if (val.is_null()) {
            insert_null(ndx);
        }
        else {
            insert(ndx, val.get<typename RemoveOptional<T>::type>());
        }
    }

    void resize(size_t new_size) override
    {
        update_if_needed();
        size_t current_size = m_tree->size();
        while (new_size > current_size) {
            insert_null(current_size++);
        }
        remove(new_size, current_size);
        m_obj.bump_both_versions();
    }

    void add(T value)
    {
        insert(size(), value);
    }

    T set(size_t ndx, T value)
    {
        REALM_ASSERT_DEBUG(!update_if_needed());

        if (value_is_null(value) && !m_nullable)
            throw LogicError(LogicError::column_not_nullable);

        // get will check for ndx out of bounds
        T old = get(ndx);
        if (old != value) {
            ensure_writeable();
            do_set(ndx, value);
            m_obj.bump_content_version();
        }
        if (Replication* repl = this->m_const_obj->get_replication()) {
            set_repl(repl, ndx, value);
        }
        return old;
    }

    void insert(size_t ndx, T value)
    {
        REALM_ASSERT_DEBUG(!update_if_needed());

        if (value_is_null(value) && !m_nullable)
            throw LogicError(LogicError::column_not_nullable);

        ensure_created();
        if (ndx > m_tree->size()) {
            throw std::out_of_range("Index out of range");
        }
        ensure_writeable();
        if (Replication* repl = this->m_const_obj->get_replication()) {
            insert_repl(repl, ndx, value);
        }
        do_insert(ndx, value);
        m_obj.bump_content_version();
    }

    T remove(LstIterator<T>& it)
    {
        return remove(ConstLstBase::adjust(it.m_ndx));
    }

    T remove(size_t ndx)
    {
        REALM_ASSERT_DEBUG(!update_if_needed());
        ensure_writeable();
        if (Replication* repl = this->m_const_obj->get_replication()) {
            ConstLstBase::erase_repl(repl, ndx);
        }
        T old = get(ndx);
        do_remove(ndx);
        ConstLstBase::adj_remove(ndx);
        m_obj.bump_content_version();

        return old;
    }

    void remove(size_t from, size_t to) override
    {
        while (from < to) {
            remove(--to);
        }
    }

    void move(size_t from, size_t to) override
    {
        REALM_ASSERT_DEBUG(!update_if_needed());
        if (from != to) {
            ensure_writeable();
            if (Replication* repl = this->m_const_obj->get_replication()) {
                ConstLstBase::move_repl(repl, from, to);
            }
            if (to > from) {
                to++;
            }
            else {
                from++;
            }
            // We use swap here as it handles the special case for StringData where
            // 'to' and 'from' points into the same array. In this case you cannot
            // set an entry with the result of a get from another entry in the same
            // leaf.
            m_tree->insert(to, BPlusTree<T>::default_value(m_nullable));
            m_tree->swap(from, to);
            m_tree->erase(from);

            m_obj.bump_content_version();
        }
    }

    void swap(size_t ndx1, size_t ndx2) override
    {
        REALM_ASSERT_DEBUG(!update_if_needed());
        if (ndx1 != ndx2) {
            if (Replication* repl = this->m_const_obj->get_replication()) {
                ConstLstBase::swap_repl(repl, ndx1, ndx2);
            }
            m_tree->swap(ndx1, ndx2);
            m_obj.bump_content_version();
        }
    }

    void clear() override
    {
        ensure_created();
        update_if_needed();
        ensure_writeable();
        if (size() > 0) {
            if (Replication* repl = this->m_const_obj->get_replication()) {
                ConstLstBase::clear_repl(repl);
            }
            m_tree->clear();
            m_obj.bump_content_version();
        }
    }

protected:
    Obj m_obj;
    bool update_if_needed()
    {
        if (m_obj.update_if_needed()) {
            return init_from_parent();
        }
        return false;
    }
    void ensure_created()
    {
        if (!ConstLstIf<T>::m_valid && m_obj.is_valid()) {
            create();
        }
    }
    void ensure_writeable()
    {
        if (m_obj.ensure_writeable()) {
            init_from_parent();
        }
    }
    void do_set(size_t ndx, T value)
    {
        m_tree->set(ndx, value);
    }
    void do_insert(size_t ndx, T value)
    {
        m_tree->insert(ndx, value);
    }
    void do_remove(size_t ndx)
    {
        m_tree->erase(ndx);
    }
    void set_repl(Replication* repl, size_t ndx, T value);
    void insert_repl(Replication* repl, size_t ndx, T value);
};

template <class T>
Lst<T>::Lst(const Lst<T>& other)
    : ConstLstBase(other.m_col_key, &m_obj)
    , ConstLstIf<T>(other)
    , m_obj(other.m_obj)
{
    m_nullable = other.m_nullable;
}

template <class T>
Lst<T>::Lst(Lst<T>&& other) noexcept
    : ConstLstBase(other.m_col_key, &m_obj)
    , ConstLstIf<T>(std::move(other))
    , m_obj(std::move(other.m_obj))
{
    m_nullable = other.m_nullable;
}

template <class T>
Lst<T>& Lst<T>::operator=(const Lst& other)
{
    ConstLstIf<T>::operator=(other);
    m_obj = other.m_obj;
    m_nullable = other.m_nullable;
    return *this;
}

template <class T>
Lst<T>& Lst<T>::operator=(const BPlusTree<T>& other)
{
    *m_tree = other;
    return *this;
}


template <>
void Lst<ObjKey>::do_set(size_t ndx, ObjKey target_key);

template <>
void Lst<ObjKey>::do_insert(size_t ndx, ObjKey target_key);

template <>
void Lst<ObjKey>::do_remove(size_t ndx);

template <>
void Lst<ObjKey>::clear();

class ConstLnkLst : public ConstLstIf<ObjKey> {
public:
    ConstLnkLst()
        : ConstLstBase({}, &m_obj)
    {
    }

    ConstLnkLst(const ConstObj& obj, ColKey col_key)
        : ConstLstBase(col_key, &m_obj)
        , ConstLstIf<ObjKey>(obj.get_alloc())
        , m_obj(obj)
    {
        this->init_from_parent();
    }
    ConstLnkLst(ConstLnkLst&& other) noexcept
        : ConstLstBase(other.m_col_key, &m_obj)
        , ConstLstIf<ObjKey>(std::move(other))
        , m_obj(std::move(other.m_obj))
    {
    }

    // Getting links
    ConstObj operator[](size_t link_ndx) const
    {
        return get_object(link_ndx);
    }
    ConstObj get_object(size_t link_ndx) const;

    void update_child_ref(size_t, ref_type) override
    {
    }

private:
    ConstObj m_obj;
};

class LnkLst : public Lst<ObjKey>, public ObjList {
public:
    LnkLst()
        : ConstLstBase({}, &m_obj)
        , ObjList(this->m_tree.get())
    {
    }
    LnkLst(const Obj& owner, ColKey col_key);
    LnkLst(const LnkLst& other)
        : ConstLstBase(other.m_col_key, &m_obj)
        , Lst<ObjKey>(other)
        , ObjList(this->m_tree.get(), m_obj.get_target_table(m_col_key))
    {
    }
    LnkLst(LnkLst&& other) noexcept
        : ConstLstBase(other.m_col_key, &m_obj)
        , Lst<ObjKey>(std::move(other))
        , ObjList(this->m_tree.get(), m_obj.get_target_table(m_col_key))
    {
    }
    LnkLst& operator=(const LnkLst& other)
    {
        Lst<ObjKey>::operator=(other);
        this->ObjList::assign(this->m_tree.get(), m_obj.get_target_table(m_col_key));
        return *this;
    }

    LnkLstPtr clone() const
    {
        if (m_obj.is_valid()) {
            return std::make_unique<LnkLst>(m_obj, m_col_key);
        }
        else {
            return std::make_unique<LnkLst>();
        }
    }
    TableRef get_target_table() const
    {
        return m_table.cast_away_const();
    }
    bool is_in_sync() const override
    {
        return true;
    }
    size_t size() const override
    {
        return Lst<ObjKey>::size();
    }

    Obj get_object(size_t ndx);

    Obj operator[](size_t ndx)
    {
        return get_object(ndx);
    }

    using Lst<ObjKey>::find_first;
    using Lst<ObjKey>::find_all;

    TableView get_sorted_view(SortDescriptor order) const;
    TableView get_sorted_view(ColKey column_key, bool ascending = true) const;
    void remove_target_row(size_t link_ndx);
    void remove_all_target_rows();

private:
    friend class DB;
    friend class ConstTableView;
    friend class Query;
    void get_dependencies(TableVersions&) const override;
    void sync_if_needed() const override;
};

template <typename U>
ConstLst<U> ConstObj::get_list(ColKey col_key) const
{
    return ConstLst<U>(*this, col_key);
}

template <typename U>
ConstLstPtr<U> ConstObj::get_list_ptr(ColKey col_key) const
{
    Obj obj(*this);
    return std::const_pointer_cast<const Lst<U>>(obj.get_list_ptr<U>(col_key));
}

template <typename U>
Lst<U> Obj::get_list(ColKey col_key) const
{
    return Lst<U>(*this, col_key);
}

template <typename U>
LstPtr<U> Obj::get_list_ptr(ColKey col_key) const
{
    return std::make_unique<Lst<U>>(*this, col_key);
}

template <>
inline LstPtr<ObjKey> Obj::get_list_ptr(ColKey col_key) const
{
    return get_linklist_ptr(col_key);
}

inline ConstLnkLst ConstObj::get_linklist(ColKey col_key) const
{
    return ConstLnkLst(*this, col_key);
}

inline ConstLnkLst ConstObj::get_linklist(StringData col_name) const
{
    return get_linklist(get_column_key(col_name));
}

inline ConstLnkLstPtr ConstObj::get_linklist_ptr(ColKey col_key) const
{
    Obj obj(*this);
    return obj.get_linklist_ptr(col_key);
}

inline LnkLst Obj::get_linklist(ColKey col_key) const
{
    return LnkLst(*this, col_key);
}

inline LnkLstPtr Obj::get_linklist_ptr(ColKey col_key) const
{
    return std::make_unique<LnkLst>(*this, col_key);
}

inline LnkLst Obj::get_linklist(StringData col_name) const
{
    return get_linklist(get_column_key(col_name));
}

template <class T>
inline typename ColumnTypeTraits<T>::sum_type list_sum(const ConstLstIf<T>& list, size_t* return_cnt = nullptr)
{
    return bptree_sum(list.get_tree(), return_cnt);
}

template <class T>
inline typename ColumnTypeTraits<T>::minmax_type list_maximum(const ConstLstIf<T>& list, size_t* return_ndx = nullptr)
{
    return bptree_maximum(list.get_tree(), return_ndx);
}

template <class T>
inline typename ColumnTypeTraits<T>::minmax_type list_minimum(const ConstLstIf<T>& list, size_t* return_ndx = nullptr)
{
    return bptree_minimum(list.get_tree(), return_ndx);
}

template <class T>
inline double list_average(const ConstLstIf<T>& list, size_t* return_cnt = nullptr)
{
    return bptree_average(list.get_tree(), return_cnt);
}
}

#endif /* REALM_LIST_HPP */
