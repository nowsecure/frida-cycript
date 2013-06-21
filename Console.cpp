/* Cycript - Optimizing JavaScript Compiler/Runtime
 * Copyright (C) 2009-2013  Jay Freeman (saurik)
*/

/* GNU General Public License, Version 3 {{{ */
/*
 * Cycript is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, either version 3 of the License,
 * or (at your option) any later version.
 *
 * Cycript is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Cycript.  If not, see <http://www.gnu.org/licenses/>.
**/
/* }}} */

#include "cycript.hpp"

#ifdef CY_EXECUTE
#include "JavaScript.hpp"
#endif

#include <cstdio>
#include <sstream>

#include <setjmp.h>

#ifdef HAVE_READLINE_H
#include <readline.h>
#else
#include <readline/readline.h>
#endif

#ifdef HAVE_HISTORY_H
#include <history.h>
#else
#include <readline/history.h>
#endif

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

#include "Replace.hpp"
#include "Display.hpp"

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

void Setup(CYOutput &out, CYDriver &driver, CYOptions &options) {
    out.pretty_ = pretty_;
    CYContext context(options);
    driver.program_->Replace(context);
}

static CYUTF8String Run(CYPool &pool, int client, CYUTF8String code) {
    const char *json;
    size_t size;

    if (client == -1) {
        mode_ = Running;
#ifdef CY_EXECUTE
        json = CYExecute(pool, code);
#else
        json = NULL;
#endif
        mode_ = Working;
        if (json == NULL)
            size = 0;
        else
            size = strlen(json);
    } else {
        mode_ = Sending;
        size = code.size;
        CYSendAll(client, &size, sizeof(size));
        CYSendAll(client, code.data, code.size);
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

    return CYUTF8String(json, size);
}

static CYUTF8String Run(CYPool &pool, int client, const std::string &code) {
    return Run(pool, client, CYUTF8String(code.c_str(), code.size()));
}

static std::ostream *out_;

static void Write(bool syntax, const char *data, size_t size, std::ostream &out) {
    if (syntax)
        CYLexerHighlight(data, size, out);
    else
        out.write(data, size);
}

static void Output(bool syntax, CYUTF8String json, std::ostream *out, bool expand = false) {
    const char *data(json.data);
    size_t size(json.size);

    if (data == NULL || out == NULL)
        return;

    if (!expand ||
        data[0] != '@' && data[0] != '"' && data[0] != '\'' ||
        data[0] == '@' && data[1] != '"' && data[1] != '\''
    )
        Write(syntax, data, size, *out);
    else for (size_t i(0); i != size; ++i)
        if (data[i] != '\\')
            *out << data[i];
        else switch(data[++i]) {
            case '\0': goto done;
            case '\\': *out << '\\'; break;
            case '\'': *out << '\''; break;
            case '"': *out << '"'; break;
            case 'b': *out << '\b'; break;
            case 'f': *out << '\f'; break;
            case 'n': *out << '\n'; break;
            case 'r': *out << '\r'; break;
            case 't': *out << '\t'; break;
            case 'v': *out << '\v'; break;
            default: *out << '\\'; --i; break;
        }

  done:
    *out << std::endl;
}

static void Run(int client, bool syntax, const char *data, size_t size, std::ostream *out = NULL, bool expand = false) {
    CYPool pool;
    Output(syntax, Run(pool, client, CYUTF8String(data, size)), out, expand);
}

static void Run(int client, bool syntax, std::string &code, std::ostream *out = NULL, bool expand = false) {
    Run(client, syntax, code.c_str(), code.size(), out, expand);
}

int (*append_history$)(int, const char *);

static std::string command_;

static CYExpression *ParseExpression(CYUTF8String code) {
    std::stringstream stream;
    stream << '(' << code << ')';
    CYDriver driver(stream);

    cy::parser parser(driver);
    Setup(driver, parser);

    if (parser.parse() != 0 || !driver.errors_.empty())
        return NULL;

    CYOptions options;
    CYContext context(options);

    // XXX: this could be replaced with a CYStatement::Primitive()
    if (CYExpress *express = dynamic_cast<CYExpress *>(driver.program_->statements_))
        return express->expression_->Primitive(context);

    return NULL;
}

static int client_;

static char **Complete(const char *word, int start, int end) {
    rl_attempted_completion_over = TRUE;

    CYLocalPool pool;

    std::string line(rl_line_buffer, start);
    std::istringstream stream(command_ + line);
    CYDriver driver(stream);

    driver.auto_ = true;

    cy::parser parser(driver);
    Setup(driver, parser);

    if (parser.parse() != 0 || !driver.errors_.empty())
        return NULL;

    if (driver.mode_ == CYDriver::AutoNone)
        return NULL;

    CYExpression *expression;

    CYOptions options;
    CYContext context(options);

    std::ostringstream prefix;

    switch (driver.mode_) {
        case CYDriver::AutoPrimary:
            expression = $ CYThis();
        break;

        case CYDriver::AutoDirect:
            expression = driver.context_;
        break;

        case CYDriver::AutoIndirect:
            expression = $ CYIndirect(driver.context_);
        break;

        case CYDriver::AutoMessage: {
            CYDriver::Context &thing(driver.contexts_.back());
            expression = $M($C1($V("object_getClass"), thing.context_), $S("messages"));
            for (CYDriver::Context::Words::const_iterator part(thing.words_.begin()); part != thing.words_.end(); ++part)
                prefix << (*part)->word_ << ':';
        } break;

        default:
            _assert(false);
    }

    std::string begin(prefix.str());

    driver.program_ = $ CYProgram($ CYExpress($C3(ParseExpression(
    "   function(object, prefix, word) {\n"
    "       var names = [];\n"
    "       var before = prefix.length;\n"
    "       prefix += word;\n"
    "       var entire = prefix.length;\n"
    "       for (name in object)\n"
    "           if (name.substring(0, entire) == prefix)\n"
    "               names.push(name.substr(before));\n"
    "       return names;\n"
    "   }\n"
    ), expression, $S(begin.c_str()), $S(word))));

    driver.program_->Replace(context);

    std::ostringstream str;
    CYOutput out(str, options);
    out << *driver.program_;

    std::string code(str.str());
    CYUTF8String json(Run(pool, client_, code));
    // XXX: if this fails we should not try to parse it

    CYExpression *result(ParseExpression(json));
    if (result == NULL)
        return NULL;

    CYArray *array(dynamic_cast<CYArray *>(result));

    if (array == NULL) {
        *out_ << '\n';
        Output(false, json, out_);
        rl_forced_update_display();
        return NULL;
    }

    // XXX: use an std::set?
    typedef std::vector<std::string> Completions;
    Completions completions;

    std::string common;
    bool rest(false);

    CYForEach (element, array->elements_) {
        CYString *string(dynamic_cast<CYString *>(element->value_));
        _assert(string != NULL);

        std::string completion;
        if (string->size_ != 0)
            completion.assign(string->value_, string->size_);
        else if (driver.mode_ == CYDriver::AutoMessage)
            completion = "]";
        else
            continue;

        completions.push_back(completion);

        if (!rest) {
            common = completion;
            rest = true;
        } else {
            size_t limit(completion.size()), size(common.size());
            if (size > limit)
                common = common.substr(0, limit);
            else
                limit = size;
            for (limit = 0; limit != size; ++limit)
                if (common[limit] != completion[limit])
                    break;
            if (limit != size)
                common = common.substr(0, limit);
        }
    }

    size_t count(completions.size());
    if (count == 0)
        return NULL;

    size_t colon(common.find(':'));
    if (colon != std::string::npos)
        common = common.substr(0, colon + 1);
    if (completions.size() == 1)
        common += ' ';

    char **results(reinterpret_cast<char **>(malloc(sizeof(char *) * (count + 2))));

    results[0] = strdup(common.c_str());
    size_t index(0);
    for (Completions::const_iterator i(completions.begin()); i != completions.end(); ++i)
        results[++index] = strdup(i->c_str());
    results[count + 1] = NULL;

    return results;
}

// need char *, not const char *
static char name_[] = "cycript";
static char break_[] = " \t\n\"\\'`@$><=;|&{(" ")}" ".:[]";

static void Console(CYOptions &options) {
    CYPool pool;

    passwd *passwd;
    if (const char *username = getenv("LOGNAME"))
        passwd = getpwnam(username);
    else
        passwd = getpwuid(getuid());

    const char *basedir(pool.sprintf("%s/.cycript", passwd->pw_dir));
    const char *histfile(pool.sprintf("%s/history", basedir));
    size_t histlines(0);

    rl_initialize();
    rl_readline_name = name_;

    mkdir(basedir, 0700);
    read_history(histfile);

    bool bypass(false);
    bool debug(false);
    bool expand(false);
    bool syntax(true);

    out_ = &std::cout;

    // rl_completer_word_break_characters is broken in libedit
    rl_basic_word_break_characters = break_;

    rl_completer_word_break_characters = break_;
    rl_attempted_completion_function = &Complete;
    rl_bind_key('\t', rl_complete);

    struct sigaction action;
    sigemptyset(&action.sa_mask);
    action.sa_handler = &sigint;
    action.sa_flags = 0;
    sigaction(SIGINT, &action, NULL);

    restart: for (;;) {
        command_.clear();
        std::vector<std::string> lines;

        bool extra(false);
        const char *prompt("cy# ");

        if (setjmp(ctrlc_) != 0) {
            mode_ = Working;
            *out_ << std::endl;
            goto restart;
        }

      read:

#if RL_READLINE_VERSION >= 0x0600
        if (syntax) {
            rl_prep_term_function = CYDisplayStart;
            rl_redisplay_function = CYDisplayUpdate;
            rl_deprep_term_function = CYDisplayFinish;
        } else {
            rl_prep_term_function = rl_prep_terminal;
            rl_redisplay_function = rl_redisplay;
            rl_deprep_term_function = rl_deprep_terminal;
        }
#endif

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
                    *out_ << "bypass == " << (bypass ? "true" : "false") << std::endl;
                } else if (data == "debug") {
                    debug = !debug;
                    *out_ << "debug == " << (debug ? "true" : "false") << std::endl;
                } else if (data == "expand") {
                    expand = !expand;
                    *out_ << "expand == " << (expand ? "true" : "false") << std::endl;
                } else if (data == "syntax") {
                    syntax = !syntax;
                    *out_ << "syntax == " << (syntax ? "true" : "false") << std::endl;
                }
                add_history(line);
                ++histlines;
                goto restart;
            }
        }

        command_ += line;

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
            code = command_;
        else {
            CYLocalPool pool;

            std::istringstream stream(command_);
            CYDriver driver(stream);

            cy::parser parser(driver);
            Setup(driver, parser);

            if (parser.parse() != 0 || !driver.errors_.empty()) {
                for (CYDriver::Errors::const_iterator error(driver.errors_.begin()); error != driver.errors_.end(); ++error) {
                    cy::position begin(error->location_.begin);
                    if (begin.line != lines.size() || begin.column < lines.back().size() || error->warning_) {
                        cy::position end(error->location_.end);

                        if (begin.line != lines.size()) {
                            std::cerr << "  | ";
                            std::cerr << lines[begin.line - 1] << std::endl;
                        }

                        std::cerr << "....";
                        for (size_t i(0); i != begin.column; ++i)
                            std::cerr << '.';
                        if (begin.line != end.line || begin.column == end.column)
                            std::cerr << '^';
                        else for (size_t i(0), e(end.column - begin.column); i != e; ++i)
                            std::cerr << '^';
                        std::cerr << std::endl;

                        std::cerr << "  | ";
                        std::cerr << error->message_ << std::endl;

                        add_history(command_.c_str());
                        ++histlines;
                        goto restart;
                    }
                }

                driver.errors_.clear();

                command_ += '\n';
                prompt = "cy> ";
                goto read;
            }

            if (driver.program_ == NULL)
                goto restart;

            if (client_ != -1)
                code = command_;
            else {
                std::ostringstream str;
                CYOutput out(str, options);
                Setup(out, driver, options);
                out << *driver.program_;
                code = str.str();
            }
        }

        add_history(command_.c_str());
        ++histlines;

        if (debug) {
            Write(syntax, code.c_str(), code.size(), std::cout);
            std::cout << std::endl;
        }

        Run(client_, syntax, code, out_, expand);
    }

    if (append_history$ != NULL) {
        _syscall(close(_syscall(open(histfile, O_CREAT | O_WRONLY, 0600))));
        (*append_history$)(histlines, histfile);
    } else {
        write_history(histfile);
    }

    *out_ << std::endl;
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
    CYOptions options;

    append_history$ = (int (*)(int, const char *)) (dlsym(RTLD_DEFAULT, "append_history"));

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
                else if (strcmp(arg, "rename") == 0)
                    options.verbose_ = true;
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
                    const char *command(pool.sprintf("ps axc|sed -e '/^ *[0-9]/{s/^ *\\([0-9]*\\)\\( *[^ ]*\\)\\{3\\} *-*\\([^ ]*\\)/\\3 \\1/;/^%s /{s/^[^ ]* //;q;};};d'", arg));

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

#ifdef CY_ATTACH
    if (pid == _not(pid_t))
        client_ = -1;
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
                client_ = _syscall(accept(server, NULL, NULL));
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
    client_ = -1;
#endif

    if (script == NULL && tty)
        Console(options);
    else {
        CYLocalPool pool;

        char *start, *end;
        std::istream *indirect;

        if (script == NULL) {
            start = NULL;
            end = NULL;
            indirect = &std::cin;
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

            indirect = NULL;
        }

        CYStream direct(start, end);
        std::istream &stream(indirect == NULL ? direct : *indirect);
        CYDriver driver(stream, script ?: "<stdin>");

        cy::parser parser(driver);
        Setup(driver, parser);

        if (parser.parse() != 0 || !driver.errors_.empty()) {
            for (CYDriver::Errors::const_iterator i(driver.errors_.begin()); i != driver.errors_.end(); ++i)
                std::cerr << i->location_.begin << ": " << i->message_ << std::endl;
        } else if (driver.program_ != NULL)
            if (client_ != -1) {
                // XXX: this code means that you can't pipe to another process
                std::string code(start, end-start);
                Run(client_, false, code, &std::cout);
            } else {
                std::ostringstream str;
                CYOutput out(str, options);
                Setup(out, driver, options);
                out << *driver.program_;
                std::string code(str.str());
                if (compile)
                    std::cout << code;
                else
                    Run(client_, false, code, &std::cout);
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
