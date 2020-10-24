/*************************************************************************
 *
 * Copyright 2020 Realm Inc.
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

#ifndef REALM_UTIL_CF_STR_HPP
#define REALM_UTIL_CF_STR_HPP

#include <realm/util/features.h>

#if REALM_PLATFORM_APPLE

#include <CoreFoundation/CoreFoundation.h>

namespace realm {
namespace util {

inline std::string cfstring_to_std_string(CFStringRef cf_str)
{
    std::string ret;
    // If the CFString happens to store UTF-8 we can read its data directly
    if (const char *utf8 = CFStringGetCStringPtr(cf_str, kCFStringEncodingUTF8)) {
        ret = utf8;
        return ret;
    }

    // Otherwise we need to convert the CFString to UTF-8
    CFIndex length = CFStringGetLength(cf_str);
    CFIndex max_size = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8) + 1;
    ret.resize(max_size);
    CFStringGetCString(cf_str, &ret[0], max_size, kCFStringEncodingUTF8);
    ret.resize(strlen(ret.c_str()));
    return ret;
}

}
}

#endif // REALM_PLATFORM_APPLE

#endif // REALM_UTIL_CF_STR_HPP
