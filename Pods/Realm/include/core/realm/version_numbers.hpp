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

#ifndef REALM_VERSION_NUMBERS_HPP
#define REALM_VERSION_NUMBERS_HPP

// Do not use `cmakedefine` here, as certain versions can be 0, which CMake
// interprets as being undefined.
#define REALM_VERSION_MAJOR 6
#define REALM_VERSION_MINOR 1
#define REALM_VERSION_PATCH 4
#define REALM_VERSION_EXTRA ""
#define REALM_VERSION_STRING "6.1.4"

#endif // REALM_VERSION_NUMBERS_HPP
