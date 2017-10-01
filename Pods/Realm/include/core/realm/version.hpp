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

#ifndef REALM_VERSION_HPP
#define REALM_VERSION_HPP

#include <string>

#include <realm/util/features.h>


#define REALM_VER_MAJOR 2
#define REALM_VER_MINOR 8
#define REALM_VER_PATCH 6
#define REALM_VER_EXTRA ""
#define REALM_PRODUCT_NAME "realm-core"

#define REALM_VER_STRING                                                                                             \
    REALM_QUOTE(REALM_VER_MAJOR) "." REALM_QUOTE(REALM_VER_MINOR) "." REALM_QUOTE(REALM_VER_PATCH)
#define REALM_VER_CHUNK "[" REALM_PRODUCT_NAME "-" REALM_VER_STRING "]"

namespace realm {

enum Feature {
    feature_Debug,
    feature_Replication,
};

class StringData;

class Version {
public:
    static int get_major()
    {
        return REALM_VER_MAJOR;
    }
    static int get_minor()
    {
        return REALM_VER_MINOR;
    }
    static int get_patch()
    {
        return REALM_VER_PATCH;
    }
    static StringData get_extra();
    static std::string get_version();
    static bool is_at_least(int major, int minor, int patch, StringData extra);
    static bool is_at_least(int major, int minor, int patch);
    static bool has_feature(Feature feature);
};


} // namespace realm

#endif // REALM_VERSION_HPP
