/* Cycript - Optimizing JavaScript Compiler/Runtime
 * Copyright (C) 2009-2013  Jay Freeman (saurik)
*/

/* GNU Lesser General Public License, Version 3 {{{ */
/*
 * Cycript is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * Cycript is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with Cycript.  If not, see <http://www.gnu.org/licenses/>.
**/
/* }}} */

(function(exports) {

var libcycript = dlopen("/usr/lib/libcycript.dylib", RTLD_NOLOAD);
if (libcycript == null) {
    exports.error = dlerror();
    return;
}

var CYHandleServer = dlsym(libcycript, "CYHandleServer");
if (CYHandleServer == null) {
    exports.error = dlerror();
    return;
}

var info = new Dl_info;
if (dladdr(CYHandleServer, info) == 0) {
    exports.error = dlerror();
    free(info);
    return;
}

var path = info->dli_fname;
free(info);

var slash = path.lastIndexOf('/');
if (slash == -1)
    return;

var libsubstrate = dlopen(path.substr(0, slash) + "/libsubstrate.dylib", RTLD_GLOBAL | RTLD_LAZY);
if (libsubstrate == null) {
    exports.error = dlerror();
    return;
}

MSGetImageByName = @encode(void *(const char *))(dlsym(libsubstrate, "MSGetImageByName"));
MSFindSymbol = @encode(void *(void *, const char *))(dlsym(libsubstrate, "MSFindSymbol"));
MSHookFunction = @encode(void(void *, void *, void **))(dlsym(libsubstrate, "MSHookFunction"));
MSHookMessageEx = @encode(void(Class, SEL, void *, void **))(dlsym(libsubstrate, "MSHookMessageEx"));

var slice = [].slice;

exports.getImageByName = MSGetImageByName;
exports.findSymbol = MSFindSymbol;

exports.hookFunction = function(func, hook, old) {
    var type = func.type;

    var pointer;
    if (old == null || typeof old === "undefined")
        pointer = null;
    else {
        pointer = new @encode(void **);
        *old = function() { return type(*pointer).apply(null, arguments); };
    }

    MSHookFunction(func.valueOf(), type(hook), pointer);
};

exports.hookMessage = function(isa, sel, imp, old) {
    var type = sel.type(isa);

    var pointer;
    if (old == null || typeof old === "undefined")
        pointer = null;
    else {
        pointer = new @encode(void **);
        *old = function() { return type(*pointer).apply(null, [this, sel].concat(slice.call(arguments))); };
    }

    MSHookMessageEx(isa, sel, type(function(self, sel) { return imp.apply(self, slice.call(arguments, 2)); }), pointer);
};

})(exports);
