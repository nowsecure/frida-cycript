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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dlfcn.h>
#include <unistd.h>

static void fail(const char *error) {
    perror(error);
    exit(1);
}

int main(int argc, const char *argv[]) {
    char buffer[1024];
    if (readlink("/proc/self/exe", buffer, sizeof(buffer)) == -1)
        fail("cannot read from /proc/self/exe");

    char *slash = strrchr(buffer, '/');
    if (slash == NULL)
        fail("could not find executable folder");

    strcpy(slash + 1, "cycript-a32");
    void *handle = dlopen(buffer, RTLD_LAZY);
    if (handle == NULL) {
        fprintf(stderr, "%s\n", dlerror());
        fail("could not load actual executable");
    }

    int (*$main)(int argc, const char *argv[]);
    $main = (int (*)(int, const char *[])) dlsym(handle, "main");
    if ($main == NULL)
        fail("could not locale main symbol");

    return $main(argc, argv);
}
