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
#ifndef REALM_UTIL_MISC_EXT_ERRORS_HPP
#define REALM_UTIL_MISC_EXT_ERRORS_HPP

#include <system_error>

namespace realm {
namespace util {

/// FIXME: The intention is that this enum will be merged into, and subsumed by
/// util::MiscErrors in `<realm/util/misc_errors.hpp>` in the core library.
enum class MiscExtErrors {
    /// End of input.
    end_of_input = 1,

    /// Premature end of input. That is, end of input at an unexpected, or
    /// illegal place in an input stream.
    premature_end_of_input,

    /// Delimiter not found.
    delim_not_found,

    /// Operation not supported
    operation_not_supported,
};

class MiscExtErrorCategory : public std::error_category {
public:
    const char* name() const noexcept override final;
    std::string message(int) const override final;
};

/// The error category associated with MiscErrors. The name of this category is
/// `realm.util.misc_ext`.
extern MiscExtErrorCategory misc_ext_error_category;

inline std::error_code make_error_code(MiscExtErrors err)
{
    return std::error_code(int(err), misc_ext_error_category);
}

} // namespace util
} // namespace realm

namespace std {

template<> class is_error_code_enum<realm::util::MiscExtErrors> {
public:
    static const bool value = true;
};

} // namespace std

#endif // REALM_UTIL_MISC_EXT_ERRORS_HPP
