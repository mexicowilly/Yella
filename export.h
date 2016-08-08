/*
 * Copyright 2013-2016 Will Mason
 * 
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 * 
 *      http://www.apache.org/licenses/LICENSE-2.0
 * 
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#if !defined(EXPORT_H__)
#define EXPORT_H__

#if defined(yella_EXPORTS)

#if defined(_MSC_VER)
#define YELLA_EXPORT __declspec(dllexport)
#elif defined(__clang__) || (defined(__GNUC__) && __GNUC__ >= 4)
#define YELLA_EXPORT __attribute__ ((visibility ("default")))
#elif defined(__SUNPRO_C)
#define YELLA_EXPORT __global
#else
#define YELLA_EXPORT
#endif

#else

#if defined(_MSC_VER)
#define YELLA_EXPORT __declspec(dllimport)
#elif defined(__clang__) || (defined(__GNUC__) && __GNUC__ >= 4)
#define YELLA_EXPORT __attribute__ ((visibility ("default")))
#elif defined(__SUNPRO_C)
#define YELLA_EXPORT __global
#else
#define YELLA_EXPORT
#endif

#endif

#if !defined(__CYGWIN__) && (defined(__clang__) || (defined(__GNUC__) && __GNUC__ >= 4))
#define YELLA_NO_EXPORT __attribute__ ((visibility ("hidden")))
#elif defined(__SUNPRO_C)
#define YELLA_NO_EXPORT __hidden
#else
#define YELLA_NO_EXPORT
#endif

#endif
