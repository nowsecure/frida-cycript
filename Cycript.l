%{
#include "Cycript.tab.h"
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

"&"    return CYTokenAmpersand;
"&&"   return CYTokenAmpersandAmpersand;
"&="   return CYTokenAmpersandEqual;
"^"    return CYTokenCarrot;
"^="   return CYTokenCarrotEqual;
"="    return CYTokenEqual;
"=="   return CYTokenEqualEqual;
"==="  return CYTokenEqualEqualEqual;
"!"    return CYTokenExclamation;
"!="   return CYTokenExclamationEqual;
"!=="  return CYTokenExclamationEqualEqual;
"-"    return CYTokenHyphen;
"-="   return CYTokenHyphenEqual;
"--"   return CYTokenHyphenHyphen;
"->"   return CYTokenHyphenRight;
"<"    return CYTokenLeft;
"<="   return CYTokenLeftEqual;
"<<"   return CYTokenLeftLeft;
"<<="  return CYTokenLeftLeftEqual;
"%"    return CYTokenPercent;
"%="   return CYTokenPercentEqual;
"."    return CYTokenPeriod;
"|"    return CYTokenPipe;
"|="   return CYTokenPipeEqual;
"||"   return CYTokenPipePipe;
"+"    return CYTokenPlus;
"+="   return CYTokenPlusEqual;
"++"   return CYTokenPlusPlus;
">"    return CYTokenRight;
">="   return CYTokenRightEqual;
">>"   return CYTokenRightRight;
">>="  return CYTokenRightRightEqual;
">>>"  return CYTokenRightRightRight;
">>>=" return CYTokenRightRightRightEqual;
"/"    return CYTokenSlash;
"/="   return CYTokenSlashEqual;
"*"    return CYTokenStar;
"*="   return CYTokenStarEqual;
"~"    return CYTokenTilde;

":"    return CYTokenColon;
","    return CYTokenComma;
"?"    return CYTokenQuestion;
";"    return CYTokenSemiColon;

"("    return CYTokenOpenParen;
")"    return CYTokenCloseParen;
"{"    return CYTokenOpenBrace;
"}"    return CYTokenCloseBrace;
"["    return CYTokenOpenBracket;
"]"    return CYTokenCloseBracket;

"break"      return CYTokenBreak;
"case"       return CYTokenCase;
"catch"      return CYTokenCatch;
"continue"   return CYTokenContinue;
"default"    return CYTokenDefault;
"delete"     return CYTokenDelete;
"do"         return CYTokenDo;
"else"       return CYTokenElse;
"false"      return CYTokenFalse;
"finally"    return CYTokenFinally;
"for"        return CYTokenFor;
"function"   return CYTokenFunction;
"if"         return CYTokenIf;
"in"         return CYTokenIn;
"instanceof" return CYTokenInstanceOf;
"new"        return CYTokenNew;
"null"       return CYTokenNull;
"return"     return CYTokenReturn;
"switch"     return CYTokenSwitch;
"this"       return CYTokenThis;
"throw"      return CYTokenThrow;
"true"       return CYTokenTrue;
"try"        return CYTokenTry;
"typeof"     return CYTokenTypeOf;
"var"        return CYTokenVar;
"void"       return CYTokenVoid;
"while"      return CYTokenWhile;
"with"       return CYTokenWith;

[a-zA-Z$_][a-zA-Z$_0-9]* return CYTokenIdentifier;
[0-9]+                   return CYTokenNumber;

[ \t\n]                  ;
