#include <cstring>
#include <iostream>
#include <map>
#include <sstream>
#include <string>

#include <clang-c/Index.h>

struct CYCXString {
    CXString value_;

    CYCXString(CXString value) :
        value_(value)
    {
    }

    ~CYCXString() {
        clang_disposeString(value_);
    }

    operator const char *() const {
        return clang_getCString(value_);
    }
};

struct CYFieldBaton {
    std::ostringstream types;
    std::ostringstream names;
};

static CXChildVisitResult CYFieldVisit(CXCursor cursor, CXCursor parent, CXClientData arg) {
    CYFieldBaton &baton(*static_cast<CYFieldBaton *>(arg));

    if (clang_getCursorKind(cursor) == CXCursor_FieldDecl) {
        CXType type(clang_getCursorType(cursor));
        baton.types << "(typedef " << CYCXString(clang_getTypeSpelling(type)) << "),";
        baton.names << "'" << CYCXString(clang_getCursorSpelling(cursor)) << "',";
    }

    return CXChildVisit_Continue;
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
    CXTranslationUnit unit;
    CXToken *tokens;
    unsigned count;

    CYTokens(CXTranslationUnit unit, CXCursor cursor) :
        unit(unit)
    {
        CXSourceRange range(clang_getCursorExtent(cursor));
        clang_tokenize(unit, range, &tokens, &count);
    }

    ~CYTokens() {
        clang_disposeTokens(unit, tokens, count);
    }

    operator CXToken *() const {
        return tokens;
    }
};

static CXChildVisitResult CYChildVisit(CXCursor cursor, CXCursor parent, CXClientData arg) {
    CYChildBaton &baton(*static_cast<CYChildBaton *>(arg));
    CXTranslationUnit &unit(baton.unit);

    CYCXString spelling(clang_getCursorSpelling(cursor));
    std::string name(spelling);
    std::ostringstream value;

    /*CXSourceLocation location(clang_getCursorLocation(cursor));

    CXFile file;
    unsigned line;
    unsigned column;
    unsigned offset;
    clang_getSpellingLocation(location, &file, &line, &column, &offset);

    if (file != NULL) {
        CYCXString path(clang_getFileName(file));
        std::cout << spelling << " " << path << ":" << line << std::endl;
    }*/

    switch (clang_getCursorKind(cursor)) {
        case CXCursor_EnumConstantDecl: {
            value << clang_getEnumConstantDeclValue(cursor);
        } break;

        case CXCursor_MacroDefinition: {
            CYTokens tokens(unit, cursor);
            if (tokens.count <= 2)
                goto skip;

            CXCursor cursors[tokens.count];
            clang_annotateTokens(unit, tokens, tokens.count, cursors);

            for (unsigned i(1); i != tokens.count - 1; ++i) {
                CYCXString token(clang_getTokenSpelling(unit, tokens[i]));
                if (i != 1)
                    value << " ";
                else if (strcmp(token, "(") == 0)
                    goto skip;
                value << token;
            }
        } break;

        case CXCursor_StructDecl: {
            if (!clang_isCursorDefinition(cursor))
                goto skip;
            if (spelling[0] == '\0')
                goto skip;

            CYFieldBaton baton;

            baton.types << "[";
            baton.names << "[";
            clang_visitChildren(cursor, &CYFieldVisit, &baton);
            baton.types << "]";
            baton.names << "]";

            name += "$cy";
            value << "new Type(" << baton.types.str() << "," << baton.names.str() << ")";
        } break;

        case CXCursor_TypedefDecl: {
            CXType type(clang_getTypedefDeclUnderlyingType(cursor));
            value << "(typedef " << CYCXString(clang_getTypeSpelling(type)) << ")";
        } break;

        case CXCursor_FunctionDecl:
        case CXCursor_VarDecl: {
            CXType type(clang_getCursorType(cursor));
            value << "*(typedef " << CYCXString(clang_getTypeSpelling(type)) << ").pointerTo()(dlsym(RTLD_DEFAULT,'" << spelling << "'))";
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

    CXTranslationUnit unit(clang_parseTranslationUnit(index, file, argv + offset, argc - offset, NULL, 0, CXTranslationUnit_DetailedPreprocessingRecord | CXTranslationUnit_SkipFunctionBodies));

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
