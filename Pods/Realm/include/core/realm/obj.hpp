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

#ifndef REALM_OBJ_HPP
#define REALM_OBJ_HPP

#include <realm/array.hpp>
#include <realm/cluster.hpp>
#include <realm/table_ref.hpp>
#include <realm/keys.hpp>
#include <map>

#define REALM_CLUSTER_IF

namespace realm {

class Replication;
class TableView;
class ConstLstBase;
class LstBase;
struct GlobalKey;

template <class>
class ConstLstIf;

template <class>
class ConstLst;

template <class>
class Lst;
template <class T>
using LstPtr = std::unique_ptr<Lst<T>>;
template <class T>
using ConstLstPtr = std::unique_ptr<const Lst<T>>;
using ConstLstBasePtr = std::unique_ptr<ConstLstBase>;
using LstBasePtr = std::unique_ptr<LstBase>;

class LnkLst;
class ConstLnkLst;
using LnkLstPtr = std::unique_ptr<LnkLst>;
using ConstLnkLstPtr = std::unique_ptr<const LnkLst>;

// 'Object' would have been a better name, but it clashes with a class in ObjectStore
class ConstObj {
public:
    ConstObj()
        : m_table(nullptr)
        , m_row_ndx(size_t(-1))
        , m_storage_version(-1)
        , m_valid(false)
    {
    }
    ConstObj(ConstTableRef table, MemRef mem, ObjKey key, size_t row_ndx);

    Allocator& get_alloc() const;

    bool operator==(const ConstObj& other) const;

    ObjKey get_key() const
    {
        return m_key;
    }

    GlobalKey get_object_id() const;

    ConstTableRef get_table() const
    {
        return m_table;
    }

    Replication* get_replication() const;

    // Check if this object is default constructed
    explicit operator bool() const
    {
        return m_table != nullptr;
    }

    // Check if the object is still alive
    bool is_valid() const;
    // Will throw if object is not valid
    void check_valid() const;
    // Delete object from table. Object is invalid afterwards.
    void remove();

    template <typename U>
    U get(ColKey col_key) const;

    Mixed get_any(ColKey col_key) const;

    template <typename U>
    U get(StringData col_name) const
    {
        return get<U>(get_column_key(col_name));
    }
    ConstObj get_linked_object(ColKey link_col_key) const;
    int cmp(const ConstObj& other, ColKey col_key) const;

    template <typename U>
    ConstLst<U> get_list(ColKey col_key) const;
    template <typename U>
    ConstLstPtr<U> get_list_ptr(ColKey col_key) const;
    template <typename U>
    ConstLst<U> get_list(StringData col_name) const
    {
        return get_list<U>(get_column_key(col_name));
    }

    ConstLnkLst get_linklist(ColKey col_key) const;
    ConstLnkLstPtr get_linklist_ptr(ColKey col_key) const;
    ConstLnkLst get_linklist(StringData col_name) const;

    ConstLstBasePtr get_listbase_ptr(ColKey col_key) const;

    size_t get_link_count(ColKey col_key) const;

    bool is_null(ColKey col_key) const;
    bool is_null(StringData col_name) const
    {
        return is_null(get_column_key(col_name));
    }
    bool has_backlinks(bool only_strong_links) const;
    size_t get_backlink_count(bool only_strong_links = false) const;
    size_t get_backlink_count(const Table& origin, ColKey origin_col_key) const;
    ObjKey get_backlink(const Table& origin, ColKey origin_col_key, size_t backlink_ndx) const;
    TableView get_backlink_view(TableRef src_table, ColKey src_col_key);

    // To be used by the query system when a single object should
    // be tested. Will allow a function to be called in the context
    // of the owning cluster.
    template <class T>
    bool evaluate(T func) const
    {
        Cluster cluster(0, get_alloc(), *get_tree_top());
        cluster.init(m_mem);
        cluster.set_offset(m_key.value - cluster.get_key_value(m_row_ndx));
        return func(&cluster, m_row_ndx);
    }

    void to_json(std::ostream& out, size_t link_depth, std::map<std::string, std::string>& renames,
                 std::vector<ColKey>& followed) const;
    void to_json(std::ostream& out, size_t link_depth = 0,
                 std::map<std::string, std::string>* renames = nullptr) const
    {
        std::map<std::string, std::string> renames2;
        renames = renames ? renames : &renames2;

        std::vector<ColKey> followed;
        to_json(out, link_depth, *renames, followed);
    }

    std::string to_string() const;

protected:
    friend class Obj;
    friend class ColumnListBase;
    friend class ConstLstBase;
    friend class ConstLnkLst;
    friend class LnkLst;
    friend class LinkMap;
    friend class ConstTableView;
    friend class Transaction;
    friend struct ClusterNode::IteratorState;

    mutable ConstTableRef m_table;
    ObjKey m_key;
    mutable MemRef m_mem;
    mutable size_t m_row_ndx;
    mutable uint64_t m_storage_version;
    mutable bool m_valid;

    Allocator& _get_alloc() const;
    bool update() const;
    // update if needed - with and without check of table instance version:
    bool update_if_needed() const;
    bool _update_if_needed() const; // no check, use only when already checked
    template <class T>
    bool do_is_null(ColKey::Idx col_ndx) const;

    const ClusterTree* get_tree_top() const;
    ColKey get_column_key(StringData col_name) const;
    TableKey get_table_key() const;
    TableRef get_target_table(ColKey col_key) const;
    const Spec& get_spec() const;

    template <typename U>
    U _get(ColKey::Idx col_ndx) const;

    template <class T>
    int cmp(const ConstObj& other, ColKey::Idx col_ndx) const;
    int cmp(const ConstObj& other, ColKey::Idx col_ndx) const;
    ObjKey get_backlink(ColKey backlink_col, size_t backlink_ndx) const;
    std::vector<ObjKey> get_all_backlinks(ColKey backlink_col) const;
};

std::ostream& operator<<(std::ostream&, const ConstObj& obj);

class Obj : public ConstObj {
public:
    Obj()
    {
    }
    Obj(TableRef table, MemRef mem, ObjKey key, size_t row_ndx);

    TableRef get_table() const
    {
        return m_table.cast_away_const();
    }


    template <typename U>
    Obj& set(ColKey col_key, U value, bool is_default = false);

    Obj& set(ColKey col_key, Mixed value);

    template <typename U>
    Obj& set(StringData col_name, U value, bool is_default = false)
    {
        return set(get_column_key(col_name), value, is_default);
    }

    Obj& set_null(ColKey col_key, bool is_default = false);
    Obj& set_null(StringData col_name, bool is_default = false)
    {
        return set_null(get_column_key(col_name), is_default);
    }

    Obj& add_int(ColKey col_key, int64_t value);
    Obj& add_int(StringData col_name, int64_t value)
    {
        return add_int(get_column_key(col_name), value);
    }

    template <typename U>
    Obj& set_list_values(ColKey col_key, const std::vector<U>& values);

    template <typename U>
    std::vector<U> get_list_values(ColKey col_key) const;

    template <class Head, class... Tail>
    Obj& set_all(Head v, Tail... tail);

    void assign(const ConstObj& other);

    Obj get_linked_object(ColKey link_col_key);

    template <typename U>
    Lst<U> get_list(ColKey col_key) const;
    template <typename U>
    LstPtr<U> get_list_ptr(ColKey col_key) const;

    template <typename U>
    Lst<U> get_list(StringData col_name) const
    {
        return get_list<U>(get_column_key(col_name));
    }

    LnkLst get_linklist(ColKey col_key) const;
    LnkLstPtr get_linklist_ptr(ColKey col_key) const;
    LnkLst get_linklist(StringData col_name) const;

    LstBasePtr get_listbase_ptr(ColKey col_key) const;

private:
    friend class ArrayBacklink;
    friend class CascadeState;
    friend class Cluster;
    friend class ConstLstBase;
    friend class ConstObj;
    template <class>
    friend class Lst;
    friend class LnkLst;
    friend class Table;

    Obj(const ConstObj& other)
        : ConstObj(other)
    {
    }
    template <class Val>
    Obj& _set(size_t col_ndx, Val v);
    template <class Head, class... Tail>
    Obj& _set(size_t col_ndx, Head v, Tail... tail);
    ColKey spec_ndx2colkey(size_t col_ndx);
    bool ensure_writeable();
    void bump_content_version();
    void bump_both_versions();
    template <class T>
    void do_set_null(ColKey col_key);

    void set_int(ColKey col_key, int64_t value);
    void add_backlink(ColKey backlink_col, ObjKey origin_key);
    bool remove_one_backlink(ColKey backlink_col, ObjKey origin_key);
    void nullify_link(ColKey origin_col, ObjKey target_key);
    // Used when inserting a new link. You will not remove existing links in this process
    void set_backlink(ColKey col_key, ObjKey new_key) const;
    // Used when replacing a link, return true if CascadeState contains objects to remove
    bool replace_backlink(ColKey col_key, ObjKey old_key, ObjKey new_key, CascadeState& state) const;
    // Used when removing a backlink, return true if CascadeState contains objects to remove
    bool remove_backlink(ColKey col_key, ObjKey old_key, CascadeState& state) const;
    template <class T>
    inline void set_spec(T&, ColKey);
};

inline Obj Obj::get_linked_object(ColKey link_col_key)
{
    return ConstObj::get_linked_object(link_col_key);
}

template <>
Obj& Obj::set(ColKey, int64_t value, bool is_default);

template <>
Obj& Obj::set(ColKey, ObjKey value, bool is_default);


template <>
inline Obj& Obj::set(ColKey col_key, int value, bool is_default)
{
    return set(col_key, int_fast64_t(value), is_default);
}

template <>
inline Obj& Obj::set(ColKey col_key, uint_fast64_t value, bool is_default)
{
    int_fast64_t value_2 = 0;
    if (REALM_UNLIKELY(int_cast_with_overflow_detect(value, value_2))) {
        REALM_TERMINATE("Unsigned integer too big.");
    }
    return set(col_key, value_2, is_default);
}

template <>
inline Obj& Obj::set(ColKey col_key, const char* str, bool is_default)
{
    return set(col_key, StringData(str), is_default);
}

template <>
inline Obj& Obj::set(ColKey col_key, char* str, bool is_default)
{
    return set(col_key, StringData(str), is_default);
}

template <>
inline Obj& Obj::set(ColKey col_key, std::string str, bool is_default)
{
    return set(col_key, StringData(str), is_default);
}

template <>
inline Obj& Obj::set(ColKey col_key, realm::null, bool is_default)
{
    return set_null(col_key, is_default);
}

template <>
inline Obj& Obj::set(ColKey col_key, Optional<bool> value, bool is_default)
{
    return value ? set(col_key, *value, is_default) : set_null(col_key, is_default);
}

template <>
inline Obj& Obj::set(ColKey col_key, Optional<int64_t> value, bool is_default)
{
    return value ? set(col_key, *value, is_default) : set_null(col_key, is_default);
}

template <>
inline Obj& Obj::set(ColKey col_key, Optional<float> value, bool is_default)
{
    return value ? set(col_key, *value, is_default) : set_null(col_key, is_default);
}

template <>
inline Obj& Obj::set(ColKey col_key, Optional<double> value, bool is_default)
{
    return value ? set(col_key, *value, is_default) : set_null(col_key, is_default);
}

template <typename U>
Obj& Obj::set_list_values(ColKey col_key, const std::vector<U>& values)
{
    size_t sz = values.size();
    auto list = get_list<U>(col_key);
    list.resize(sz);
    for (size_t i = 0; i < sz; i++)
        list.set(i, values[i]);

    return *this;
}

template <typename U>
std::vector<U> Obj::get_list_values(ColKey col_key) const
{
    std::vector<U> values;
    auto list = get_list<U>(col_key);
    for (auto v : list)
        values.push_back(v);

    return values;
}

template <class Val>
inline Obj& Obj::_set(size_t col_ndx, Val v)
{
    return set(spec_ndx2colkey(col_ndx), v);
}

template <class Head, class... Tail>
inline Obj& Obj::_set(size_t col_ndx, Head v, Tail... tail)
{
    set(spec_ndx2colkey(col_ndx), v);
    return _set(col_ndx + 1, tail...);
}

template <class Head, class... Tail>
inline Obj& Obj::set_all(Head v, Tail... tail)
{
    return _set(0, v, tail...);
}
}

#endif // REALM_OBJ_HPP
