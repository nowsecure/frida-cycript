%code top {
#include "Cycript.tab.hh"
int cylex(YYSTYPE *lvalp, YYLTYPE *llocp);
}

%code requires {
#include "Parser.hpp"
}

%union {
    CYExpression *expression_;
    CYTokenIdentifier *identifier_;
    CYTokenNumber *number_;
    CYTokenString *string_;
}

%name-prefix "cy"

%language "C++"
%locations
%glr-parser

%defines

%debug
%error-verbose

%parse-param { CYParser *context }

%token Ampersand "&"
%token AmpersandAmpersand "&&"
%token AmpersandEqual "&="
%token Carrot "^"
%token CarrotEqual "^="
%token Equal "="
%token EqualEqual "=="
%token EqualEqualEqual "==="
%token Exclamation "!"
%token ExclamationEqual "!="
%token ExclamationEqualEqual "!=="
%token Hyphen "-"
%token HyphenEqual "-="
%token HyphenHyphen "--"
%token HyphenRight "->"
%token Left "<"
%token LeftEqual "<="
%token LeftLeft "<<"
%token LeftLeftEqual "<<="
%token Percent "%"
%token PercentEqual "%="
%token Period "."
%token Pipe "|"
%token PipeEqual "|="
%token PipePipe "||"
%token Plus "+"
%token PlusEqual "+="
%token PlusPlus "++"
%token Right ">"
%token RightEqual ">="
%token RightRight ">>"
%token RightRightEqual ">>="
%token RightRightRight ">>>"
%token RightRightRightEqual ">>>="
%token Slash "/"
%token SlashEqual "/="
%token Star "*"
%token StarEqual "*="
%token Tilde "~"

%token Colon ":"
%token Comma ","
%token Question "?"
%token SemiColon ";"

%token OpenParen "("
%token CloseParen ")"
%token OpenBrace "{"
%token CloseBrace "}"
%token OpenBracket "["
%token CloseBracket "]"

%token Break "break"
%token Case "case"
%token Catch "catch"
%token Continue "continue"
%token Default "default"
%token Delete "delete"
%token Do "do"
%token Else "else"
%token False "false"
%token Finally "finally"
%token For "for"
%token Function "function"
%token If "if"
%token In "in"
%token InstanceOf "instanceof"
%token New "new"
%token Null "null"
%token Return "return"
%token Switch "switch"
%token This "this"
%token Throw "throw"
%token True "true"
%token Try "try"
%token TypeOf "typeof"
%token Var "var"
%token Void "void"
%token While "while"
%token With "with"

%token <identifier_> Identifier
%token <number_> NumericLiteral
%token <string_> StringLiteral

%%

%start Program;

IdentifierOpt
    : Identifier
    |
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
