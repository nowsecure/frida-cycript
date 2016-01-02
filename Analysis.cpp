/* Cycript - The Truly Universal Scripting Language
 * Copyright (C) 2009-2016  Jay Freeman (saurik)
*/

/* GNU Affero General Public License, Version 3 {{{ */
/*
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.

 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
**/
/* }}} */

#define __USE_EXTERN_INLINES

#include <dlfcn.h>
#include <unistd.h>

#include <sys/stat.h>

#include <sqlite3.h>

#if CY_JAVA
#ifdef __APPLE__
#include <JavaVM/jni.h>
#else
#include <jni.h>
#endif
#endif

#if CY_RUBY
#ifdef __APPLE__
#include <Ruby/ruby.h>
#else
#include <ruby.h>
#endif
#endif

#if CY_PYTHON
#include <Python.h>
#endif

#if CY_OBJECTIVEC
#include <objc/runtime.h>
#endif

#ifdef __APPLE__
#include <AddressBook/AddressBook.h>
#include <CoreData/CoreData.h>
#include <CoreLocation/CoreLocation.h>
#include <Security/Security.h>

#if TARGET_OS_IPHONE
#include <UIKit/UIKit.h>
#else
#include <AppKit/AppKit.h>
#endif
#endif
