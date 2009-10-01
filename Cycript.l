%{
#include "Cycript.tab.hh"
typedef cy::parser::token tk;
#define YY_EXTRA_TYPE CYDriver *

#define T yylval->newline_ = yyextra->state_ == CYNewLine;
#define C T yyextra->state_ = CYClear;
#define R T yyextra->state_ = CYRestricted;
#define N { bool restricted(yyextra->state_ == CYRestricted); if (restricted) { yyextra->state_ = CYClear; return tk::NewLine; } else yyextra->state_ = CYNewLine; }
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
Escape   \\['"\\bfnrtv]|\\0|\\x[0-9a-fA-F]{2}|\\u[0-9a-fA-F]{4}

%%

\/\/[^\n]* ;
\/\*(\n|[^\*]|\*[^/])\*\/ if (memchr(yytext, '\n', yyleng) != NULL) N // XXX: supposedly I will be screwed on very very long multi-line comments and need to replace this with a manual lexer. http://websrv.cs.fsu.edu/~engelen/courses/COP5621/Pr2.pdf ; XXX: this rule doesn't work anyway, fucking A :(

"&"    C return tk::Ampersand;
"&&"   C return tk::AmpersandAmpersand;
"&="   C return tk::AmpersandEqual;
"^"    C return tk::Carrot;
"^="   C return tk::CarrotEqual;
"="    C return tk::Equal;
"=="   C return tk::EqualEqual;
"==="  C return tk::EqualEqualEqual;
"!"    C return tk::Exclamation;
"!="   C return tk::ExclamationEqual;
"!=="  C return tk::ExclamationEqualEqual;
"-"    C return tk::Hyphen;
"-="   C return tk::HyphenEqual;
"--"   C return yylval->newline_ ? tk::HyphenHyphen_ : tk::HyphenHyphen;
"->"   C return tk::HyphenRight;
"<"    C return tk::Left;
"<="   C return tk::LeftEqual;
"<<"   C return tk::LeftLeft;
"<<="  C return tk::LeftLeftEqual;
"%"    C return tk::Percent;
"%="   C return tk::PercentEqual;
"."    C return tk::Period;
"|"    C return tk::Pipe;
"|="   C return tk::PipeEqual;
"||"   C return tk::PipePipe;
"+"    C return tk::Plus;
"+="   C return tk::PlusEqual;
"++"   C return yylval->newline_ ? tk::PlusPlus_ : tk::PlusPlus;
">"    C return tk::Right;
">="   C return tk::RightEqual;
">>"   C return tk::RightRight;
">>="  C return tk::RightRightEqual;
">>>"  C return tk::RightRightRight;
">>>=" C return tk::RightRightRightEqual;
"/"    C return tk::Slash;
"/="   C return tk::SlashEqual;
"*"    C return tk::Star;
"*="   C return tk::StarEqual;
"~"    C return tk::Tilde;

":"    C return tk::Colon;
","    C return tk::Comma;
"?"    C return tk::Question;
";"    C return tk::SemiColon;

"("    C return tk::OpenParen;
")"    C return tk::CloseParen;

"{"    C return tk::OpenBrace;
"}"    C return tk::CloseBrace;

"["    C return tk::OpenBracket;
"]"    C return tk::CloseBracket;

"break"      R yylval->word_ = new CYWord("break"); return tk::Break;
"case"       C yylval->word_ = new CYWord("case"); return tk::Case;
"catch"      C yylval->word_ = new CYWord("catch"); return tk::Catch;
"continue"   R yylval->word_ = new CYWord("continue"); return tk::Continue;
"default"    C yylval->word_ = new CYWord("default"); return tk::Default;
"delete"     C yylval->word_ = new CYWord("delete"); return tk::Delete;
"do"         C yylval->word_ = new CYWord("do"); return tk::Do;
"else"       C yylval->word_ = new CYWord("else"); return tk::Else;
"false"      C yylval->false_ = new CYFalse(); return tk::False;
"finally"    C yylval->word_ = new CYWord("finally"); return tk::Finally;
"for"        C yylval->word_ = new CYWord("for"); return tk::For;
"function"   C yylval->word_ = new CYWord("function"); return tk::Function;
"if"         C yylval->word_ = new CYWord("if"); return tk::If;
"in"         C yylval->word_ = new CYWord("in"); return tk::In;
"instanceof" C yylval->word_ = new CYWord("instanceof"); return tk::InstanceOf;
"new"        C yylval->word_ = new CYWord("new"); return tk::New;
"null"       C yylval->null_ = new CYNull(); return tk::Null;
"return"     R yylval->word_ = new CYWord("return"); return tk::Return;
"switch"     C yylval->word_ = new CYWord("switch"); return tk::Switch;
"this"       C yylval->this_ = new CYThis(); return tk::This;
"throw"      R yylval->word_ = new CYWord("throw"); return tk::Throw;
"true"       C yylval->true_ = new CYTrue(); return tk::True;
"try"        C yylval->word_ = new CYWord("try"); return tk::Try;
"typeof"     C yylval->word_ = new CYWord("typeof"); return tk::TypeOf;
"var"        C yylval->word_ = new CYWord("var"); return tk::Var;
"void"       C yylval->word_ = new CYWord("void"); return tk::Void;
"while"      C yylval->word_ = new CYWord("while"); return tk::While;
"with"       C yylval->word_ = new CYWord("with"); return tk::With;

[a-zA-Z$_][a-zA-Z$_0-9]* yylval->identifier_ = new CYIdentifier(apr_pstrmemdup(yyextra->pool_, yytext, yyleng)); C return tk::Identifier;

(\.[0-9]+|(0|[1-9][0-9]*)(\.[0-9]*)?){Exponent}? yylval->number_ = new CYNumber(strtod(yytext, NULL)); C return tk::NumericLiteral;

0[xX][0-9a-fA-F]+ C yylval->number_ = new CYNumber(strtoull(yytext + 2, NULL, 16)); return tk::NumericLiteral;

0[bB][0-1]+ C yylval->number_ = new CYNumber(strtoull(yytext + 2, NULL, 2)); return tk::NumericLiteral;

\"([^"\\\n]|{Escape})*\" C return tk::StringLiteral;
'([^'\\\n]|{Escape})*' C return tk::StringLiteral;

\n N
[ \t] ;

%%

void CYDriver::ScannerInit() {
    cylex_init(&scanner_);
    cyset_extra(this, scanner_);
}

void CYDriver::ScannerDestroy() {
    cylex_destroy(scanner_);
}
