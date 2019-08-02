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
#ifndef REALM_UTIL_SUBSTITUTE_HPP
#define REALM_UTIL_SUBSTITUTE_HPP

#include <type_traits>
#include <utility>
#include <functional>
#include <stdexcept>
#include <vector>
#include <map>
#include <locale>
#include <ostream>
#include <sstream>

#include <realm/util/optional.hpp>
#include <realm/util/string_view.hpp>
#include <realm/util/logger.hpp>


namespace realm {
namespace util {
namespace _private {

class SubstituterBase {
protected:
    template<class, class...> struct FindArg1;
    template<class, bool, class, class...> struct FindArg2;
    static StderrLogger s_default_logger;
};

} // namespace _private


struct SubstituterConfig {
    /// Allow parsing to be considered successful even when syntax errors are
    /// detected. When enabled, logging will happen on `warn`, instead of
    /// `error` level.
    bool lenient = false;

    /// The logger to be used by default. If left unspecified, the default
    /// logger is one that logs to STDERR. In any case, logging happens only
    /// during parsing.
    Logger* logger = nullptr;
};


/// Perform variable substitutions in text.
///
/// A variable reference generally has the form `@{<name>}`, where `<name>` is
/// the variable name. For example, if the variable name is `x`, then `@{x}` is
/// a reference to that variable. If the variable name consists of a single
/// letter, then a shorter form of reference, `@<name>` is available, i.e.,
/// since `x` is a single letter, `@x` is a reference to `x`. As a special rule,
/// `@@` is substituted by `@`.
///
/// Example of use:
///
///     struct CtxA { int y = 0; };
///     struct CtxB { int x = 0; };
///     using Subst = Substituter<const CtxA&, const CtxB&>;
///     Subst subst;
///     subst["x"] = &CtxB::x;
///     subst["y"] = [](std::ostream& out, const CtxA& a, const CtxB&) {
///         out << a.y;
///     };
///     Subst::Template templ;
///     if (subst.parse("<@x:@y>\n", templ)) {
///         CtxA a;
///         CtxB b;
///         for (int i = 0; i < 3; ++i) {
///             templ.expand(std::cout, a, b);
///             a.y += 1;
///             b.x += 2;
///         }
///     }
///
/// This code should write
///
///     <0:0>
///     <2:1>
///     <4:2>
///
/// to STDOUT.
template<class... A> class Substituter : private _private::SubstituterBase {
public:
    using EvalFunc = void(std::ostream&, A&...);
    class ProtoDef;
    class Template;

    Substituter(SubstituterConfig = {}) noexcept;

    ProtoDef operator[](const char* name) noexcept;

    bool expand(StringView text, std::ostream&, A&&...) const;

    bool parse(StringView text, Template&) const;
    bool parse(StringView text, Template&, Logger&) const;

private:
    using size_type = StringView::size_type;
    struct Substitution;

    const bool m_lenient;
    Logger& m_logger;

    using Variables = std::map<StringView, std::function<EvalFunc>>;
    Variables m_variables;

    void define(const char* name, std::function<EvalFunc>);
};



template<class... A> class Substituter<A...>::ProtoDef {
public:
    template<class T> void operator=(T*);
    template<class T, class C> void operator=(T C::*);
    void operator=(std::function<EvalFunc>);

private:
    Substituter& m_substituter;
    const char* m_name;

    ProtoDef(Substituter& substituter, const char* name) noexcept;

    friend class Substituter;
};



template<class... A> class Substituter<A...>::Template {
public:
    /// Uses std::locale::classic().
    std::string expand(A&&...) const;

    void expand(std::ostream&, A...) const;

    bool refers_to(const char* name) const noexcept;

private:
    StringView m_text;
    std::vector<Substitution> m_substitutions;

    friend class Substituter;
};





// Implementation

namespace _private {

template<class T, bool, class A, class... B> struct SubstituterBase::FindArg2 {
    static const T& find(const A&, const B&... b) noexcept
    {
        return FindArg1<T, B...>::find(b...);
    }
};

template<class T, class A, class... B>
struct SubstituterBase::FindArg2<T, true, A, B...> {
    static const T& find(const A& a, const B&...) noexcept
    {
        return a;
    }
};

template<class T, class A, class... B> struct SubstituterBase::FindArg1<T, A, B...> {
    static const T& find(const A& a, const B&... b) noexcept
    {
        using P = typename std::remove_reference<A>::type*;
        return FindArg2<T, std::is_convertible<P, const T*>::value, A, B...>::find(a, b...);
    }
};

} // namespace _private

template<class... A> struct Substituter<A...>::Substitution {
    size_type begin, end;
    const typename Variables::value_type* var_def;
};

template<class... A> inline Substituter<A...>::Substituter(SubstituterConfig config) noexcept :
    m_lenient{config.lenient},
    m_logger{config.logger ? *config.logger : s_default_logger}
{
}

template<class... A> inline auto Substituter<A...>::operator[](const char* name) noexcept -> ProtoDef
{
    return ProtoDef{*this, name};
}

template<class... A>
inline bool Substituter<A...>::expand(StringView text, std::ostream& out, A&&... arg) const
{
    Template templ;
    if (parse(text, templ)) { // Throws
        templ.expand(out, std::forward<A>(arg)...); // Throws
        return true;
    }
    return false;
}

template<class... A> inline bool Substituter<A...>::parse(StringView text, Template& templ) const
{
    return parse(text, templ, m_logger); // Throws
}

template<class... A>
bool Substituter<A...>::parse(StringView text, Template& templ, Logger& logger) const
{
    bool error = false;
    Logger::Level log_level = (m_lenient ? Logger::Level::warn : Logger::Level::error);
    std::vector<Substitution> substitutions;
    StringView var_name;
    size_type curr = 0;
    size_type end  = text.size();
    for (;;) {
        size_type i = text.find('@', curr);
        if (i == StringView::npos)
            break;
        if (i + 1 == end) {
            logger.log(log_level, "Unterminated `@` at end of text"); // Throws
            error = true;
            break;
        }
        char ch = text[i + 1];
        if (ch == '{') {
            size_type j = text.find('}', i + 2);
            if (j == StringView::npos) {
                logger.log(log_level, "Unterminated `@{`"); // Throws
                error = true;
                curr = i + 2;
                continue;
            }
            var_name = text.substr(i + 2, j - (i + 2));
            curr = j + 1;
        }
        else {
            var_name = text.substr(i + 1, 1); // Throws
            curr = i + 2;
        }
        const typename Variables::value_type* var_def = nullptr;
        if (ch != '@') {
            auto k = m_variables.find(var_name);
            if (k == m_variables.end()) {
                logger.log(log_level, "Undefined variable `%1` in substitution `%2`", var_name,
                           text.substr(i, curr - i)); // Throws
                error = true;
                continue;
            }
            var_def = &*k;
        }
        substitutions.push_back({i, curr, var_def}); // Throws
    }
    if (error && !m_lenient)
        return false;
    templ.m_text = text;
    templ.m_substitutions = std::move(substitutions);
    return true;
}

template<class... A>
inline void Substituter<A...>::define(const char* name, std::function<EvalFunc> func)
{
    auto p = m_variables.emplace(name, std::move(func)); // Throws
    bool was_inserted = p.second;
    if (!was_inserted)
        throw std::runtime_error("Multiple definitions for same variable name");
}

template<class... A> template<class T> inline void Substituter<A...>::ProtoDef::operator=(T* var)
{
    *this = [var](std::ostream& out, const A&...) {
        out << *var; // Throws
    };
}

template<class... A>
template<class T, class C> inline void Substituter<A...>::ProtoDef::operator=(T C::* var)
{
    *this = [var](std::ostream& out, const A&... arg) {
        const C& obj = FindArg1<C, A...>::find(arg...);
        out << obj.*var; // Throws
    };
}

template<class... A>
inline void Substituter<A...>::ProtoDef::operator=(std::function<EvalFunc> func)
{
    m_substituter.define(m_name, std::move(func)); // Throws
}

template<class... A>
inline Substituter<A...>::ProtoDef::ProtoDef(Substituter& substituter, const char* name) noexcept :
    m_substituter{substituter},
    m_name{name}
{
}

template<class... A> std::string Substituter<A...>::Template::expand(A&&... arg) const
{
    std::ostringstream out;
    out.imbue(std::locale::classic());
    expand(out, std::forward<A>(arg)...); // Throws
    std::string str = std::move(out).str(); // Throws
    return str;
}

template<class... A> void Substituter<A...>::Template::expand(std::ostream& out, A... arg) const
{
    std::ios_base::fmtflags flags = out.flags();
    try {
        size_type curr = 0;
        for (const Substitution& subst: m_substitutions) {
            out << m_text.substr(curr, subst.begin - curr); // Throws
            if (subst.var_def) {
                const std::function<EvalFunc>& eval_func = subst.var_def->second;
                eval_func(out, arg...); // Throws
                out.flags(flags);
            }
            else {
                out << "@"; // Throws
            }
            curr = subst.end;
        }
        out << m_text.substr(curr); // Throws
    }
    catch (...) {
        out.flags(flags);
        throw;
    }
}

template<class... A>
inline bool Substituter<A...>::Template::refers_to(const char* name) const noexcept
{
    StringView name_2 = name;
    for (const auto& subst: m_substitutions) {
        if (subst.var_def) {
            if (name_2 != subst.var_def->first)
                continue;
            return true;
        }
    }
    return false;
}

} // namespace util
} // namespace realm

#endif // REALM_UTIL_SUBSTITUTE_HPP
