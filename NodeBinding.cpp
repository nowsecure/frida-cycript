/* Cycript - The Truly Universal Scripting Language
 * Copyright (C) 2009-2016  Jay Freeman (saurik)
 * Copyright (C)      2016  NowSecure <oleavr@nowsecure.com>
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

#include "Driver.hpp"
#include "JavaScript.hpp"
#include "Syntax.hpp"

#include <sstream>

#include <nan.h>
#include <node.h>

using v8::Context;
using v8::FunctionTemplate;
using v8::Handle;
using v8::Object;
using v8::String;
using v8::Value;

namespace cynode {

static bool GetStringArg(CYPool &pool, Handle<Value> value, const char *&result);
static bool GetOptionalStringArg(CYPool &pool, Handle<Value> value, const char *&result);

static void Attach(const Nan::FunctionCallbackInfo<Value> &info) {
    if (info.Length() < 3) {
        Nan::ThrowTypeError("Bad argument count");
        return;
    }

    CYPool pool;

    const char *device_id;
    if (!GetOptionalStringArg(pool, info[0], device_id))
        return;

    const char *host;
    if (!GetOptionalStringArg(pool, info[1], host))
        return;

    const char *target;
    if (!GetOptionalStringArg(pool, info[2], target))
        return;

    try {
        CYAttach(device_id, host, target);
    } catch (const CYException &error) {
        Nan::ThrowError(error.PoolCString(pool));
    }
}

static void Execute(const Nan::FunctionCallbackInfo<Value> &info) {
    if (info.Length() < 1) {
        Nan::ThrowTypeError("Bad argument count");
        return;
    }

    CYPool pool;

    const char *command;
    if (!GetStringArg(pool, info[0], command))
        return;

    try {
        std::stringbuf stream(command);
        CYDriver driver(pool, stream);
        driver.strict_ = false;

        if (driver.Parse() || !driver.errors_.empty()) {
            for (CYDriver::Errors::const_iterator error(driver.errors_.begin()); error != driver.errors_.end(); ++error) {
                auto message(error->message_);
                Nan::ThrowError(message.c_str());
                return;
            }

            return;
        }

        if (driver.script_ == NULL)
            return;

        std::stringbuf str;
        CYOptions options;
        CYOutput out(str, options);
        out.pretty_ = false;
        driver.Replace(options);
        out << *driver.script_;
        auto code(str.str());

        auto json(CYExecute(pool, CYUTF8String(code.c_str(), code.size())));

        if (json != NULL)
          info.GetReturnValue().Set(Nan::New(json).ToLocalChecked());
        else
          info.GetReturnValue().Set(Nan::Null());
    } catch (const CYException &error) {
        Nan::ThrowError(error.PoolCString(pool));
    }
}

static bool GetStringArg(CYPool &pool, Handle<Value> value, const char *&result) {
    if (!value->IsString()) {
        Nan::ThrowTypeError("Expected a string");
        return false;
    }

    String::Utf8Value v(value.As<String>());
    result = pool.strdup(*v);
    return true;
}

static bool GetOptionalStringArg(CYPool &pool, Handle<Value> value, const char *&result) {
    if (value->IsString()) {
        String::Utf8Value v(value.As<String>());
        result = pool.strdup(*v);
        return true;
    }

    if (value->IsNull()) {
        result = nullptr;
        return true;
    }

    Nan::ThrowTypeError("Expected a string or null");
    return false;
}

static void DisposeAll(void *data);

static void InitAll(Handle<Object> exports, Handle<Value> module, Handle<Context> context) {
    exports->Set(Nan::New("attach").ToLocalChecked(), Nan::New<FunctionTemplate>(Attach)->GetFunction());
    exports->Set(Nan::New("execute").ToLocalChecked(), Nan::New<FunctionTemplate>(Execute)->GetFunction());

    node::AtExit(DisposeAll, nullptr);
}

static void DisposeAll(void *data) {
    CYDestroyContext();
}

}

NODE_MODULE_CONTEXT_AWARE(cycript_binding, cynode::InitAll)
