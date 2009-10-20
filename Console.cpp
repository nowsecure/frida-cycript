/* Cycript - Remove Execution Server and Disassembler
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

#define _GNU_SOURCE

#include <substrate.h>
#include "cycript.hpp"

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

void Setup(CYDriver &driver, cy::parser &parser) {
#if YYDEBUG
    if (bison_)
        parser.set_debug_level(1);
#endif
    if (strict_)
        driver.strict_ = true;
}

void Run(int socket, const char *data, size_t size, FILE *fout = NULL, bool expand = false) {
    CYPool pool;

    const char *json;
    if (socket == -1) {
        mode_ = Running;
        json = CYExecute(pool, data);
        mode_ = Working;
        if (json != NULL)
            size = strlen(json);
    } else {
        mode_ = Sending;
        CYSendAll(socket, &size, sizeof(size));
        CYSendAll(socket, data, size);
        mode_ = Waiting;
        CYRecvAll(socket, &size, sizeof(size));
        if (size == _not(size_t))
            json = NULL;
        else {
            char *temp(new(pool) char[size + 1]);
            CYRecvAll(socket, temp, size);
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

void Run(int socket, std::string &code, FILE *fout = NULL, bool expand = false) {
    Run(socket, code.c_str(), code.size(), fout, expand);
}

static void Console(int socket) {
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

                        std::cerr << "  | ";
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

            if (socket != -1)
                code = command;
            else {
                std::ostringstream str;
                CYOutput out(str);
                driver.program_->Multiple(out);
                code = str.str();
            }
        }

        add_history(command.c_str());

        if (debug)
            std::cout << code << std::endl;

        Run(socket, code, fout, expand);
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

int main(int argc, char *argv[]) {
    bool tty(isatty(STDIN_FILENO));
    pid_t pid(_not(pid_t));
    bool compile(false);

    for (;;) switch (getopt(argc, argv, "cg:p:s")) {
        case -1:
            goto getopt;
        case '?':
            fprintf(stderr, "usage: cycript [-c] [-p <pid>] [<script> [<arg>...]]\n");
            return 1;

        case 'c':
            compile = true;
        break;

        case 'g':
            if (false);
#if YYDEBUG
            else if (strcmp(optarg, "bison") == 0)
                bison_ = true;
#endif
            else {
                fprintf(stderr, "invalid name for -g\n");
                return 1;
            }
        break;

        case 'p': {
            size_t size(strlen(optarg));
            char *end;
            pid = strtoul(optarg, &end, 0);
            if (optarg + size != end) {
                fprintf(stderr, "invalid pid for -p\n");
                return 1;
            }
        } break;

        case 's':
            strict_ = true;
        break;
    } getopt:;

    const char *script;

    if (pid != _not(pid_t) && optind < argc - 1) {
        fprintf(stderr, "-p cannot set argv\n");
        return 1;
    }

    if (pid != _not(pid_t) && compile) {
        fprintf(stderr, "-p conflicts with -c\n");
        return 1;
    }

    if (optind == argc)
        script = NULL;
    else {
        // XXX: const_cast?! wtf gcc :(
        CYSetArgs(argc - optind - 1, const_cast<const char **>(argv + optind + 1));
        script = argv[optind];
        if (strcmp(script, "-") == 0)
            script = NULL;
    }

    if (script == NULL && !tty && pid != _not(pid_t)) {
        fprintf(stderr, "non-terminal attaching to remove console\n");
        return 1;
    }

    int socket;

    if (pid == _not(pid_t))
        socket = -1;
    else {
        socket = _syscall(::socket(PF_UNIX, SOCK_STREAM, 0));

        struct sockaddr_un address;
        memset(&address, 0, sizeof(address));
        address.sun_family = AF_UNIX;
        sprintf(address.sun_path, "/tmp/.s.cy.%u", pid);

        _syscall(connect(socket, reinterpret_cast<sockaddr *>(&address), SUN_LEN(&address)));
    }

    if (script == NULL && tty)
        Console(socket);
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
            if (socket != -1)
                Run(socket, start, end - start, stdout);
            else {
                std::ostringstream str;
                CYOutput out(str);
                driver.program_->Multiple(out);
                std::string code(str.str());
                if (compile)
                    std::cout << code;
                else
                    Run(socket, code);
            }
    }

    return 0;
}
