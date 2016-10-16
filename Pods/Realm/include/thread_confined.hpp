////////////////////////////////////////////////////////////////////////////
//
// Copyright 2016 Realm Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
////////////////////////////////////////////////////////////////////////////

#ifndef REALM_THREAD_CONFINED_HPP
#define REALM_THREAD_CONFINED_HPP

#include "list.hpp"
#include "object_accessor.hpp"
#include "results.hpp"

#include <realm/link_view.hpp>
#include <realm/query.hpp>
#include <realm/row.hpp>
#include <realm/table_view.hpp>

namespace realm {
namespace _impl {
    class AnyHandover;
}
// Type-erased wrapper for any type which must be exported to be handed between threads
class AnyThreadConfined {
public:
    enum class Type {
        Object,
        List,
        Results,
    };
    
    // Constructors
    AnyThreadConfined(Object object)   : m_type(Type::Object),  m_object(object)   { }
    AnyThreadConfined(List list)       : m_type(Type::List),    m_list(list)       { }
    AnyThreadConfined(Results results) : m_type(Type::Results), m_results(results) { }

    AnyThreadConfined(const AnyThreadConfined&);
    AnyThreadConfined& operator=(const AnyThreadConfined&);
    AnyThreadConfined(AnyThreadConfined&&);
    AnyThreadConfined& operator=(AnyThreadConfined&&);
    ~AnyThreadConfined();

    Type get_type() const { return m_type; }
    SharedRealm get_realm() const;

    // Getters
    Object  get_object()  const { REALM_ASSERT(m_type == Type::Object);  return m_object;  }
    List    get_list()    const { REALM_ASSERT(m_type == Type::List);    return m_list;    }
    Results get_results() const { REALM_ASSERT(m_type == Type::Results); return m_results; }

    _impl::AnyHandover export_for_handover() const;

private:
    Type m_type;
    union {
        Object  m_object;
        List    m_list;
        Results m_results;
    };
};
}

#endif /* REALM_THREAD_CONFINED_HPP */
