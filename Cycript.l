%{
#include "Cycript.tab.hh"
typedef cy::parser::token tk;
#define YY_EXTRA_TYPE CYDriver *
#define N yylval->newline_ = yyextra->newline_; yyextra->newline_ = false; yyextra->restricted_ = false;
#define R yyextra->restricted_ = true;
#define L { bool restricted(yyextra->restricted_); yyextra->restricted_ = false; if (restricted) return tk::NewLine; else yyextra->newline_ = true; }
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
\/\*(\n|[^\*]|\*[^/])\*\/ if (memchr(yytext, '\n', yyleng) != NULL) L // XXX: supposedly I will be screwed on very very long multi-line comments and need to replace this with a manual lexer. http://websrv.cs.fsu.edu/~engelen/courses/COP5621/Pr2.pdf ; XXX: this rule doesn't work anyway, fucking A :(

"&"    N return tk::Ampersand;
"&&"   N return tk::AmpersandAmpersand;
"&="   N return tk::AmpersandEqual;
"^"    N return tk::Carrot;
"^="   N return tk::CarrotEqual;
"="    N return tk::Equal;
"=="   N return tk::EqualEqual;
"==="  N return tk::EqualEqualEqual;
"!"    N return tk::Exclamation;
"!="   N return tk::ExclamationEqual;
"!=="  N return tk::ExclamationEqualEqual;
"-"    N return tk::Hyphen;
"-="   N return tk::HyphenEqual;
"--"   N return yylval->newline_ ? tk::HyphenHyphen_ : tk::HyphenHyphen;
"->"   N return tk::HyphenRight;
"<"    N return tk::Left;
"<="   N return tk::LeftEqual;
"<<"   N return tk::LeftLeft;
"<<="  N return tk::LeftLeftEqual;
"%"    N return tk::Percent;
"%="   N return tk::PercentEqual;
"."    N return tk::Period;
"|"    N return tk::Pipe;
"|="   N return tk::PipeEqual;
"||"   N return tk::PipePipe;
"+"    N return tk::Plus;
"+="   N return tk::PlusEqual;
"++"   N return yylval->newline_ ? tk::PlusPlus_ : tk::PlusPlus;
">"    N return tk::Right;
">="   N return tk::RightEqual;
">>"   N return tk::RightRight;
">>="  N return tk::RightRightEqual;
">>>"  N return tk::RightRightRight;
">>>=" N return tk::RightRightRightEqual;
"/"    N return tk::Slash;
"/="   N return tk::SlashEqual;
"*"    N return tk::Star;
"*="   N return tk::StarEqual;
"~"    N return tk::Tilde;

":"    N return tk::Colon;
","    N return tk::Comma;
"?"    N return tk::Question;
";"    N return tk::SemiColon;

"("    N return tk::OpenParen;
")"    N return tk::CloseParen;

"{"    N return tk::OpenBrace;
"}"    N return tk::CloseBrace;

"["    N return tk::OpenBracket;
"]"    N return tk::CloseBracket;

"break"      N R yylval->word_ = new CYWord("break"); return tk::Break;
"case"       N yylval->word_ = new CYWord("case"); return tk::Case;
"catch"      N yylval->word_ = new CYWord("catch"); return tk::Catch;
"continue"   N R yylval->word_ = new CYWord("continue"); return tk::Continue;
"default"    N yylval->word_ = new CYWord("default"); return tk::Default;
"delete"     N yylval->word_ = new CYWord("delete"); return tk::Delete;
"do"         N yylval->word_ = new CYWord("do"); return tk::Do;
"else"       N yylval->word_ = new CYWord("else"); return tk::Else;
"false"      N yylval->false_ = new CYFalse(); return tk::False;
"finally"    N yylval->word_ = new CYWord("finally"); return tk::Finally;
"for"        N yylval->word_ = new CYWord("for"); return tk::For;
"function"   N yylval->word_ = new CYWord("function"); return tk::Function;
"if"         N yylval->word_ = new CYWord("if"); return tk::If;
"in"         N yylval->word_ = new CYWord("in"); return tk::In;
"instanceof" N yylval->word_ = new CYWord("instanceof"); return tk::InstanceOf;
"new"        N yylval->word_ = new CYWord("new"); return tk::New;
"null"       N yylval->null_ = new CYNull(); return tk::Null;
"return"     N R yylval->word_ = new CYWord("return"); return tk::Return;
"switch"     N yylval->word_ = new CYWord("switch"); return tk::Switch;
"this"       N yylval->this_ = new CYThis(); return tk::This;
"throw"      N R yylval->word_ = new CYWord("throw"); return tk::Throw;
"true"       N yylval->true_ = new CYTrue(); return tk::True;
"try"        N yylval->word_ = new CYWord("try"); return tk::Try;
"typeof"     N yylval->word_ = new CYWord("typeof"); return tk::TypeOf;
"var"        N yylval->word_ = new CYWord("var"); return tk::Var;
"void"       N yylval->word_ = new CYWord("void"); return tk::Void;
"while"      N yylval->word_ = new CYWord("while"); return tk::While;
"with"       N yylval->word_ = new CYWord("with"); return tk::With;

[a-zA-Z$_][a-zA-Z$_0-9]* yylval->identifier_ = new CYIdentifier(apr_pstrmemdup(yyextra->pool_, yytext, yyleng)); N return tk::Identifier;

(\.[0-9]+|(0|[1-9][0-9]*)(\.[0-9]*)?){Exponent}? yylval->number_ = new CYNumber(strtod(yytext, NULL)); N return tk::NumericLiteral;

0[xX][0-9a-fA-F]+ N yylval->number_ = new CYNumber(strtoull(yytext + 2, NULL, 16)); return tk::NumericLiteral;

0[bB][0-1]+ N yylval->number_ = new CYNumber(strtoull(yytext + 2, NULL, 2)); return tk::NumericLiteral;

\"([^"\\\n]|{Escape})*\" N return tk::StringLiteral;
'([^'\\\n]|{Escape})*' N return tk::StringLiteral;

\n L
[ \t] ;

%%

void CYDriver::ScannerInit() {
    cylex_init(&scanner_);
    cyset_extra(this, scanner_);
}

void CYDriver::ScannerDestroy() {
    cylex_destroy(scanner_);
}
