#define _GNU_SOURCE

#include <substrate.h>
#include "cycript.hpp"

#include <cstdio>
#include <sstream>

#include <setjmp.h>

#include <readline/readline.h>
#include <readline/history.h>

#include "Cycript.tab.hh"

static jmp_buf ctrlc_;

void sigint(int) {
    longjmp(ctrlc_, 1);
}

int main(int argc, const char *argv[]) {
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

        _pooled

        JSStringRef script(JSStringCreateWithUTF8CString(code.c_str()));

        JSContextRef context(CYGetJSContext());

        JSValueRef exception(NULL);
        JSValueRef result(JSEvaluateScript(context, script, NULL, NULL, 0, &exception));
        JSStringRelease(script);

        if (exception != NULL)
            result = exception;

        if (JSValueIsUndefined(context, result))
            goto restart;

        CFStringRef json;

        @try { json:
            json = CYCopyJSONString(context, result);
        } @catch (id error) {
            CYThrow(context, error, &result);
            goto json;
        }

        fputs([reinterpret_cast<const NSString *>(json) UTF8String], fout);
        CFRelease(json);

        fputs("\n", fout);
        fflush(fout);
    }

    fputs("\n", fout);
    fflush(fout);

    return 0;
}
