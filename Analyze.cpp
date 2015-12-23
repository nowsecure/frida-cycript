/* Cycript - Optimizing JavaScript Compiler/Runtime
 * Copyright (C) 2009-2015  Jay Freeman (saurik)
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

#include <cmath>
#include <cstring>
#include <iostream>
#include <map>
#include <sstream>
#include <string>

#include <clang-c/Index.h>

#include "Functor.hpp"
#include "Replace.hpp"
#include "Syntax.hpp"

static CXChildVisitResult CYVisit(CXCursor cursor, CXCursor parent, CXClientData arg) {
    (*reinterpret_cast<const Functor<void (CXCursor)> *>(arg))(cursor);
    return CXChildVisit_Continue;
}

static unsigned CYForChild(CXCursor cursor, const Functor<void (CXCursor)> &visitor) {
    return clang_visitChildren(cursor, &CYVisit, const_cast<void *>(static_cast<const void *>(&visitor)));
}

static bool CYOneChild(CXCursor cursor, const Functor<void (CXCursor)> &visitor) {
    bool visited(false);
    CYForChild(cursor, fun([&](CXCursor child) {
        _assert(!visited);
        visited = true;
        visitor(child);
    }));
    return visited;
}

struct CYCXString {
    CXString value_;

    CYCXString(CXString value) :
        value_(value)
    {
    }

    CYCXString(CXCursor cursor) :
        value_(clang_getCursorSpelling(cursor))
    {
    }

    CYCXString(CXCursorKind kind) :
        value_(clang_getCursorKindSpelling(kind))
    {
    }

    CYCXString(CXFile file) :
        value_(clang_getFileName(file))
    {
    }

    CYCXString(CXTranslationUnit unit, CXToken token) :
        value_(clang_getTokenSpelling(unit, token))
    {
    }

    ~CYCXString() {
        clang_disposeString(value_);
    }

    operator const char *() const {
        return clang_getCString(value_);
    }

    const char *Pool(CYPool &pool) const {
        return pool.strdup(*this);
    }

    bool operator ==(const char *rhs) const {
        const char *lhs(*this);
        return lhs == rhs || strcmp(lhs, rhs) == 0;
    }
};

template <void (&clang_get_Location)(CXSourceLocation, CXFile *, unsigned *, unsigned *, unsigned *) = clang_getSpellingLocation>
struct CYCXPosition {
    CXFile file_;
    unsigned line_;
    unsigned column_;
    unsigned offset_;

    CYCXPosition(CXSourceLocation location) {
        clang_get_Location(location, &file_, &line_, &column_, &offset_);
    }

    CYCXPosition(CXTranslationUnit unit, CXToken token) :
        CYCXPosition(clang_getTokenLocation(unit, token))
    {
    }

    CXSourceLocation Get(CXTranslationUnit unit) const {
        return clang_getLocation(unit, file_, line_, column_);
    }
};

template <void (&clang_get_Location)(CXSourceLocation, CXFile *, unsigned *, unsigned *, unsigned *)>
std::ostream &operator <<(std::ostream &out, const CYCXPosition<clang_get_Location> &position) {
    if (position.file_ != NULL)
        out << "[" << CYCXString(position.file_) << "]:";
    out << position.line_ << ":" << position.column_ << "@" << position.offset_;
    return out;
}

typedef std::map<std::string, std::string> CYKeyMap;

struct CYChildBaton {
    CXTranslationUnit unit;
    CYKeyMap &keys;

    CYChildBaton(CXTranslationUnit unit, CYKeyMap &keys) :
        unit(unit),
        keys(keys)
    {
    }
};

struct CYTokens {
  private:
    CXTranslationUnit unit_;
    CXToken *tokens_;
    unsigned count_;
    unsigned valid_;

  public:
    CYTokens(CXTranslationUnit unit, CXSourceRange range) :
        unit_(unit)
    {
        clang_tokenize(unit_, range, &tokens_, &count_);


        // libclang's tokenizer is horribly broken and returns "extra" tokens.
        // this code goes back through the tokens and filters for good ones :/

        CYCXPosition<> end(clang_getRangeEnd(range));
        CYCXString file(end.file_);

        for (valid_ = 0; valid_ != count_; ++valid_) {
            CYCXPosition<> position(unit, tokens_[valid_]);
            _assert(CYCXString(position.file_) == file);
            if (position.offset_ >= end.offset_)
                break;
        }
    }

    CYTokens(CXTranslationUnit unit, CXCursor cursor) :
        CYTokens(unit, clang_getCursorExtent(cursor))
    {
    }

    ~CYTokens() {
        clang_disposeTokens(unit_, tokens_, count_);
    }

    operator CXToken *() const {
        return tokens_;
    }

    size_t size() const {
        return valid_;
    }
};

static CYExpression *CYTranslateExpression(CXTranslationUnit unit, CXCursor cursor) {
    switch (CXCursorKind kind = clang_getCursorKind(cursor)) {
        case CXCursor_CallExpr: {
            CYExpression *function(NULL);
            CYList<CYArgument> arguments;
            CYForChild(cursor, fun([&](CXCursor child) {
                CYExpression *expression(CYTranslateExpression(unit, child));
                if (function == NULL)
                    function = expression;
                else
                    arguments->*$C_(expression);
            }));
            return $C(function, arguments);
        } break;

        case CXCursor_DeclRefExpr: {
            return $V(CYCXString(cursor).Pool($pool));
        } break;

        case CXCursor_IntegerLiteral: {
            // libclang doesn't provide any reasonable way to do this
            // note: clang_tokenize doesn't work if this is a macro
            // the token range starts inside the macro but ends after it
            // the tokenizer freaks out and either fails with 0 tokens
            // or returns some massive number of tokens ending here :/

            CXSourceRange range(clang_getCursorExtent(cursor));
            CYCXPosition<> start(clang_getRangeStart(range));
            CYCXPosition<> end(clang_getRangeEnd(range));
            CYCXString file(start.file_);
            _assert(file == CYCXString(end.file_));

            CYPool pool;
            size_t size;
            char *data(static_cast<char *>(CYPoolFile(pool, file, &size)));
            _assert(start.offset_ <= size && end.offset_ <= size && start.offset_ <= end.offset_);

            const char *token($pool.strndup(data + start.offset_, end.offset_ - start.offset_));
            double value(CYCastDouble(token));
            if (!std::isnan(value))
                return $ CYNumber(value);

            return $V(token);
        } break;

        case CXCursor_CStyleCastExpr:
            // XXX: most of the time, this is a "NoOp" integer cast; but we should check it

        case CXCursor_UnexposedExpr:
            // there is a very high probability that this is actually an "ImplicitCastExpr"
            // "Douglas Gregor" <dgregor@apple.com> err'd on the incorrect side of this one
            // http://lists.llvm.org/pipermail/cfe-commits/Week-of-Mon-20110926/046998.html

        case CXCursor_ParenExpr: {
            CYExpression *pass(NULL);
            CYOneChild(cursor, fun([&](CXCursor child) {
                pass = CYTranslateExpression(unit, child);
            }));
            return pass;
        } break;

        default:
            //std::cerr << "E:" << CYCXString(kind) << std::endl;
            _assert(false);
    }
}

static CYStatement *CYTranslateStatement(CXTranslationUnit unit, CXCursor cursor) {
    switch (CXCursorKind kind = clang_getCursorKind(cursor)) {
        case CXCursor_ReturnStmt: {
            CYExpression *value(NULL);
            CYOneChild(cursor, fun([&](CXCursor child) {
                value = CYTranslateExpression(unit, child);
            }));
            return $ CYReturn(value);
        } break;

        default:
            //std::cerr << "S:" << CYCXString(kind) << std::endl;
            _assert(false);
    }
}

static CYStatement *CYTranslateBlock(CXTranslationUnit unit, CXCursor cursor) {
    CYList<CYStatement> statements;
    CYForChild(cursor, fun([&](CXCursor child) {
        statements->*CYTranslateStatement(unit, child);
    }));
    return $ CYBlock(statements);
}

static CXChildVisitResult CYChildVisit(CXCursor cursor, CXCursor parent, CXClientData arg) {
    CYChildBaton &baton(*static_cast<CYChildBaton *>(arg));
    CXTranslationUnit &unit(baton.unit);

    CYCXString spelling(cursor);
    std::string name(spelling);
    std::ostringstream value;

    /*CXSourceLocation location(clang_getCursorLocation(cursor));
    CYCXPosition<> position(location);
    std::cout << spelling << " " << position << std::endl;*/

    switch (CXCursorKind kind = clang_getCursorKind(cursor)) {
        case CXCursor_EnumConstantDecl: {
            value << clang_getEnumConstantDeclValue(cursor);
        } break;

        case CXCursor_MacroDefinition: try {
            CXSourceRange range(clang_getCursorExtent(cursor));
            CYTokens tokens(unit, range);
            _assert(tokens.size() != 0);

            CXCursor cursors[tokens.size()];
            clang_annotateTokens(unit, tokens, tokens.size(), cursors);

            CYCXPosition<> start(clang_getRangeStart(range));
            CYCXString first(unit, tokens[1]);
            if (first == "(") {
                CYCXPosition<> paren(unit, tokens[1]);
                if (start.offset_ + strlen(spelling) == paren.offset_)
                    _assert(false); // XXX: support parameterized macros
            }

            for (unsigned i(1); i != tokens.size(); ++i) {
                CYCXString token(unit, tokens[i]);
                if (i != 1)
                    value << " ";
                value << token;
            }
        } catch (const CYException &error) {
            CYPool pool;
            //std::cerr << error.PoolCString(pool) << std::endl;
            goto skip;
        } break;

        case CXCursor_StructDecl: {
            if (!clang_isCursorDefinition(cursor))
                goto skip;
            if (spelling[0] == '\0')
                goto skip;

            std::ostringstream types;
            std::ostringstream names;

            CYForChild(cursor, fun([&](CXCursor child) {
                if (clang_getCursorKind(child) == CXCursor_FieldDecl) {
                    CXType type(clang_getCursorType(child));
                    types << "(typedef " << CYCXString(clang_getTypeSpelling(type)) << "),";
                    names << "'" << CYCXString(child) << "',";
                }
            }));

            name += "$cy";
            value << "new Type([" << types.str() << "],[" << names.str() << "])";
        } break;

        case CXCursor_TypedefDecl: {
            CXType type(clang_getTypedefDeclUnderlyingType(cursor));
            value << "(typedef " << CYCXString(clang_getTypeSpelling(type)) << ")";
        } break;

        case CXCursor_FunctionDecl:
        case CXCursor_VarDecl: try {
            std::string label;

            CYList<CYFunctionParameter> parameters;
            CYStatement *code(NULL);

            CYLocalPool local;

            CYForChild(cursor, fun([&](CXCursor child) {
                switch (CXCursorKind kind = clang_getCursorKind(child)) {
                    case CXCursor_AsmLabelAttr:
                        label = CYCXString(child);
                        break;

                    case CXCursor_CompoundStmt:
                        code = CYTranslateBlock(unit, child);
                        break;

                    case CXCursor_ParmDecl:
                        parameters->*$P($B($I(CYCXString(child).Pool($pool))));
                        break;

                    case CXCursor_IntegerLiteral:
                    case CXCursor_ObjCClassRef:
                    case CXCursor_TypeRef:
                    case CXCursor_UnexposedAttr:
                        break;

                    default:
                        //std::cerr << "A:" << CYCXString(child) << std::endl;
                        break;
                }
            }));

            if (label.empty()) {
                label = spelling;
                label = '_' + label;
            } else if (label[0] != '_')
                goto skip;

            if (code == NULL) {
                CXType type(clang_getCursorType(cursor));
                value << "*(typedef " << CYCXString(clang_getTypeSpelling(type)) << ").pointerTo()(dlsym(RTLD_DEFAULT,'" << label.substr(1) << "'))";
            } else {
                CYOptions options;
                CYOutput out(*value.rdbuf(), options);
                CYFunctionExpression *function($ CYFunctionExpression(NULL, parameters, code));
                function->Output(out, CYNoBFC);
                //std::cerr << value.str() << std::endl;
            }
        } catch (const CYException &error) {
            CYPool pool;
            //std::cerr << error.PoolCString(pool) << std::endl;
            goto skip;
        } break;

        default: {
            return CXChildVisit_Recurse;
        } break;
    }

    baton.keys[name] = value.str();

  skip:
    return CXChildVisit_Continue;
}

int main(int argc, const char *argv[]) {
    CXIndex index(clang_createIndex(0, 0));

    const char *file(argv[1]);

    unsigned offset(3);
#if CY_OBJECTIVEC
    argv[--offset] = "-ObjC++";
#endif

    CXTranslationUnit unit(clang_parseTranslationUnit(index, file, argv + offset, argc - offset, NULL, 0, CXTranslationUnit_DetailedPreprocessingRecord));

    for (unsigned i(0), e(clang_getNumDiagnostics(unit)); i != e; ++i) {
        CXDiagnostic diagnostic(clang_getDiagnostic(unit, i));
        CYCXString spelling(clang_getDiagnosticSpelling(diagnostic));
        std::cerr << spelling << std::endl;
    }

    CYKeyMap keys;
    CYChildBaton baton(unit, keys);
    clang_visitChildren(clang_getTranslationUnitCursor(unit), &CYChildVisit, &baton);

    for (CYKeyMap::const_iterator key(keys.begin()); key != keys.end(); ++key) {
        std::string value(key->second);
        for (size_t i(0), e(value.size()); i != e; ++i)
            if (value[i] <= 0 || value[i] >= 0x7f || value[i] == '\n')
                goto skip;
        std::cout << key->first << "|\"" << value << "\"" << std::endl;
    skip:; }

    clang_disposeTranslationUnit(unit);
    clang_disposeIndex(index);

    return 0;
}
