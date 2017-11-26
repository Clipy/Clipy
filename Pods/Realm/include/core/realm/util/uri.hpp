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
#ifndef REALM_UTIL_URI_HPP
#define REALM_UTIL_URI_HPP

#include <string>

namespace realm {
namespace util {


/// \brief A decomposed URI reference.
///
/// A Uri object contains a URI reference decomposed into its 5 main component
/// parts (scheme, authority, path, query, and fragment identifier).
///
/// The decomposition process (as carried out by the constructor) performs a
/// maximally lenient parsing of the specified URI reference. It does that
/// according to the following regular expression (copied verbatimly from
/// http://tools.ietf.org/html/rfc3986#appendix-B):
///
///     ^(([^:/?#]+):)?(//([^/?#]*))?([^?#]*)(\?([^#]*))?(#(.*))?
///      12            3  4          5       6  7        8 9
///
///     Group
///     ------------------------
///     1       Scheme part
///     3       Authority part
///     5       Path part
///     6       Query part
///     8       Fragment identifier part
///
/// NOTE: Since this regular expression maches every string, every string is
/// decomposable.
///
/// NOTE: This class does not attempt to perform any level of validation of URI
/// references against the grammer specified in the RFC. Such validation could
/// be added later, for example through a new `Uri::validate()`.
///
/// For example, the decomposition of
/// "http://www.ietf.org/rfc/rfc2396.txt?foo=bar#chp3" is:
///
/// <pre>
///
///   scheme -> "http:"
///   auth   -> "//www.ietf.org"
///   path   -> "/rfc/rfc2396.txt"
///   query  -> "?foo=bar"
///   frag   -> "#chp3"
///
/// </pre>
///
/// This class also provides recomposition of a URI references from their
/// component parts, where the parts can be specified individually, or be a
/// result of URI resoultion.
///
/// It is important to understand, however, that certain restrictions need to
/// apply to each component part in order that the URI reference as a whole is
/// self consistent. More concretely, it is necessary to require that the
/// component parts at any time must have values that will be preserved across a
/// recomposition -> decomposition cycle.
///
/// The actual restrictions on each component part is specified for the
/// corresponding setter-method (e.g., set_scheme()).
///
/// Note that component parts resulting from decomposition, canonicalize, or
/// from resolution (resolve()) will automatically (by design of the underlying
/// algorithm) adhere to these rules.
///
/// Decomposition, recomposition, conanonicalization, and resolution algorithms
/// are taken from RFC 3986.
///
/// \sa http://tools.ietf.org/html/rfc3986
class Uri {
public:
    Uri();

    /// Decompose the specified URI reference into its five main parts.
    Uri(const std::string&);

    /// Reconstruct a URI reference from its 5 components.
    std::string recompose() const;

/*
    /// Resolve this URI reference against the specified base URI reference
    /// according to the rules described in section 5.2 of RFC 3986.
    ///
    /// Be aware that a fragment identifier on the base URI reference is never
    /// carried over to the result. This is in accordance with the RFC.
    void resolve(const Uri& base, bool strict = true);
*/

    /// Remove empty URI components. Also, for URI references having either a
    /// scheme part or an authority part, replace an absent path with "/".
    void canonicalize();

    /// Get the scheme part of this URI reference including the trailing ":", or
    /// the empty tring if there is no scheme part.
    const std::string& get_scheme() const;

    /// Get the authority part of this URI reference including the leading "//",
    /// or the empty tring if there is no authority part.
    const std::string& get_auth() const;

    /// Same as get_auth() (with no arguments), but parse the authority component
    /// into userinfo, host, and port subcomponents.
    ///
    /// \return True if, and only if the authority component was present (i.e.,
    /// not the empty string). When false is returned, none of the specified
    /// strings will have been modified.
    bool get_auth(std::string& userinfo, std::string& host, std::string& port) const;

    /// Get the path part of this URI reference, or the empty tring if there is
    /// no path part.
    const std::string& get_path() const;

    /// Get the query part of this URI reference including the leading "?", or
    /// the empty tring if there is no query part.
    const std::string& get_query() const;

    /// Get the fragment identifier of this URI reference including the leading
    /// "#", or the empty tring if there is no fragment identifier.
    const std::string& get_frag() const;

    /// The specified string must either be empty or have a final ":". Also, it
    /// must not contain "/", "?", or "#", nor may it contain more than one ":".
    ///
    /// \throw std::invalid_argument If the specified string is not valid
    /// according to the specified rules.
    void set_scheme(const std::string&);

    /// The specified string must either be empty or have "//" as a
    /// prefix. Also, it must not contain "?" or "#", nor may it contain "/"
    /// beyond the first two.
    ///
    /// \throw std::invalid_argument If the specified string is not valid
    /// according to the specified rules.
    void set_auth(const std::string&);

    /// The specified string must not contain "?" or "#".
    ///
    /// \throw std::invalid_argument If the specified string is not valid
    /// according to the specified rules.
    void set_path(const std::string&);

    /// The specified string must either be empty or have a leading "?". Also,
    /// it must not contain "#".
    ///
    /// \throw std::invalid_argument If the specified string is not valid
    /// according to the specified rules.
    void set_query(const std::string&);

/*
    /// Set the query string to the serialized form of the specified set of
    /// query parameters. This is slightly faster than set_query(q.encode())
    /// because it avoids the validity check on the string.
    void set_query(const Params&);
*/

    /// The specified string must either be empty or have a leading "#".
    ///
    /// \throw std::invalid_argument If the specified string is not valid
    /// according to the specified rules.
    void set_frag(const std::string&);

    bool is_absolute() const;

private:
    std::string m_scheme, m_auth, m_path, m_query, m_frag;
};


/// uri_percent_encode() uri encodes a string as defined in according to
/// https://tools.ietf.org/html/rfc3986#section-2.1
/// The unescaped input must be UTF-8 encoded. uri_percent_encode() works
/// by replacing each UTF-8 character by three charatcers.
/// pct-encoded = "%" HEXDIG HEXDIG
/// where HEXDIG HEXDIG is the hexadecimal value of the character.
/// HEXDIG is a capital letter for A - F.
/// Unreserved chracters are not encoded.
/// unreseved = ALPHA / DIGIT / "-" / "." / "_" / "~"
///
/// uri_percent_decode() is the inverse of uri_percent_encode().
/// uri_percent_decode() throws std::runtime_error if the input
/// is invalid and cannot be decoded.
std::string uri_percent_encode(const std::string& unescaped);
std::string uri_percent_decode(const std::string& escaped);


// Implementation

inline Uri::Uri()
{
}

inline std::string Uri::recompose() const
{
    return m_scheme + m_auth + m_path + m_query + m_frag;
}

inline const std::string& Uri::get_scheme() const
{
    return m_scheme;
}

inline const std::string& Uri::get_auth() const
{
    return m_auth;
}

inline const std::string& Uri::get_path() const
{
    return m_path;
}

inline const std::string& Uri::get_query() const
{
    return m_query;
}

inline const std::string& Uri::get_frag() const
{
    return m_frag;
}

inline bool Uri::is_absolute() const
{
    return !m_scheme.empty();
}

} // namespace util
} // namespace realm

#endif // REALM_UTIL_URI_HPP
