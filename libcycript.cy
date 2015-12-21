/* Cycript - Optimizing JavaScript Compiler/Runtime
 * Copyright (C) 2009-2015  Jay Freeman (saurik)
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

var process = {
    env: {},
};

(function() {

let $cy_set = function(object, properties) {
    for (const name in properties)
        Object.defineProperty(object, name, {
            configurable: true,
            enumerable: false,
            writable: true,
            value: properties[name],
        });
};

const F_OK = 0;
const X_OK = (1<<0);
const W_OK = (1<<1);
const R_OK = (1<<2);

typedef long size_t;

extern "C" int access(const char *path, int amode);
extern "C" char *getcwd(char *buf, size_t size);
extern "C" int getpid();

$cy_set(Date.prototype, {
    toCYON: function() {
        return `new ${this.constructor.name}(${this.toUTCString().toCYON()})`;
    },
});

$cy_set(Error.prototype, {
    toCYON: function() {
        let stack = this.stack.split('\n');
        if (stack.slice(-1)[0] == "global code")
            stack = stack.slice(0, -1);
        for (let i = 0; i != stack.length; ++i)
            stack[i] = '\n    ' + stack[i];
        return `new ${this.constructor.name}(${this.message.toCYON()}) /*${stack.join('')} */`;
    },
});

let IsFile = function(path) {
    // XXX: this doesn't work on symlinks, but I don't want to fix stat :/
    return access(path, F_OK) == 0 && access(path + '/', F_OK) == -1;
};

let StartsWith = function(lhs, rhs) {
    return lhs.substring(0, rhs.length) == rhs;
};

let ResolveFile = function(exact, name) {
    if (exact && IsFile(name))
        return name;
    for (let suffix of ['.js', '.json'])
        if (IsFile(name + suffix))
            return name + suffix;
    return null;
};


let GetLibraryPath = function() {
    let handle = dlopen("/usr/lib/libcycript.dylib", RTLD_NOLOAD);
    if (handle == null)
        return null;

    try {
        let CYHandleServer = dlsym(handle, "CYHandleServer");
        if (CYHandleServer == null)
            return null;

        let info = new Dl_info;
        if (dladdr(CYHandleServer, info) == 0)
            return null;

        let path = info->dli_fname;
        let slash = path.lastIndexOf('/');
        if (slash == -1)
            return null;

        return path.substr(0, slash);
    } finally {
        dlclose(handle);
    }
};

let ResolveFolder = function(name) {
    if (access(name + '/', F_OK) == -1)
        return null;

    if (IsFile(name + "/package.json")) {
        let package = require(name + "/package.json");
        let path = ResolveFile(true, name + "/" + package.main);
        if (path != null)
            return path;
    }

    return ResolveFile(false, name + "/index");
};

let ResolveEither = function(name) {
    let path = null;
    if (path == null)
        path = ResolveFile(true, name);
    if (path == null)
        path = ResolveFolder(name);
    return path;
};

require.resolve = function(name) {
    if (StartsWith(name, '/')) {
        let path = ResolveEither(name);
        if (path != null)
            return path;
    } else {
        let cwd = new (typedef char[1024]);
        cwd = getcwd(cwd, cwd.length).toString();
        cwd = cwd.split('/');

        if (StartsWith(name, './') || StartsWith(name, '../')) {
            let path = ResolveEither(cwd + '/' + name);
            if (path != null)
                return path;
        } else {
            for (let i = cwd.length; i != 0; --i) {
                let modules = cwd.slice(0, i).concat("node_modules").join('/');
                let path = ResolveEither(modules + "/" + name);
                if (path != null)
                    return path;
            }

            let library = GetLibraryPath();
            let path = ResolveFile(true, library + "/cycript0.9/" + name + ".cy");
            if (path != null)
                return path;
        }
    }

    throw new Error("Cannot find module '" + name + "'");
};

var bindings = {};

process.binding = function(name) {
    let binding = bindings[name];
    if (typeof binding != 'undefined')
        return binding;

    switch (name) {
        case 'buffer': binding = {
            setupBufferJS() {
            },
        }; break;

        case 'cares_wrap': binding = {
        }; break;

        case 'constants': binding = {
        }; break;

        case 'fs': binding = {
            FSInitialize() {
            },

            lstat() {
                throw new Error("stat(" + arguments[0] + ")");
            },
        }; break;

        case 'pipe_wrap': binding = {
        }; break;

        case 'smalloc': binding = {
            alloc() {
            },
        }; break;

        case 'stream_wrap': binding = {
        }; break;

        case 'tcp_wrap': binding = {
        }; break;

        case 'timer_wrap': binding = {
            kOnTimeout: 0,
            Timer: {
            },
        }; break;

        case 'tty_wrap': binding = {
        }; break;

        case 'uv': binding = {
        }; break;

        default:
            throw new Error('No such module: ' + name);
    }

    bindings[name] = binding;
    return binding;
};

let environ = *(typedef char ***)(dlsym(RTLD_DEFAULT, "environ"));
for (let i = 0; environ[i] != null; ++i) {
    let assign = environ[i];
    let equal = assign.indexOf('=');
    let name = assign.substr(0, equal);
    let value = assign.substr(equal + 1);
    process.env[name.toString()] = value;
}

process.pid = getpid();

})();
