#define _GNU_SOURCE

#include <substrate.h>
#include "cycript.h"

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
                goto restart;
            }
        }

        lines.push_back(line);
        command += line;
        free(line);

        CYDriver driver("");
        cy::parser parser(driver);

        driver.data_ = command.c_str();
        driver.size_ = command.size();

        if (parser.parse() != 0 || !driver.errors_.empty()) {
            for (CYDriver::Errors::const_iterator i(driver.errors_.begin()); i != driver.errors_.end(); ++i) {
                cy::position begin(i->location_.begin);
                if (begin.line != lines.size() || begin.column - 1 != lines.back().size()) {
                    std::cerr << i->message_ << std::endl;
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

        add_history(command.c_str());

        std::ostringstream str;
        driver.source_->Show(str);

        std::string code(str.str());
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
