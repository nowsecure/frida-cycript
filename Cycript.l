%{
#include "Cycript.tab.hh"
typedef cy::parser::token tk;
%}

%option prefix="cy"
%option bison-bridge
%option bison-locations
%option noyywrap
%option yylineno
%option nounput
%option interactive

delim         [ \t]
whitesp       {delim}+
digit         [0-9]
number        [-]?{digit}*[.]?{digit}+

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

"break"      return tk::Break;
"case"       return tk::Case;
"catch"      return tk::Catch;
"continue"   return tk::Continue;
"default"    return tk::Default;
"delete"     return tk::Delete;
"do"         return tk::Do;
"else"       return tk::Else;
"false"      return tk::False;
"finally"    return tk::Finally;
"for"        return tk::For;
"function"   return tk::Function;
"if"         return tk::If;
"in"         return tk::In;
"instanceof" return tk::InstanceOf;
"new"        return tk::New;
"null"       return tk::Null;
"return"     return tk::Return;
"switch"     return tk::Switch;
"this"       return tk::This;
"throw"      return tk::Throw;
"true"       return tk::True;
"try"        return tk::Try;
"typeof"     return tk::TypeOf;
"var"        return tk::Var;
"void"       return tk::Void;
"while"      return tk::While;
"with"       return tk::With;

[a-zA-Z$_][a-zA-Z$_0-9]* return tk::Identifier;
[0-9]+                   return tk::NumericLiteral;

[ \t\n]                  ;
