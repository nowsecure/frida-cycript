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

static jmp_buf ctrlc_;

static void sigint(int) {
    longjmp(ctrlc_, 1);
}

static JSStringRef Result_;

void Run(const char *code, FILE *fout) { _pooled
    JSStringRef script(JSStringCreateWithUTF8CString(code));

    JSContextRef context(CYGetJSContext());

    JSValueRef exception(NULL);
    JSValueRef result(JSEvaluateScript(context, script, NULL, NULL, 0, &exception));
    JSStringRelease(script);

    if (exception != NULL) { error:
        result = exception;
        exception = NULL;
    }

    if (!JSValueIsUndefined(context, result)) {
        CYPool pool;
        const char *json;

        json = CYPoolJSONString(pool, context, result, &exception);
        if (exception != NULL)
            goto error;

        CYSetProperty(context, CYGetGlobalObject(context), Result_, result);

        if (fout != NULL) {
            fputs(json, fout);
            fputs("\n", fout);
            fflush(fout);
        }
    }
}

static void Console() {
    bool bypass(false);
    bool debug(false);

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
            fputs("\n", fout);
            fflush(fout);
            goto restart;
        }

      read:
        char *line(readline(prompt));
        if (line == NULL)
            break;

        if (!extra) {
            extra = true;
            if (line[0] == '\\') {
                std::string data(line + 1);
                if (data == "bypass") {
                    bypass = !bypass;
                    fprintf(fout, "bypass == %s\n", bypass ? "true" : "false");
                    fflush(fout);
                } else if (data == "debug") {
                    debug = !debug;
                    fprintf(fout, "debug == %s\n", debug ? "true" : "false");
                    fflush(fout);
                }
                add_history(line);
                goto restart;
            }
        }

        lines.push_back(line);
        command += line;
        free(line);

        std::string code;

        if (bypass)
            code = command;
        else {
            CYDriver driver("");
            cy::parser parser(driver);

            driver.data_ = command.c_str();
            driver.size_ = command.size();

            if (parser.parse() != 0 || !driver.errors_.empty()) {
                for (CYDriver::Errors::const_iterator i(driver.errors_.begin()); i != driver.errors_.end(); ++i) {
                    cy::position begin(i->location_.begin);
                    if (begin.line != lines.size() || begin.column - 1 != lines.back().size()) {
                        std::cerr << i->message_ << std::endl;
                        add_history(command.c_str());
                        goto restart;
                    }
                }

                driver.errors_.clear();

                command += '\n';
                prompt = "cy> ";
                goto read;
            }

            if (driver.source_ == NULL)
                goto restart;

            std::ostringstream str;
            driver.source_->Show(str);
            code = str.str();
        }

        add_history(command.c_str());

        if (debug)
            std::cout << code << std::endl;

        Run(code.c_str(), fout);
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

int main(int argc, const char *argv[]) {
    const char *script;

    if (argc == 1)
        script = NULL;
    else {
        CYSetArgs(argc - 1, argv + 1);
        script = argv[1];
    }

    Result_ = CYCopyJSString("_");

    if (script == NULL || strcmp(script, "-") == 0)
        Console();
    else {
        CYDriver driver(script);
        cy::parser parser(driver);

        size_t size;
        char *start(reinterpret_cast<char *>(Map(script, &size)));
        char *end(start + size);

        if (size >= 2 && start[0] == '#' && start[1] == '!') {
            start += 2;
            while (start != end && *start++ != '\n');
        }

        driver.data_ = start;
        driver.size_ = end - start;

        if (parser.parse() != 0 || !driver.errors_.empty()) {
            for (CYDriver::Errors::const_iterator i(driver.errors_.begin()); i != driver.errors_.end(); ++i)
                std::cerr << i->location_.begin << ": " << i->message_ << std::endl;
        } else if (driver.source_ != NULL) {
            std::ostringstream str;
            driver.source_->Show(str);
            std::string code(str.str());
            std::cout << code << std::endl;
            Run(code.c_str(), stdout);
        }
    }

    return 0;
}
