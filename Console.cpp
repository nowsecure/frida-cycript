/* Cycript - Inlining/Optimizing JavaScript Compiler
 * Copyright (C) 2009  Jay Freeman (saurik)
*/

/* Modified BSD License {{{ */
/*
 *        Redistribution and use in source and binary
 * forms, with or without modification, are permitted
 * provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the
 *    above copyright notice, this list of conditions
 *    and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the
 *    above copyright notice, this list of conditions
 *    and the following disclaimer in the documentation
 *    and/or other materials provided with the
 *    distribution.
 * 3. The name of the author may not be used to endorse
 *    or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,
 * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
/* }}} */

#include "cycript.hpp"
#include "Context.hpp"

#ifdef CY_EXECUTE
#include "JavaScript.hpp"
#endif

#include <cstdio>
#include <sstream>

#include <setjmp.h>

#include <readline/readline.h>
#include <readline/history.h>

#include <sys/mman.h>

#include <errno.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "Cycript.tab.hh"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <pwd.h>

#include <apr_getopt.h>

#include <dlfcn.h>

static volatile enum {
    Working,
    Parsing,
    Running,
    Sending,
    Waiting,
} mode_;

static jmp_buf ctrlc_;

static void sigint(int) {
    switch (mode_) {
        case Working:
            return;
        case Parsing:
            longjmp(ctrlc_, 1);
        case Running:
            throw "*** Ctrl-C";
        case Sending:
            return;
        case Waiting:
            return;
    }
}

#if YYDEBUG
static bool bison_;
#endif
static bool strict_;
static bool pretty_;

void Setup(CYDriver &driver, cy::parser &parser) {
#if YYDEBUG
    if (bison_)
        parser.set_debug_level(1);
#endif
    if (strict_)
        driver.strict_ = true;
}

void Setup(CYOutput &out, CYDriver &driver) {
    out.pretty_ = pretty_;

    CYOptions options;
    CYContext context(driver.pool_, options);
    driver.program_->Replace(context);
}

void Run(int client, const char *data, size_t size, FILE *fout = NULL, bool expand = false) {
    CYPool pool;

    const char *json;
    if (client == -1) {
        mode_ = Running;
#ifdef CY_EXECUTE
        json = CYExecute(pool, data);
#else
        json = NULL;
#endif
        mode_ = Working;
        if (json != NULL)
            size = strlen(json);
    } else {
        mode_ = Sending;
        CYSendAll(client, &size, sizeof(size));
        CYSendAll(client, data, size);
        mode_ = Waiting;
        CYRecvAll(client, &size, sizeof(size));
        if (size == _not(size_t))
            json = NULL;
        else {
            char *temp(new(pool) char[size + 1]);
            CYRecvAll(client, temp, size);
            temp[size] = '\0';
            json = temp;
        }
        mode_ = Working;
    }

    if (json != NULL && fout != NULL) {
        if (!expand || json[0] != '"' && json[0] != '\'')
            fputs(json, fout);
        else for (size_t i(0); i != size; ++i)
            if (json[i] != '\\')
                fputc(json[i], fout);
            else switch(json[++i]) {
                case '\0': goto done;
                case '\\': fputc('\\', fout); break;
                case '\'': fputc('\'', fout); break;
                case '"': fputc('"', fout); break;
                case 'b': fputc('\b', fout); break;
                case 'f': fputc('\f', fout); break;
                case 'n': fputc('\n', fout); break;
                case 'r': fputc('\r', fout); break;
                case 't': fputc('\t', fout); break;
                case 'v': fputc('\v', fout); break;
                default: fputc('\\', fout); --i; break;
            }

      done:
        fputs("\n", fout);
        fflush(fout);
    }
}

void Run(int client, std::string &code, FILE *fout = NULL, bool expand = false) {
    Run(client, code.c_str(), code.size(), fout, expand);
}

int (*append_history$)(int, const char *);

static void Console(apr_pool_t *pool, int client) {
    passwd *passwd;
    if (const char *username = getenv("LOGNAME"))
        passwd = getpwnam(username);
    else
        passwd = getpwuid(getuid());

    const char *basedir(apr_psprintf(pool, "%s/.cycript", passwd->pw_dir));
    const char *histfile(apr_psprintf(pool, "%s/history", basedir));
    size_t histlines(0);

    mkdir(basedir, 0700);
    read_history(histfile);

    bool bypass(false);
    bool debug(false);
    bool expand(false);

    FILE *fout(stdout);

    rl_bind_key('\t', rl_insert);

    struct sigaction action;
    sigemptyset(&action.sa_mask);
    action.sa_handler = &sigint;
    action.sa_flags = 0;
    sigaction(SIGINT, &action, NULL);

    restart: for (;;) {
        std::string command;
        std::vector<std::string> lines;

        bool extra(false);
        const char *prompt("cy# ");

        if (setjmp(ctrlc_) != 0) {
            mode_ = Working;
            fputs("\n", fout);
            fflush(fout);
            goto restart;
        }

      read:
        mode_ = Parsing;
        char *line(readline(prompt));
        mode_ = Working;
        if (line == NULL)
            break;
        if (line[0] == '\0')
            goto read;

        if (!extra) {
            extra = true;
            if (line[0] == '?') {
                std::string data(line + 1);
                if (data == "bypass") {
                    bypass = !bypass;
                    fprintf(fout, "bypass == %s\n", bypass ? "true" : "false");
                    fflush(fout);
                } else if (data == "debug") {
                    debug = !debug;
                    fprintf(fout, "debug == %s\n", debug ? "true" : "false");
                    fflush(fout);
                } else if (data == "expand") {
                    expand = !expand;
                    fprintf(fout, "expand == %s\n", expand ? "true" : "false");
                    fflush(fout);
                }
                add_history(line);
                ++histlines;
                goto restart;
            }
        }

        command += line;

        char *begin(line), *end(line + strlen(line));
        while (char *nl = reinterpret_cast<char *>(memchr(begin, '\n', end - begin))) {
            *nl = '\0';
            lines.push_back(begin);
            begin = nl + 1;
        }

        lines.push_back(begin);

        free(line);

        std::string code;

        if (bypass)
            code = command;
        else {
            CYDriver driver("");
            cy::parser parser(driver);
            Setup(driver, parser);

            driver.data_ = command.c_str();
            driver.size_ = command.size();

            if (parser.parse() != 0 || !driver.errors_.empty()) {
                for (CYDriver::Errors::const_iterator error(driver.errors_.begin()); error != driver.errors_.end(); ++error) {
                    cy::position begin(error->location_.begin);
                    if (begin.line != lines.size() || begin.column - 1 != lines.back().size() || error->warning_) {
                        cy::position end(error->location_.end);

                        if (begin.line != lines.size()) {
                            std::cerr << "  | ";
                            std::cerr << lines[begin.line - 1] << std::endl;
                        }

                        std::cerr << "....";
                        for (size_t i(0); i != begin.column - 1; ++i)
                            std::cerr << '.';
                        if (begin.line != end.line || begin.column == end.column)
                            std::cerr << '^';
                        else for (size_t i(0), e(end.column - begin.column); i != e; ++i)
                            std::cerr << '^';
                        std::cerr << std::endl;

                        std::cerr << "  | ";
                        std::cerr << error->message_ << std::endl;

                        add_history(command.c_str());
                        ++histlines;
                        goto restart;
                    }
                }

                driver.errors_.clear();

                command += '\n';
                prompt = "cy> ";
                goto read;
            }

            if (driver.program_ == NULL)
                goto restart;

            if (client != -1)
                code = command;
            else {
                std::ostringstream str;
                CYOptions options;
                CYOutput out(str, options);
                Setup(out, driver);
                out << *driver.program_;
                code = str.str();
            }
        }

        add_history(command.c_str());
        ++histlines;

        if (debug)
            std::cout << code << std::endl;

        Run(client, code, fout, expand);
    }

    if (append_history$ != NULL) {
        _syscall(close(_syscall(open(histfile, O_CREAT | O_WRONLY, 0600))));
        (*append_history$)(histlines, histfile);
    } else {
        write_history(histfile);
    }

    fputs("\n", fout);
    fflush(fout);
}

static void *Map(const char *path, size_t *psize) {
    int fd;
    _syscall(fd = open(path, O_RDONLY));

    struct stat stat;
    _syscall(fstat(fd, &stat));
    size_t size(stat.st_size);

    *psize = size;

    void *base;
    _syscall(base = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0));

    _syscall(close(fd));
    return base;
}

void InjectLibrary(pid_t pid);

int Main(int argc, char const * const argv[], char const * const envp[]) {
    bool tty(isatty(STDIN_FILENO));
    bool compile(false);

    append_history$ = reinterpret_cast<int (*)(int, const char *)>(dlsym(RTLD_DEFAULT, "append_history"));

#ifdef CY_ATTACH
    pid_t pid(_not(pid_t));
#endif

    CYPool pool;
    apr_getopt_t *state;
    _aprcall(apr_getopt_init(&state, pool, argc, argv));

    for (;;) {
        char opt;
        const char *arg;

        apr_status_t status(apr_getopt(state,
            "cg:n:"
#ifdef CY_ATTACH
            "p:"
#endif
            "s"
        , &opt, &arg));

        switch (status) {
            case APR_EOF:
                goto getopt;
            case APR_BADCH:
            case APR_BADARG:
                fprintf(stderr,
                    "usage: cycript [-c]"
#ifdef CY_ATTACH
                    " [-p <pid|name>]"
#endif
                    " [<script> [<arg>...]]\n"
                );
                return 1;
            default:
                _aprcall(status);
        }

        switch (opt) {
            case 'c':
                compile = true;
            break;

            case 'g':
                if (false);
#if YYDEBUG
                else if (strcmp(arg, "bison") == 0)
                    bison_ = true;
#endif
                else {
                    fprintf(stderr, "invalid name for -g\n");
                    return 1;
                }
            break;

            case 'n':
                if (false);
                else if (strcmp(arg, "minify") == 0)
                    pretty_ = true;
                else {
                    fprintf(stderr, "invalid name for -n\n");
                    return 1;
                }
            break;

#ifdef CY_ATTACH
            case 'p': {
                size_t size(strlen(arg));
                char *end;

                pid = strtoul(arg, &end, 0);
                if (arg + size != end) {
                    // XXX: arg needs to be escaped in some horrendous way of doom
                    const char *command(apr_psprintf(pool, "ps axc|sed -e '/^ *[0-9]/{s/^ *\\([0-9]*\\)\\( *[^ ]*\\)\\{3\\} *-*\\([^ ]*\\)/\\3 \\1/;/^%s /{s/^[^ ]* //;q;};};d'", arg));

                    if (FILE *pids = popen(command, "r")) {
                        char value[32];
                        size = 0;

                        for (;;) {
                            size_t read(fread(value + size, 1, sizeof(value) - size, pids));
                            if (read == 0)
                                break;
                            else {
                                size += read;
                                if (size == sizeof(value)) {
                                    pid = _not(pid_t);
                                    goto fail;
                                }
                            }
                        }

                      size:
                        if (size == 0)
                            goto fail;
                        if (value[size - 1] == '\n') {
                            --size;
                            goto size;
                        }

                        value[size] = '\0';
                        size = strlen(value);
                        pid = strtoul(value, &end, 0);
                        if (value + size != end) fail:
                            pid = _not(pid_t);
                        _syscall(pclose(pids));
                    }

                    if (pid == _not(pid_t)) {
                        fprintf(stderr, "invalid pid for -p\n");
                        return 1;
                    }
                }
            } break;
#endif

            case 's':
                strict_ = true;
            break;
        }
    } getopt:;

    const char *script;
    int ind(state->ind);

#ifdef CY_ATTACH
    if (pid != _not(pid_t) && ind < argc - 1) {
        fprintf(stderr, "-p cannot set argv\n");
        return 1;
    }

    if (pid != _not(pid_t) && compile) {
        fprintf(stderr, "-p conflicts with -c\n");
        return 1;
    }
#endif

    if (ind == argc)
        script = NULL;
    else {
#ifdef CY_EXECUTE
        // XXX: const_cast?! wtf gcc :(
        CYSetArgs(argc - ind - 1, const_cast<const char **>(argv + ind + 1));
#endif
        script = argv[ind];
        if (strcmp(script, "-") == 0)
            script = NULL;
    }

#ifdef CY_ATTACH
    if (pid != _not(pid_t) && script == NULL && !tty) {
        fprintf(stderr, "non-terminal attaching to remote console\n");
        return 1;
    }
#endif

    int client;

#ifdef CY_ATTACH
    if (pid == _not(pid_t))
        client = -1;
    else {
        int server(_syscall(socket(PF_UNIX, SOCK_STREAM, 0))); try {
            struct sockaddr_un address;
            memset(&address, 0, sizeof(address));
            address.sun_family = AF_UNIX;

            sprintf(address.sun_path, "/tmp/.s.cy.%u", getpid());

            _syscall(bind(server, reinterpret_cast<sockaddr *>(&address), SUN_LEN(&address)));
            _syscall(chmod(address.sun_path, 0777));

            try {
                _syscall(listen(server, 1));
                InjectLibrary(pid);
                client = _syscall(accept(server, NULL, NULL));
            } catch (...) {
                // XXX: exception?
                unlink(address.sun_path);
                throw;
            }
        } catch (...) {
            _syscall(close(server));
            throw;
        }
    }
#else
    client = -1;
#endif

    if (script == NULL && tty)
        Console(pool, client);
    else {
        CYDriver driver(script ?: "<stdin>");
        cy::parser parser(driver);
        Setup(driver, parser);

        char *start, *end;

        if (script == NULL) {
            start = NULL;
            end = NULL;

            driver.file_ = stdin;
        } else {
            size_t size;
            start = reinterpret_cast<char *>(Map(script, &size));
            end = start + size;

            if (size >= 2 && start[0] == '#' && start[1] == '!') {
                start += 2;

                if (void *line = memchr(start, '\n', end - start))
                    start = reinterpret_cast<char *>(line);
                else
                    start = end;
            }

            driver.data_ = start;
            driver.size_ = end - start;
        }

        if (parser.parse() != 0 || !driver.errors_.empty()) {
            for (CYDriver::Errors::const_iterator i(driver.errors_.begin()); i != driver.errors_.end(); ++i)
                std::cerr << i->location_.begin << ": " << i->message_ << std::endl;
        } else if (driver.program_ != NULL)
            if (client != -1) {
                std::string code(start, end-start);
                Run(client, code, stdout);
            } else {
                std::ostringstream str;
                CYOptions options;
                CYOutput out(str, options);
                Setup(out, driver);
                out << *driver.program_;
                std::string code(str.str());
                if (compile)
                    std::cout << code;
                else
                    Run(client, code, stdout);
            }
    }

    return 0;
}

int main(int argc, char const * const argv[], char const * const envp[]) {
    apr_status_t status(apr_app_initialize(&argc, &argv, &envp));

    if (status != APR_SUCCESS) {
        fprintf(stderr, "apr_app_initialize() != APR_SUCCESS\n");
        return 1;
    } else try {
        return Main(argc, argv, envp);
    } catch (const CYException &error) {
        CYPool pool;
        fprintf(stderr, "%s\n", error.PoolCString(pool));
        return 1;
    }
}
