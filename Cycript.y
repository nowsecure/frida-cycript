%code top {
#include "Parser.hpp"
#include "Cycript.tab.h"
void cyerror(YYLTYPE *locp, CYParser *context, const char *msg);
int cylex(YYSTYPE *lvalp, YYLTYPE *llocp);
}

%name-prefix "cy"

%locations
%define api.pure
%glr-parser

%defines

%debug
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

%token CYTokenColon ":"
%token CYTokenComma ","
%token CYTokenQuestion "?"
%token CYTokenSemiColon ";"

%token CYTokenOpenParen "("
%token CYTokenCloseParen ")"
%token CYTokenOpenBrace "{"
%token CYTokenCloseBrace "}"
%token CYTokenOpenBracket "["
%token CYTokenCloseBracket "]"

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

%token CYTokenIdentifier
%token CYTokenNumber
%token CYTokenString

%%

Start
    : Program
    ;

IdentifierOpt
    : Identifier
    |
    ;

Identifier
    : CYTokenIdentifier
    ;

Literal
    : NullLiteral
    | BooleanLiteral
    | NumericLiteral
    | StringLiteral
    ;

NullLiteral
    : "null"
    ;

BooleanLiteral
    : "true"
    | "false"
    ;

NumericLiteral
    : CYTokenNumber
    ;

StringLiteral
    : CYTokenString
    ;

/* Objective-C Extensions {{{ */
VariadicCall
    : "," AssignmentExpression VariadicCall
    |
    ;

SelectorCall_
    : SelectorCall
    | VariadicCall
    ;

SelectorCall
    : IdentifierOpt ":" AssignmentExpression SelectorCall_
    ;

SelectorList
    : SelectorCall
    | Identifier
    ;

ObjectiveCall
    : "[" AssignmentExpression SelectorList "]"
    ;
/* }}} */

/* 11.1 Primary Expressions {{{ */
PrimaryExpression
    : "this"
    | Identifier
    | Literal
    | ArrayLiteral
    | ObjectLiteral
    | "(" Expression ")"
    | ObjectiveCall
    ;
/* }}} */
/* 11.1.4 Array Initialiser {{{ */
ArrayLiteral
    : "[" ElementList "]"
    ;

Element
    : AssignmentExpression
    |
    ;

ElementList_
    : "," ElementList
    |
    ;

ElementList
    : Element ElementList_
    ;
/* }}} */
/* 11.1.5 Object Initialiser {{{ */
ObjectLiteral
    : "{" PropertyNameAndValueListOpt "}"
    ;

PropertyNameAndValueList_
    : "," PropertyNameAndValueList
    |
    ;

PropertyNameAndValueListOpt
    : PropertyNameAndValueList
    |
    ;

PropertyNameAndValueList
    : PropertyName ":" AssignmentExpression PropertyNameAndValueList_
    ;

PropertyName
    : Identifier
    | StringLiteral
    | NumericLiteral
    ;
/* }}} */

MemberExpression
    : PrimaryExpression
    | FunctionExpression
    | MemberExpression "[" Expression "]"
    | MemberExpression "." Identifier
    | "new" MemberExpression Arguments
    ;

NewExpression
    : MemberExpression
    | "new" NewExpression
    ;

CallExpression
    : MemberExpression Arguments
    | CallExpression Arguments
    | CallExpression "[" Expression "]"
    | CallExpression "." Identifier
    ;

ArgumentList_
    : "," ArgumentList
    |
    ;

ArgumentListOpt
    : ArgumentList
    |
    ;

ArgumentList
    : AssignmentExpression ArgumentList_
    ;

Arguments
    : "(" ArgumentListOpt ")"
    ;

LeftHandSideExpression
    : NewExpression
    | CallExpression
    ;

PostfixExpression
    : LeftHandSideExpression
    | LeftHandSideExpression "++"
    | LeftHandSideExpression "--"
    ;

UnaryExpression
    : PostfixExpression
    | "delete" UnaryExpression
    | "void" UnaryExpression
    | "typeof" UnaryExpression
    | "++" UnaryExpression
    | "--" UnaryExpression
    | "+" UnaryExpression
    | "-" UnaryExpression
    | "~" UnaryExpression
    | "!" UnaryExpression
    | "*" UnaryExpression
    | "&" UnaryExpression
    ;

MultiplicativeExpression
    : UnaryExpression
    | MultiplicativeExpression "*" UnaryExpression
    | MultiplicativeExpression "/" UnaryExpression
    | MultiplicativeExpression "%" UnaryExpression
    ;

AdditiveExpression
    : MultiplicativeExpression
    | AdditiveExpression "+" MultiplicativeExpression
    | AdditiveExpression "-" MultiplicativeExpression
    ;

ShiftExpression
    : AdditiveExpression
    | ShiftExpression "<<" AdditiveExpression
    | ShiftExpression ">>" AdditiveExpression
    | ShiftExpression ">>>" AdditiveExpression
    ;

RelationalExpression
    : ShiftExpression
    | RelationalExpression "<" ShiftExpression
    | RelationalExpression ">" ShiftExpression
    | RelationalExpression "<=" ShiftExpression
    | RelationalExpression ">=" ShiftExpression
    | RelationalExpression "instanceof" ShiftExpression
    | RelationalExpression "in" ShiftExpression
    ;

EqualityExpression
    : RelationalExpression
    | EqualityExpression "==" RelationalExpression
    | EqualityExpression "!=" RelationalExpression
    | EqualityExpression "===" RelationalExpression
    | EqualityExpression "!==" RelationalExpression
    ;

BitwiseANDExpression
    : EqualityExpression
    | BitwiseANDExpression "&" EqualityExpression
    ;

BitwiseXORExpression
    : BitwiseANDExpression
    | BitwiseXORExpression "^" BitwiseANDExpression
    ;

BitwiseORExpression
    : BitwiseXORExpression
    | BitwiseORExpression "|" BitwiseXORExpression
    ;

LogicalANDExpression
    : BitwiseORExpression
    | LogicalANDExpression "&&" BitwiseORExpression
    ;

LogicalORExpression
    : LogicalANDExpression
    | LogicalORExpression "||" LogicalANDExpression
    ;

ConditionalExpression
    : LogicalORExpression
    | LogicalORExpression "?" AssignmentExpression ":" AssignmentExpression
    ;

AssignmentExpression
    : ConditionalExpression
    | LeftHandSideExpression AssignmentOperator AssignmentExpression
    ;

AssignmentOperator
    : "="
    | "*="
    | "/="
    | "%="
    | "+="
    | "-="
    | "<<="
    | ">>="
    | ">>>="
    | "&="
    | "^="
    | "|="
    ;

Expression_
    : "," Expression
    |
    ;

ExpressionOpt
    : Expression
    |
    ;

Expression
    : AssignmentExpression Expression_
    ;

Statement
    : Block
    | VariableStatement
    | EmptyStatement
    | ExpressionStatement
    | IfStatement
    | IterationStatement
    | ContinueStatement
    | BreakStatement
    | ReturnStatement
    | WithStatement
    | LabelledStatement
    | SwitchStatement
    | ThrowStatement
    | TryStatement
    ;

Block
    : "{" StatementListOpt "}"
    ;

StatementListOpt
    : Statement StatementListOpt
    |
    ;

VariableStatement
    : "var" VariableDeclarationList ";"
    ;

VariableDeclarationList_
    : "," VariableDeclarationList
    |
    ;

VariableDeclarationList
    : VariableDeclaration VariableDeclarationList_
    ;

VariableDeclaration
    : Identifier InitialiserOpt
    ;

InitialiserOpt
    : Initialiser
    |
    ;

Initialiser
    : "=" AssignmentExpression
    ;

EmptyStatement
    : ";"
    ;

ExpressionStatement
    : Expression ";"
    ;

ElseStatementOpt
    : "else" Statement
    |
    ;

IfStatement
    : "if" "(" Expression ")" Statement ElseStatementOpt
    ;

IterationStatement
    : DoWhileStatement
    | WhileStatement
    | ForStatement
    | ForInStatement
    ;

DoWhileStatement
    : "do" Statement "while" "(" Expression ")" ";"
    ;

WhileStatement
    : "while" "(" Expression ")" Statement
    ;

ForStatement
    : "for" "(" ForStatementInitialiser ";" ExpressionOpt ";" ExpressionOpt ")" Statement
    ;

ForStatementInitialiser
    : ExpressionOpt
    | "var" VariableDeclarationList
    ;

ForInStatement
    : "for" "(" ForInStatementInitialiser "in" Expression ")" Statement
    ;

ForInStatementInitialiser
    : LeftHandSideExpression
    | "var" VariableDeclaration
    ;

ContinueStatement
    : "continue" IdentifierOpt ";"
    ;

BreakStatement
    : "break" IdentifierOpt ";"
    ;

ReturnStatement
    : "return" ExpressionOpt ";"
    ;

WithStatement
    : "with" "(" Expression ")" Statement
    ;

SwitchStatement
    : "switch" "(" Expression ")" CaseBlock
    ;

CaseBlock
    : "{" CaseClausesOpt "}"
    ;

CaseClausesOpt
    : CaseClause CaseClausesOpt
    | DefaultClause CaseClausesOpt
    |
    ;

CaseClause
    : "case" Expression ":" StatementListOpt
    ;

DefaultClause
    : "default" ":" StatementListOpt
    ;

LabelledStatement
    : Identifier ":" Statement
    ;

ThrowStatement
    : "throw" Expression ";"
    ;

TryStatement
    : "try" Block CatchOpt FinallyOpt
    ;

CatchOpt
    : "catch" "(" Identifier ")" Block
    |
    ;

FinallyOpt
    : "finally" Block
    |
    ;

FunctionDeclaration
    : "function" Identifier "(" FormalParameterList ")" "{" FunctionBody "}"
    ;

FunctionExpression
    : "function" IdentifierOpt "(" FormalParameterList ")" "{" FunctionBody "}"
    ;

FormalParameterList_
    : "," FormalParameterList
    |
    ;

FormalParameterList
    : Identifier FormalParameterList_
    |
    ;

FunctionBody
    : SourceElements
    ;

Program
    : SourceElements
    ;

SourceElements
    : SourceElement SourceElements
    |
    ;

SourceElement
    : Statement
    | FunctionDeclaration
    ;

%%

#include <stdio.h>

void cyerror(YYLTYPE *locp, CYParser *context, const char *msg) {
    fprintf(stderr, "err:%s\n", msg);
}
