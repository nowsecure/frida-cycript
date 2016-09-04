# frida-cycript

This is a fork of [Cycript] [1] in which we replaced its runtime with a brand
new runtime called [Mjølner] [3] powered by [Frida] [4]. This enables
frida-cycript to run on all the platforms and architectures maintained by
[frida-core] [9].

# Motivation

<img src="https://github.com/nowsecure/cycript/raw/master/demo.gif" width="583" />

[Cycript] [1] is an awesome interactive console for exploring and modifying
running applications on iOS, Mac, and Android. It was created by [@saurik] [2]
and essentially consists of four parts:

1. Its readline-based user interface;
2. Compiler that takes cylang as input and produces plain JavaScript as output;
3. A runtime that executes the plain JavaScript on JavaScriptCore, providing a
   set of APIs expected by the compiled scripts, plus some facilities for
   injecting itself into remote processes;
4. A couple of "user-space" modules written in cylang.

We didn't touch any other aspects of Cycript or did so with minimal changes.

We went out of our way to avoid touching the compiler, and also left the user
interface mostly untouched, only adding extra CLI switches for things like
device selection. We did, however, mostly rewrite the Cydia Substrate module
so existing scripts relying on this will get the portability and [performance
boost] [5] offered by Frida's instrumentation core.

We will be maintaining this fork and intend to stay in sync with user interface
and language improvements made upstream.

## FAQ

### What are some advantages of this fork?

WE believe the main advantage is portability, but also think you should consider:

- Ability to attach to sandboxed apps on Mac, without touching /usr or modifying
  the system in any way;
- Instead of crashing the process if you make a mistake and access a bad
  pointer, you will get a JavaScript exception;
- Frida's function hooking is able to hook many functions not supported by
  Cydia Substrate.

### What are some disadvantages?

Our runtime doesn't yet support all the features that upstream's runtime does,
but we are working hard to close this gap. Please file issues if something you
rely on isn't working as expected.

### Is Windows support planned?

Yes. You should already be able to do this by running frida-server on Windows
and connecting to it with Cycript on your UNIX system. (We didn't try this yet
so please tell us if and how it works for you.)

### How will this benefit existing Frida-users building their own tools?

We plan on improving [frida-compile] [7] to support cylang by integrating
the Cycript compiler with [Babel] [8]. This will allow you to mix in [our
runtime] [3] into your existing scripts.

## Status

Please see [our test-suite] [6] to get an overview of what we currently support.

## Building

### Mac

These instructions are a bit clunky for the time being.

First, enter the Frida build environment:

    git clone https://github.com/frida/frida.git
    cd frida
    git submodule init
    git submodule update
    make build/frida-env-mac-x86_64.rc
    . build/frida-env-mac-x86_64.rc

Install *bison* and *readline*:

    brew install bison readline

Mix them into your build environment:

    export CPPFLAGS="$CPPFLAGS -I/usr/local/opt/readline/include"
    export LDFLAGS="$LDFLAGS -L/usr/local/opt/readline/lib"
    export BISON=/usr/local/opt/bison/bin/bison
    export YACC=/usr/local/opt/bison/bin/yacc

Clone this repo:

    git clone https://github.com/nowsecure/cycript.git
    cd cycript
    git submodule init
    git submodule update

Run configure:

    ./configure --enable-static --with-libclang="-rpath /Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/lib /Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/lib/libclang.dylib" --with-python=/usr/bin/python-config

Build the UI and compiler:

    make -j8

Build the JavaScript runtime:

    npm install

Run Cycript:

    ./cycript

Run the test-suite:

    make && make -C build && DYLD_LIBRARY_PATH=$(pwd)/.libs node node_modules/mocha/bin/_mocha

  [1]: http://www.cycript.org/
  [2]: https://twitter.com/saurik
  [3]: https://github.com/nowsecure/mjolner
  [4]: http://www.frida.re/
  [5]: https://gist.github.com/oleavr/bfd9b65865e9f17914f2
  [6]: https://github.com/nowsecure/cycript/blob/master/test/types.js
  [7]: https://github.com/frida/frida-compile
  [8]: https://babeljs.io/
  [9]: https://github.com/frida/frida-core/tree/master/src
