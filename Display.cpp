/* Cycript - Optimizing JavaScript Compiler/Runtime
 * Copyright (C) 2009-2012  Jay Freeman (saurik)
*/

/* GNU Lesser General Public License, Version 3 {{{ */
/*
 * Cycript is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * Cycript is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with Cycript.  If not, see <http://www.gnu.org/licenses/>.
**/
/* }}} */

#include <complex>
#include <sstream>

#ifdef HAVE_READLINE_H
#include <readline.h>
#else
#include <readline/readline.h>
#endif

#include <sys/ioctl.h>

#include "Highlight.hpp"

#include <term.h>

typedef std::complex<int> CYCursor;

CYCursor current_;
int width_;
size_t point_;

unsigned CYDisplayWidth() {
    struct winsize info;
    if (ioctl(1, TIOCGWINSZ, &info) != -1)
        return info.ws_col;
    return tgetnum(const_cast<char *>("co"));
}

void CYDisplayOutput_(int (*put)(int), const char *&data) {
    for (;; ++data) {
        char next(*data);
        if (next == '\0' || next == CYIgnoreEnd)
            return;
        if (put != NULL)
            put(next);
    }
}

CYCursor CYDisplayOutput(int (*put)(int), int width, const char *data, ssize_t offset = 0) {
    CYCursor point(current_);

    for (;;) {
        if (offset-- == 0)
            point = current_;

        char next(*data++);
        switch (next) {
            case '\0':
                return point;
            break;

            case CYIgnoreStart:
                CYDisplayOutput_(put, data);
            case CYIgnoreEnd:
                ++offset;
            break;

            default:
                current_ += CYCursor(0, 1);
                if (current_.imag() == width)
            case '\n':
                    current_ = CYCursor(current_.real() + 1, 0);
                if (put != NULL)
                    put(next);
            break;

        }
    }
}

void CYDisplayMove_(char *negative, char *positive, int offset) {
    if (offset < 0)
        putp(tparm(negative, -offset));
    else if (offset > 0)
        putp(tparm(positive, offset));
}

void CYDisplayMove(CYCursor target) {
    CYCursor offset(target - current_);

    CYDisplayMove_(parm_up_cursor, parm_down_cursor, offset.real());

    if (char *parm = tparm(column_address, target.imag()))
        putp(parm);
    else
        CYDisplayMove_(parm_left_cursor, parm_right_cursor, offset.imag());

    current_ = target;
}

void CYDisplayStart(int meta) {
    rl_prep_terminal(meta);
    current_ = CYCursor();
}

void CYDisplayUpdate() {
#if RL_READLINE_VERSION >= 0x0600
    const char *prompt(rl_display_prompt);
#else
    const char *prompt(rl_prompt);
#endif

    std::ostringstream stream;
    CYLexerHighlight(rl_line_buffer, rl_end, stream, true);
    std::string string(stream.str());
    const char *buffer(string.c_str());

    int width(CYDisplayWidth());
    if (width_ != width) {
        current_ = CYCursor();
        CYDisplayOutput(NULL, width, prompt);
        current_ = CYDisplayOutput(NULL, width, buffer, point_);
    }

    CYDisplayMove(CYCursor());
    CYDisplayOutput(putchar, width, prompt);
    CYCursor target(CYDisplayOutput(putchar, width, stream.str().c_str(), rl_point));

    if (current_.imag() == 0)
        CYDisplayOutput(putchar, width, " ");
    putp(clr_eos);

    CYDisplayMove(target);
    fflush(stdout);

    width_ = width;
    point_ = rl_point;
}

void CYDisplayFinish() {
    rl_deprep_terminal();
}
