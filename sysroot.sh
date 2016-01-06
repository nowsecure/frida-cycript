#!/usr/bin/env bash

# Cycript - The Truly Universal Scripting Language
# Copyright (C) 2009-2016  Jay Freeman (saurik)

# GNU Affero General Public License, Version 3 {{{
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
# }}}

set -e

rm -rf sysroot.ios
mkdir -p sysroot.ios

for deb in ncurses_5.7-12 readline_6.0-7; do
    deb=${deb}_iphoneos-arm.deb
    [[ -f "${deb}" ]] || wget http://apt.saurik.com/debs/"${deb}"
    tar=data.tar.lzma
    ar -x "${deb}" "${tar}"
    PATH=/sw/bin:$PATH tar -C sysroot.ios -xf "${tar}"
    rm -f "${tar}"
done

mv sysroot.ios/usr/lib/_ncurses/* sysroot.ios/usr/lib
rmdir sysroot.ios/usr/lib/_ncurses

for h in jni.h jni_md.h; do
    ln -s /System/Library/Frameworks/JavaVM.framework/Versions/A/Headers/"${h}" sysroot.ios/usr/include
done
