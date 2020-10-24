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

#ifndef REALM_OBJ_LIST_HPP
#define REALM_OBJ_LIST_HPP

#include <realm/array_key.hpp>
#include <realm/table_ref.hpp>
#include <realm/handover_defs.hpp>
#include <realm/obj.hpp>

namespace realm {

class DescriptorOrdering;
class Table;
class ConstTableView;

class ObjList {
public:
    ObjList(KeyColumn* key_values);
    ObjList(KeyColumn* key_values, ConstTableRef parent);

    virtual ~ObjList()
    {
#ifdef REALM_COOKIE_CHECK
        m_debug_cookie = 0x7765697633333333; // 0x77656976 = 'view'; 0x33333333 = '3333' = destructed
#endif
    }

    const Table& get_parent() const noexcept
    {
        return *m_table;
    }

    virtual size_t size() const;

    // Get the number of total results which have been filtered out because a number of "LIMIT" operations have
    // been applied. This number only applies to the last sync.
    size_t get_num_results_excluded_by_limit() const noexcept
    {
        return m_limit_count;
    }

    // Get key for object this view is "looking" at.
    ObjKey get_key(size_t ndx) const;

    ConstObj try_get_object(size_t row_ndx) const;
    ConstObj get_object(size_t row_ndx) const;
    ConstObj front() const noexcept
    {
        return get_object(0);
    }
    ConstObj back() const noexcept
    {
        size_t last_row_ndx = size() - 1;
        return get_object(last_row_ndx);
    }
    ConstObj operator[](size_t row_ndx) const noexcept
    {
        return get_object(row_ndx);
    }

    template <class F>
    void for_each(F func) const;

    template <class T>
    ConstTableView find_all(ColKey column_key, T value);

    template <class T>
    size_t find_first(ColKey column_key, T value);

    // Get the versions of all tables which this list depends on
    TableVersions get_dependency_versions() const;

    // These three methods are overridden by TableView and ObjList/LnkLst.
    virtual void sync_if_needed() const = 0;
    virtual void get_dependencies(TableVersions&) const = 0;
    virtual bool is_in_sync() const = 0;
    void check_cookie() const
    {
#ifdef REALM_COOKIE_CHECK
        REALM_ASSERT_RELEASE(m_debug_cookie == cookie_expected);
#endif
    }

protected:
    friend class Query;
    static const uint64_t cookie_expected = 0x7765697677777777ull; // 0x77656976 = 'view'; 0x77777777 = '7777' = alive

    // Null if, and only if, the view is detached.
    mutable ConstTableRef m_table;
    KeyColumn* m_key_values = nullptr;
    size_t m_limit_count = 0;
    uint64_t m_debug_cookie;

    void assign(KeyColumn* key_values, ConstTableRef parent);

    void do_sort(const DescriptorOrdering&);
    void detach() const noexcept // may have to remove const
    {
        m_table = TableRef();
    }
};

template <class F>
inline void ObjList::for_each(F func) const
{
    auto sz = size();
    for (size_t i = 0; i < sz; i++) {
        auto o = try_get_object(i);
        if (o && func(o))
            return;
    }
}

template <class T>
size_t ObjList::find_first(ColKey column_key, T value)
{
    auto sz = size();
    for (size_t i = 0; i < sz; i++) {
        auto o = try_get_object(i);
        if (o) {
            T v = o.get<T>(column_key);
            if (v == value)
                return i;
        }
    }
    return realm::npos;
}
}

#endif /* SRC_REALM_OBJ_LIST_HPP_ */
