%{
#include "Parser.hpp"
#include "Cycript.tab.h"
void cyerror(YYLTYPE *locp, CYParser *context, const char *msg);
int cylex(YYSTYPE *lvalp, YYLTYPE *llocp);
%}

%pure-parser
%name-prefix="cy"
%locations
%defines
%error-verbose

%parse-param { CYParser *context }

%token CYTokenAmpersand "&"
%token CYTokenAmpersandAmpersand "&&"
%token CYTokenAmpersandEqual "&="
%token CYTokenCarrot "^"
%token CYTokenCarrotEqual "^="
%token CYTokenEqual "="
%token CYTokenEqualEqual "=="
%token CYTokenEqualEqualEqual "==="
%token CYTokenExclamation "!"
%token CYTokenExclamationEqual "!="
%token CYTokenExclamationEqualEqual "!=="
%token CYTokenHyphen "-"
%token CYTokenHyphenEqual "-="
%token CYTokenHyphenHyphen "--"
%token CYTokenHyphenRight "->"
%token CYTokenLeft "<"
%token CYTokenLeftEqual "<="
%token CYTokenLeftLeft "<<"
%token CYTokenLeftLeftEqual "<<="
%token CYTokenPercent "%"
%token CYTokenPercentEqual "%="
%token CYTokenPeriod "."
%token CYTokenPipe "|"
%token CYTokenPipeEqual "|="
%token CYTokenPipePipe "||"
%token CYTokenPlus "+"
%token CYTokenPlusEqual "+="
%token CYTokenPlusPlus "++"
%token CYTokenRight ">"
%token CYTokenRightEqual ">="
%token CYTokenRightRight ">>"
%token CYTokenRightRightEqual ">>="
%token CYTokenRightRightRight ">>>"
%token CYTokenRightRightRightEqual ">>>="
%token CYTokenSlash "/"
%token CYTokenSlashEqual "/="
%token CYTokenStar "*"
%token CYTokenStarEqual "*="
%token CYTokenTilde "~"

%token CYTokenBreak "break"
%token CYTokenCase "case"
%token CYTokenCatch "catch"
%token CYTokenContinue "continue"
%token CYTokenDefault "default"
%token CYTokenDelete "delete"
%token CYTokenDo "do"
%token CYTokenElse "else"
%token CYTokenFalse "false"
%token CYTokenFinally "finally"
%token CYTokenFor "for"
%token CYTokenFunction "function"
%token CYTokenIf "if"
%token CYTokenIn "in"
%token CYTokenInstanceOf "instanceof"
%token CYTokenNew "new"
%token CYTokenNull "null"
%token CYTokenReturn "return"
%token CYTokenSwitch "switch"
%token CYTokenThis "this"
%token CYTokenThrow "throw"
%token CYTokenTrue "true"
%token CYTokenTry "try"
%token CYTokenTypeOf "typeof"
%token CYTokenVar "var"
%token CYTokenVoid "void"
%token CYTokenWhile "while"
%token CYTokenWith "with"

%%

hello: ;

%%

#include <stdio.h>

void cyerror(YYLTYPE *locp, CYParser *context, const char *msg) {
    fprintf(stderr, "err:%s\n", msg);
}
