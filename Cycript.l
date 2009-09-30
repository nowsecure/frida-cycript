%{
#include "Cycript.tab.hh"
typedef cy::parser::token tk;
#define YY_EXTRA_TYPE CYDriver *
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

"&"    return tk::Ampersand;
"&&"   return tk::AmpersandAmpersand;
"&="   return tk::AmpersandEqual;
"^"    return tk::Carrot;
"^="   return tk::CarrotEqual;
"="    return tk::Equal;
"=="   return tk::EqualEqual;
"==="  return tk::EqualEqualEqual;
"!"    return tk::Exclamation;
"!="   return tk::ExclamationEqual;
"!=="  return tk::ExclamationEqualEqual;
"-"    return tk::Hyphen;
"-="   return tk::HyphenEqual;
"--"   return tk::HyphenHyphen;
"->"   return tk::HyphenRight;
"<"    return tk::Left;
"<="   return tk::LeftEqual;
"<<"   return tk::LeftLeft;
"<<="  return tk::LeftLeftEqual;
"%"    return tk::Percent;
"%="   return tk::PercentEqual;
"."    return tk::Period;
"|"    return tk::Pipe;
"|="   return tk::PipeEqual;
"||"   return tk::PipePipe;
"+"    return tk::Plus;
"+="   return tk::PlusEqual;
"++"   return tk::PlusPlus;
">"    return tk::Right;
">="   return tk::RightEqual;
">>"   return tk::RightRight;
">>="  return tk::RightRightEqual;
">>>"  return tk::RightRightRight;
">>>=" return tk::RightRightRightEqual;
"/"    return tk::Slash;
"/="   return tk::SlashEqual;
"*"    return tk::Star;
"*="   return tk::StarEqual;
"~"    return tk::Tilde;

":"    return tk::Colon;
","    return tk::Comma;
"?"    return tk::Question;
";"    return tk::SemiColon;

"("    return tk::OpenParen;
")"    return tk::CloseParen;

"{"    return tk::OpenBrace;
"}"    return tk::CloseBrace;

"["    return tk::OpenBracket;
"]"    return tk::CloseBracket;

"break"      yylval->word_ = new CYWord("break"); return tk::Break;
"case"       yylval->word_ = new CYWord("case"); return tk::Case;
"catch"      yylval->word_ = new CYWord("catch"); return tk::Catch;
"continue"   yylval->word_ = new CYWord("continue"); return tk::Continue;
"default"    yylval->word_ = new CYWord("default"); return tk::Default;
"delete"     yylval->word_ = new CYWord("delete"); return tk::Delete;
"do"         yylval->word_ = new CYWord("do"); return tk::Do;
"else"       yylval->word_ = new CYWord("else"); return tk::Else;
"false"      yylval->false_ = new CYFalse(); return tk::False;
"finally"    yylval->word_ = new CYWord("finally"); return tk::Finally;
"for"        yylval->word_ = new CYWord("for"); return tk::For;
"function"   yylval->word_ = new CYWord("function"); return tk::Function;
"if"         yylval->word_ = new CYWord("if"); return tk::If;
"in"         yylval->word_ = new CYWord("in"); return tk::In;
"instanceof" yylval->word_ = new CYWord("instanceof"); return tk::InstanceOf;
"new"        yylval->word_ = new CYWord("new"); return tk::New;
"null"       yylval->null_ = new CYNull(); return tk::Null;
"return"     yylval->word_ = new CYWord("return"); return tk::Return;
"switch"     yylval->word_ = new CYWord("switch"); return tk::Switch;
"this"       yylval->this_ = new CYThis(); return tk::This;
"throw"      yylval->word_ = new CYWord("throw"); return tk::Throw;
"true"       yylval->true_ = new CYTrue(); return tk::True;
"try"        yylval->word_ = new CYWord("try"); return tk::Try;
"typeof"     yylval->word_ = new CYWord("typeof"); return tk::TypeOf;
"var"        yylval->word_ = new CYWord("var"); return tk::Var;
"void"       yylval->word_ = new CYWord("void"); return tk::Void;
"while"      yylval->word_ = new CYWord("while"); return tk::While;
"with"       yylval->word_ = new CYWord("with"); return tk::With;

[a-zA-Z$_][a-zA-Z$_0-9]* yylval->identifier_ = new CYIdentifier(apr_pstrmemdup(yyextra->pool_, yytext, yyleng)); return tk::Identifier;

(\.[0-9]+|(0|[1-9][0-9]*)(\.[0-9]*)?){Exponent}? yylval->number_ = new CYNumber(strtod(yytext, NULL)); return tk::NumericLiteral;

0[xX][0-9a-fA-F]+ yylval->number_ = new CYNumber(strtoull(yytext + 2, NULL, 16)); return tk::NumericLiteral;

0[bB][0-1]+ yylval->number_ = new CYNumber(strtoull(yytext + 2, NULL, 2)); return tk::NumericLiteral;

\"([^"\\\n]|{Escape})*\" return tk::StringLiteral;
'([^'\\\n]|{Escape})*' return tk::StringLiteral;

[ \t\n] ;

%%

void CYDriver::ScannerInit() {
    cylex_init(&scanner_);
    cyset_extra(this, scanner_);
}

void CYDriver::ScannerDestroy() {
    cylex_destroy(scanner_);
}
