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
#include "Syntax.hpp"

#include <sstream>

#include <nan.h>
#include <node.h>

using v8::Boolean;
using v8::Context;
using v8::FunctionTemplate;
using v8::Handle;
using v8::Object;
using v8::String;
using v8::Value;

namespace cylang {

static bool GetStringArg(CYPool &pool, Handle<Value> value, const char *&result);
static bool GetBoolArg(CYPool &pool, Handle<Value> value, bool &result);

static void Compile(const Nan::FunctionCallbackInfo<Value> &info) {
    if (info.Length() < 3) {
        Nan::ThrowTypeError("Bad argument count");
        return;
    }

    CYPool pool;

    const char *code;
    if (!GetStringArg(pool, info[0], code))
        return;

    bool strict;
    if (!GetBoolArg(pool, info[1], strict))
      return;

    bool pretty;
    if (!GetBoolArg(pool, info[2], pretty))
      return;

    try {
        std::stringbuf stream(code);
        CYDriver driver(pool, stream);
        driver.strict_ = strict;

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
        out.pretty_ = pretty;
        driver.Replace(options);
        out << *driver.script_;

        info.GetReturnValue().Set(Nan::New(str.str()).ToLocalChecked());
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

static bool GetBoolArg(CYPool &pool, Handle<Value> value, bool &result) {
    if (!value->IsBoolean()) {
        Nan::ThrowTypeError("Expected a boolean");
        return false;
    }

    result = value.As<Boolean>()->Value();
    return true;
}

static void DisposeAll(void *data);

static void InitAll(Handle<Object> exports, Handle<Value> module, Handle<Context> context) {
    exports->Set(Nan::New("compile").ToLocalChecked(), Nan::New<FunctionTemplate>(Compile)->GetFunction());

    node::AtExit(DisposeAll, nullptr);
}

static void DisposeAll(void *data) {
}

}

NODE_MODULE_CONTEXT_AWARE(cylang_binding, cylang::InitAll)
