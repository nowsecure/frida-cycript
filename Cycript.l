%{
#include "Cycript.tab.hh"
typedef cy::parser::token tk;

#define YY_EXTRA_TYPE CYDriver *
#define YYLTYPE cy::location

#define T yylval->newline_ = yyextra->state_ == CYNewLine;
#define C T yyextra->state_ = CYClear;
#define R T yyextra->state_ = CYRestricted;

#define N \
    if (yyextra->state_ != CYNewLine) { \
        bool restricted(yyextra->state_ == CYRestricted); \
        if (restricted) { \
            yyextra->state_ = CYClear; \
            return tk::NewLine; \
        } else \
            yyextra->state_ = CYNewLine; \
    }

#define L { \
    yylloc->step(); \
    yylloc->columns(yyleng); \
}

int H(char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

#define YY_INPUT(data, value, size) { \
    if (yyextra->size_ == 0) \
        value = YY_NULL; \
    else { \
        size_t copy(std::min(size, yyextra->size_)); \
        memcpy(data, yyextra->data_, copy); \
        yyextra->data_ += copy; \
        yyextra->size_ -= copy; \
        value = copy; \
    } \
}

%}

%option prefix="cy"
%option bison-bridge
%option bison-locations
%option noyywrap
%option yylineno
%option nounput
%option interactive
%option reentrant

Exponent [eE][+-]?[0-9]+
Escape   \\[\\'"bfnrtv]|\\0|\\x[0-9a-fA-F]{2}|\\u[0-9a-fA-F]{4}

%%

\/\/[^\n]* ;
\/\*(\n|[^\*]|\*[^/])\*\/ if (memchr(yytext, '\n', yyleng) != NULL) N // XXX: supposedly I will be screwed on very very long multi-line comments and need to replace this with a manual lexer. http://websrv.cs.fsu.edu/~engelen/courses/COP5621/Pr2.pdf ; XXX: this rule doesn't work anyway, fucking A :(

"&"    L C return tk::Ampersand;
"&&"   L C return tk::AmpersandAmpersand;
"&="   L C return tk::AmpersandEqual;
"^"    L C return tk::Carrot;
"^="   L C return tk::CarrotEqual;
"="    L C return tk::Equal;
"=="   L C return tk::EqualEqual;
"==="  L C return tk::EqualEqualEqual;
"!"    L C return tk::Exclamation;
"!="   L C return tk::ExclamationEqual;
"!=="  L C return tk::ExclamationEqualEqual;
"-"    L C return tk::Hyphen;
"-="   L C return tk::HyphenEqual;
"--"   L C return yylval->newline_ ? tk::HyphenHyphen_ : tk::HyphenHyphen;
"->"   L C return tk::HyphenRight;
"<"    L C return tk::Left;
"<="   L C return tk::LeftEqual;
"<<"   L C return tk::LeftLeft;
"<<="  L C return tk::LeftLeftEqual;
"%"    L C return tk::Percent;
"%="   L C return tk::PercentEqual;
"."    L C return tk::Period;
"|"    L C return tk::Pipe;
"|="   L C return tk::PipeEqual;
"||"   L C return tk::PipePipe;
"+"    L C return tk::Plus;
"+="   L C return tk::PlusEqual;
"++"   L C return yylval->newline_ ? tk::PlusPlus_ : tk::PlusPlus;
">"    L C return tk::Right;
">="   L C return tk::RightEqual;
">>"   L C return tk::RightRight;
">>="  L C return tk::RightRightEqual;
">>>"  L C return tk::RightRightRight;
">>>=" L C return tk::RightRightRightEqual;
"/"    L C return tk::Slash;
"/="   L C return tk::SlashEqual;
"*"    L C return tk::Star;
"*="   L C return tk::StarEqual;
"~"    L C return tk::Tilde;

":"    L C return tk::Colon;
","    L C return tk::Comma;
"?"    L C return tk::Question;
";"    L C return tk::SemiColon;

"("    L C return tk::OpenParen;
")"    L C return tk::CloseParen;

"{"    L C return tk::OpenBrace;
"}"    L C return tk::CloseBrace;

"["    L C return tk::OpenBracket;
"]"    L C return tk::CloseBracket;

"@selector"  L C return tk::AtSelector;

"break"      L R yylval->word_ = new CYWord("break"); return tk::Break;
"case"       L C yylval->word_ = new CYWord("case"); return tk::Case;
"catch"      L C yylval->word_ = new CYWord("catch"); return tk::Catch;
"continue"   L R yylval->word_ = new CYWord("continue"); return tk::Continue;
"default"    L C yylval->word_ = new CYWord("default"); return tk::Default;
"delete"     L C yylval->word_ = new CYWord("delete"); return tk::Delete;
"do"         L C yylval->word_ = new CYWord("do"); return tk::Do;
"else"       L C yylval->word_ = new CYWord("else"); return tk::Else;
"false"      L C yylval->false_ = new CYFalse(); return tk::False;
"finally"    L C yylval->word_ = new CYWord("finally"); return tk::Finally;
"for"        L C yylval->word_ = new CYWord("for"); return tk::For;
"function"   L C yylval->word_ = new CYWord("function"); return tk::Function;
"if"         L C yylval->word_ = new CYWord("if"); return tk::If;
"in"         L C yylval->word_ = new CYWord("in"); return tk::In;
"instanceof" L C yylval->word_ = new CYWord("instanceof"); return tk::InstanceOf;
"new"        L C yylval->word_ = new CYWord("new"); return tk::New;
"null"       L C yylval->null_ = new CYNull(); return tk::Null;
"return"     L R yylval->word_ = new CYWord("return"); return tk::Return;
"switch"     L C yylval->word_ = new CYWord("switch"); return tk::Switch;
"this"       L C yylval->this_ = new CYThis(); return tk::This;
"throw"      L R yylval->word_ = new CYWord("throw"); return tk::Throw;
"true"       L C yylval->true_ = new CYTrue(); return tk::True;
"try"        L C yylval->word_ = new CYWord("try"); return tk::Try;
"typeof"     L C yylval->word_ = new CYWord("typeof"); return tk::TypeOf;
"var"        L C yylval->word_ = new CYWord("var"); return tk::Var;
"void"       L C yylval->word_ = new CYWord("void"); return tk::Void;
"while"      L C yylval->word_ = new CYWord("while"); return tk::While;
"with"       L C yylval->word_ = new CYWord("with"); return tk::With;

[a-zA-Z$_][a-zA-Z$_0-9]* yylval->identifier_ = new CYIdentifier(apr_pstrmemdup(yyextra->pool_, yytext, yyleng)); L C return tk::Identifier;

(\.[0-9]+|(0|[1-9][0-9]*)(\.[0-9]*)?){Exponent}? yylval->number_ = new CYNumber(strtod(yytext, NULL)); L C return tk::NumericLiteral;

0[xX][0-9a-fA-F]+ L C yylval->number_ = new CYNumber(strtoull(yytext + 2, NULL, 16)); return tk::NumericLiteral;

0[bB][0-1]+ L C yylval->number_ = new CYNumber(strtoull(yytext + 2, NULL, 2)); return tk::NumericLiteral;

\"([^"\\\n]|{Escape})*\"|'([^'\\\n]|{Escape})*' L C {
    char *value(reinterpret_cast<char *>(apr_palloc(yyextra->pool_, yyleng)));
    char *local(value);

    for (int i(1); i != yyleng - 1; ++i) {
        char next(yytext[i]);

        if (yytext[i] == '\\')
            switch (next = yytext[++i]) {
                case '\\': next = '\\'; break;
                case '\'': next = '\''; break;
                case '"': next = '"'; break;
                case 'b': next = '\b'; break;
                case 'f': next = '\f'; break;
                case 'n': next = '\n'; break;
                case 'r': next = '\r'; break;
                case 't': next = '\t'; break;
                case 'v': next = '\v'; break;
                case '0': next = '\0'; break;

                case 'x':
                    next = H(yytext[i + 1]) << 4 | H(yytext[i + 2]);
                    i += 2;
                break;
            }

        *local++ = next;
    }

    *local = '\0';
    yylval->string_ = new CYString(value, local - value);
    return tk::StringLiteral;
}

\n yylloc->end.lines(); yylloc->step(); N

[ \t] L
<<EOF>> L yyterminate();
. yyterminate();

%%

void CYDriver::ScannerInit() {
    cylex_init(&scanner_);
    cyset_extra(this, scanner_);
}

void CYDriver::ScannerDestroy() {
    cylex_destroy(scanner_);
}
