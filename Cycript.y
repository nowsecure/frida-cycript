%code top {
#include "Cycript.tab.hh"
int cylex(YYSTYPE *lvalp, YYLTYPE *llocp, void *scanner);
#define scanner driver.scanner_
}

%code requires {
#include "Parser.hpp"

typedef struct {
    bool newline_;

    union {
        CYArgument *argument_;
        CYBoolean *boolean_;
        CYClause *clause_;
        CYCatch *catch_;
        CYDeclaration *declaration_;
        CYDeclarations *declarations_;
        CYElement *element_;
        CYExpression *expression_;
        CYFalse *false_;
        CYForInitialiser *for_;
        CYForInInitialiser *forin_;
        CYIdentifier *identifier_;
        CYLiteral *literal_;
        CYName *name_;
        CYNull *null_;
        CYNumber *number_;
        CYParameter *parameter_;
        CYProperty *property_;
        CYSource *source_;
        CYStatement *statement_;
        CYString *string_;
        CYThis *this_;
        CYTrue *true_;
        CYWord *word_;
    };
} YYSTYPE;

}

%name-prefix "cy"

%language "C++"
%locations
%glr-parser

%initial-action {
    @$.begin.filename = @$.end.filename = &driver.filename_;
};

%defines

%debug
%error-verbose

%parse-param { CYDriver &driver }
%lex-param { void *scanner }

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
%token HyphenHyphen_ "\n--"
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
%token PlusPlus_ "\n++"
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
%token NewLine "\n"

%token OpenParen "("
%token CloseParen ")"

%token OpenBrace "{"
%token CloseBrace "}"

%token OpenBracket "["
%token CloseBracket "]"

%token <word_> Break "break"
%token <word_> Case "case"
%token <word_> Catch "catch"
%token <word_> Continue "continue"
%token <word_> Default "default"
%token <word_> Delete "delete"
%token <word_> Do "do"
%token <word_> Else "else"
%token <false_> False "false"
%token <word_> Finally "finally"
%token <word_> For "for"
%token <word_> Function "function"
%token <word_> If "if"
%token <word_> In "in"
%token <word_> InstanceOf "instanceof"
%token <word_> New "new"
%token <null_> Null "null"
%token <word_> Return "return"
%token <word_> Switch "switch"
%token <this_> This "this"
%token <word_> Throw "throw"
%token <true_> True "true"
%token <word_> Try "try"
%token <word_> TypeOf "typeof"
%token <word_> Var "var"
%token <word_> Void "void"
%token <word_> While "while"
%token <word_> With "with"

%token <identifier_> Identifier
%token <number_> NumericLiteral
%token <string_> StringLiteral

%type <expression_> AdditiveExpression
%type <argument_> ArgumentList
%type <argument_> ArgumentList_
%type <argument_> ArgumentListOpt
%type <argument_> Arguments
%type <literal_> ArrayLiteral
%type <expression_> AssignmentExpression
%type <expression_> BitwiseANDExpression
%type <statement_> Block
%type <boolean_> BooleanLiteral
%type <expression_> BitwiseORExpression
%type <expression_> BitwiseXORExpression
%type <statement_> BreakStatement
%type <expression_> CallExpression
%type <clause_> CaseBlock
%type <clause_> CaseClause
%type <clause_> CaseClausesOpt
%type <catch_> CatchOpt
%type <expression_> ConditionalExpression
%type <statement_> ContinueStatement
%type <clause_> DefaultClause
%type <statement_> DoWhileStatement
%type <expression_> Element
%type <element_> ElementList
%type <element_> ElementList_
%type <statement_> ElseStatementOpt
%type <statement_> EmptyStatement
%type <expression_> EqualityExpression
%type <expression_> Expression
%type <expression_> Expression_
%type <expression_> ExpressionOpt
%type <statement_> ExpressionStatement
%type <statement_> FinallyOpt
%type <statement_> ForStatement
%type <for_> ForStatementInitialiser
%type <statement_> ForInStatement
%type <forin_> ForInStatementInitialiser
%type <parameter_> FormalParameterList
%type <parameter_> FormalParameterList_
%type <source_> FunctionBody
%type <source_> FunctionDeclaration
%type <expression_> FunctionExpression
%type <identifier_> IdentifierOpt
%type <statement_> IfStatement
%type <expression_> Initialiser
%type <expression_> InitialiserOpt
%type <statement_> IterationStatement
%type <statement_> LabelledStatement
%type <expression_> LeftHandSideExpression
%type <literal_> Literal
%type <expression_> LogicalANDExpression
%type <expression_> LogicalORExpression
%type <expression_> MemberExpression
%type <expression_> MultiplicativeExpression
%type <expression_> NewExpression
%type <null_> NullLiteral
%type <literal_> ObjectLiteral
%type <expression_> MessageExpression
%type <expression_> PostfixExpression
%type <expression_> PrimaryExpression
%type <source_> Program
%type <name_> PropertyName
%type <property_> PropertyNameAndValueList
%type <property_> PropertyNameAndValueList_
%type <property_> PropertyNameAndValueListOpt
%type <expression_> QExpressionOpt
%type <identifier_> QIdentifierOpt
%type <expression_> RelationalExpression
%type <statement_> ReturnStatement
%type <argument_> SelectorCall
%type <argument_> SelectorCall_
%type <argument_> SelectorList
%type <expression_> ShiftExpression
%type <source_> SourceElement
%type <source_> SourceElements
%type <statement_> Statement
%type <statement_> StatementListOpt
%type <statement_> SwitchStatement
%type <statement_> ThrowStatement
%type <statement_> TryStatement
%type <expression_> UnaryExpression
%type <declaration_> VariableDeclaration
%type <declarations_> VariableDeclarationList
%type <declarations_> VariableDeclarationList_
%type <statement_> VariableStatement
%type <argument_> VariadicCall
%type <statement_> WhileStatement
%type <statement_> WithStatement
%type <word_> Word
%type <word_> WordOpt

%nonassoc "if"
%nonassoc "else"

%%

%start Program;

Q
    :
    /*| NewLine*/
    ;

QTerminator
    : Q Terminator
    ;

/*TerminatorOpt
    : QTerminator
    |
    ;

Terminator
    : ";"
    | NewLine
    ;*/

TerminatorOpt
    : ";"
    | NewLine
    |
    ;

Terminator
    : ";"
    | NewLine
    | error { yyerrok; }
    ;

NewLineOpt
    : NewLine
    |
    ;

WordOpt /*Qq*/
    : Word { $$ = $1; }
    | { $$ = NULL; }
    ;

Word /*Q*/
    : Identifier { $$ = $1; }
    | "break" NewLineOpt { $$ = $1; }
    | "case" { $$ = $1; }
    | "catch" { $$ = $1; }
    | "continue" NewLineOpt { $$ = $1; }
    | "default" { $$ = $1; }
    | "delete" { $$ = $1; }
    | "do" { $$ = $1; }
    | "else" { $$ = $1; }
    | "false" { $$ = $1; }
    | "finally" { $$ = $1; }
    | "for" { $$ = $1; }
    | "function" { $$ = $1; }
    | "if" { $$ = $1; }
    | "in" { $$ = $1; }
    | "instanceof" { $$ = $1; }
    | "new" { $$ = $1; }
    | "null" { $$ = $1; }
    | "return" NewLineOpt { $$ = $1; }
    | "switch" { $$ = $1; }
    | "this" { $$ = $1; }
    | "throw" NewLineOpt { $$ = $1; }
    | "true" { $$ = $1; }
    | "try" { $$ = $1; }
    | "typeof" { $$ = $1; }
    | "var" { $$ = $1; }
    | "void" { $$ = $1; }
    | "while" { $$ = $1; }
    | "with" { $$ = $1; }
    ;

IdentifierOpt /*Q*/
    : Identifier { $$ = $1; }
    | { $$ = NULL; }
    ;

QIdentifierOpt
    : Q Identifier { $$ = $2; }
    | { $$ = NULL; }
    ;

Literal /*Q*/
    : NullLiteral { $$ = $1; }
    | BooleanLiteral { $$ = $1; }
    | NumericLiteral { $$ = $1; }
    | StringLiteral { $$ = $1; }
    ;

NullLiteral /*Q*/
    : "null" { $$ = $1; }
    ;

BooleanLiteral /*Q*/
    : "true" { $$ = $1; }
    | "false" { $$ = $1; }
    ;

/* Objective-C Extensions {{{ */
VariadicCall /*Qq*/
    : "," Q AssignmentExpression VariadicCall { $$ = new(driver.pool_) CYArgument(NULL, $3, $4); }
    | { $$ = NULL; }
    ;

SelectorCall_ /*Qq*/
    : SelectorCall { $$ = $1; }
    | VariadicCall { $$ = $1; }
    ;

SelectorCall /*Qq*/
    : WordOpt ":" Q AssignmentExpression SelectorCall_ { $$ = new(driver.pool_) CYArgument($1 ?: new(driver.pool_) CYBlank(), $4, $5); }
    ;

SelectorList /*Qq*/
    : SelectorCall { $$ = $1; }
    | Word Q { $$ = new(driver.pool_) CYArgument($1, NULL); }
    ;

MessageExpression /*Q*/
    : "[" Q AssignmentExpression SelectorList "]" { $$ = new(driver.pool_) CYMessage($3, $4); }
    ;
/* }}} */

/* 11.1 Primary Expressions {{{ */
PrimaryExpression /*Q*/
    : "this" { $$ = $1; }
    | Identifier { $$ = new(driver.pool_) CYVariable($1); }
    | Literal { $$ = $1; }
    | ArrayLiteral { $$ = $1; }
    | ObjectLiteral { $$ = $1; }
    | "(" Q Expression ")" { $$ = $3; }
    | MessageExpression { $$ = $1; }
    ;
/* }}} */
/* 11.1.4 Array Initialiser {{{ */
ArrayLiteral /*Q*/
    : "[" Q ElementList "]" { $$ = $3; }
    ;

Element /*Qq*/
    : AssignmentExpression { $$ = $1; }
    | { $$ = NULL; }
    ;

ElementList_ /*Qq*/
    : "," Q ElementList { $$ = $3; }
    | { $$ = NULL; }
    ;

ElementList /*Qq*/
    : Element ElementList_ { $$ = new(driver.pool_) CYElement($1, $2); }
    ;
/* }}} */
/* 11.1.5 Object Initialiser {{{ */
ObjectLiteral /*Q*/
    : "{" PropertyNameAndValueListOpt "}" { $$ = $2; }
    ;

PropertyNameAndValueList_ /*Qq*/
    : "," PropertyNameAndValueList { $$ = $2; }
    | { $$ = NULL; }
    ;

PropertyNameAndValueListOpt /*q*/
    : PropertyNameAndValueList { $$ = $1; }
    | Q { $$ = NULL; }
    ;

PropertyNameAndValueList /*q*/
    : PropertyName Q ":" Q AssignmentExpression PropertyNameAndValueList_ { $$ = new(driver.pool_) CYProperty($1, $5, $6); }
    ;

PropertyName
    : Q Identifier { $$ = $2; }
    | Q StringLiteral { $$ = $2; }
    | Q NumericLiteral { $$ = $2; }
    ;
/* }}} */

MemberExpression /*Q*/
    : PrimaryExpression { $$ = $1; }
    | FunctionExpression { $$ = $1; }
    | MemberExpression Q "[" Q Expression "]" { $$ = new(driver.pool_) CYMember($1, $5); }
    | MemberExpression Q "." Q Identifier { $$ = new(driver.pool_) CYMember($1, new(driver.pool_) CYString($5)); }
    | "new" Q MemberExpression Arguments { $$ = new(driver.pool_) CYNew($3, $4); }
    ;

NewExpression /*Q*/
    : MemberExpression { $$ = $1; }
    | "new" Q NewExpression { $$ = new(driver.pool_) CYNew($3, NULL); }
    ;

CallExpression /*Q*/
    : MemberExpression Arguments { $$ = new(driver.pool_) CYCall($1, $2); }
    | CallExpression Arguments { $$ = new(driver.pool_) CYCall($1, $2); }
    | CallExpression Q "[" Q Expression "]" { $$ = new(driver.pool_) CYMember($1, $5); }
    | CallExpression Q "." Q Identifier { $$ = new(driver.pool_) CYMember($1, new(driver.pool_) CYString($5)); }
    ;

ArgumentList_ /*Qq*/
    : "," ArgumentList { $$ = $2; }
    | { $$ = NULL; }
    ;

ArgumentListOpt /*q*/
    : ArgumentList { $$ = $1; }
    | Q { $$ = NULL; }
    ;

ArgumentList /*q*/
    : Q AssignmentExpression ArgumentList_ { $$ = new(driver.pool_) CYArgument(NULL, $2, $3); }
    ;

Arguments
    : Q "(" ArgumentListOpt ")" { $$ = $3; }
    ;

LeftHandSideExpression /*Q*/
    : NewExpression { $$ = $1; }
    | CallExpression { $$ = $1; }
    | "*" Q LeftHandSideExpression { $$ = new(driver.pool_) CYIndirect($3); }
    ;

PostfixExpression /*Q*/
    : LeftHandSideExpression { $$ = $1; }
    | LeftHandSideExpression "++" { $$ = new(driver.pool_) CYPostIncrement($1); }
    | LeftHandSideExpression "--" { $$ = new(driver.pool_) CYPostDecrement($1); }
    ;

UnaryExpression /*Qq*/
    : PostfixExpression Q { $$ = $1; }
    | "delete" Q UnaryExpression { $$ = new(driver.pool_) CYDelete($3); }
    | "void" Q UnaryExpression { $$ = new(driver.pool_) CYVoid($3); }
    | "typeof" Q UnaryExpression { $$ = new(driver.pool_) CYTypeOf($3); }
    | "++" Q UnaryExpression { $$ = new(driver.pool_) CYPreIncrement($3); }
    | "\n++" Q UnaryExpression { $$ = new(driver.pool_) CYPreIncrement($3); }
    | "--" Q UnaryExpression { $$ = new(driver.pool_) CYPreDecrement($3); }
    | "\n--" Q UnaryExpression { $$ = new(driver.pool_) CYPreDecrement($3); }
    | "+" Q UnaryExpression { $$ = $3; }
    | "-" Q UnaryExpression { $$ = new(driver.pool_) CYNegate($3); }
    | "~" Q UnaryExpression { $$ = new(driver.pool_) CYBitwiseNot($3); }
    | "!" Q UnaryExpression { $$ = new(driver.pool_) CYLogicalNot($3); }
    | "&" Q UnaryExpression { $$ = new(driver.pool_) CYAddressOf($3); }
    ;

MultiplicativeExpression /*Qq*/
    : UnaryExpression { $$ = $1; }
    | MultiplicativeExpression "*" Q UnaryExpression { $$ = new(driver.pool_) CYMultiply($1, $4); }
    | MultiplicativeExpression "/" Q UnaryExpression { $$ = new(driver.pool_) CYDivide($1, $4); }
    | MultiplicativeExpression "%" Q UnaryExpression { $$ = new(driver.pool_) CYModulus($1, $4); }
    ;

AdditiveExpression /*Qq*/
    : MultiplicativeExpression { $$ = $1; }
    | AdditiveExpression "+" Q MultiplicativeExpression { $$ = new(driver.pool_) CYAdd($1, $4); }
    | AdditiveExpression "-" Q MultiplicativeExpression { $$ = new(driver.pool_) CYSubtract($1, $4); }
    ;

ShiftExpression /*Qq*/
    : AdditiveExpression { $$ = $1; }
    | ShiftExpression "<<" Q AdditiveExpression { $$ = new(driver.pool_) CYShiftLeft($1, $4); }
    | ShiftExpression ">>" Q AdditiveExpression { $$ = new(driver.pool_) CYShiftRightSigned($1, $4); }
    | ShiftExpression ">>>" Q AdditiveExpression { $$ = new(driver.pool_) CYShiftRightUnsigned($1, $4); }
    ;

RelationalExpression /*Qq*/
    : ShiftExpression { $$ = $1; }
    | RelationalExpression "<" Q ShiftExpression { $$ = new(driver.pool_) CYLess($1, $4); }
    | RelationalExpression ">" Q ShiftExpression { $$ = new(driver.pool_) CYGreater($1, $4); }
    | RelationalExpression "<=" Q ShiftExpression { $$ = new(driver.pool_) CYLessOrEqual($1, $4); }
    | RelationalExpression ">=" Q ShiftExpression { $$ = new(driver.pool_) CYGreaterOrEqual($1, $4); }
    | RelationalExpression "instanceof" Q ShiftExpression { $$ = new(driver.pool_) CYInstanceOf($1, $4); }
    | RelationalExpression "in" Q ShiftExpression { $$ = new(driver.pool_) CYIn($1, $4); }
    ;

EqualityExpression /*Qq*/
    : RelationalExpression { $$ = $1; }
    | EqualityExpression "==" Q RelationalExpression { $$ = new(driver.pool_) CYEqual($1, $4); }
    | EqualityExpression "!=" Q RelationalExpression { $$ = new(driver.pool_) CYNotEqual($1, $4); }
    | EqualityExpression "===" Q RelationalExpression { $$ = new(driver.pool_) CYIdentical($1, $4); }
    | EqualityExpression "!==" Q RelationalExpression { $$ = new(driver.pool_) CYNotIdentical($1, $4); }
    ;

BitwiseANDExpression /*Qq*/
    : EqualityExpression { $$ = $1; }
    | BitwiseANDExpression "&" Q EqualityExpression { $$ = new(driver.pool_) CYBitwiseAnd($1, $4); }
    ;

BitwiseXORExpression /*Qq*/
    : BitwiseANDExpression { $$ = $1; }
    | BitwiseXORExpression "^" Q BitwiseANDExpression { $$ = new(driver.pool_) CYBitwiseXOr($1, $4); }
    ;

BitwiseORExpression /*Qq*/
    : BitwiseXORExpression { $$ = $1; }
    | BitwiseORExpression "|" Q BitwiseXORExpression { $$ = new(driver.pool_) CYBitwiseOr($1, $4); }
    ;

LogicalANDExpression /*Qq*/
    : BitwiseORExpression { $$ = $1; }
    | LogicalANDExpression "&&" Q BitwiseORExpression { $$ = new(driver.pool_) CYLogicalAnd($1, $4); }
    ;

LogicalORExpression /*Qq*/
    : LogicalANDExpression { $$ = $1; }
    | LogicalORExpression "||" Q LogicalANDExpression { $$ = new(driver.pool_) CYLogicalOr($1, $4); }
    ;

ConditionalExpression /*Qq*/
    : LogicalORExpression { $$ = $1; }
    | LogicalORExpression "?" Q AssignmentExpression ":" Q AssignmentExpression { $$ = new(driver.pool_) CYCondition($1, $4, $7); }
    ;

AssignmentExpression /*Qq*/
    : ConditionalExpression { $$ = $1; }
    | LeftHandSideExpression Q "=" Q AssignmentExpression { $$ = new(driver.pool_) CYAssign($1, $5); }
    | LeftHandSideExpression Q "*=" Q AssignmentExpression { $$ = new(driver.pool_) CYMultiplyAssign($1, $5); }
    | LeftHandSideExpression Q "/=" Q AssignmentExpression { $$ = new(driver.pool_) CYDivideAssign($1, $5); }
    | LeftHandSideExpression Q "%=" Q AssignmentExpression { $$ = new(driver.pool_) CYModulusAssign($1, $5); }
    | LeftHandSideExpression Q "+=" Q AssignmentExpression { $$ = new(driver.pool_) CYAddAssign($1, $5); }
    | LeftHandSideExpression Q "-=" Q AssignmentExpression { $$ = new(driver.pool_) CYSubtractAssign($1, $5); }
    | LeftHandSideExpression Q "<<=" Q AssignmentExpression { $$ = new(driver.pool_) CYShiftLeftAssign($1, $5); }
    | LeftHandSideExpression Q ">>=" Q AssignmentExpression { $$ = new(driver.pool_) CYShiftRightSignedAssign($1, $5); }
    | LeftHandSideExpression Q ">>>=" Q AssignmentExpression { $$ = new(driver.pool_) CYShiftRightUnsignedAssign($1, $5); }
    | LeftHandSideExpression Q "&=" Q AssignmentExpression { $$ = new(driver.pool_) CYBitwiseAndAssign($1, $5); }
    | LeftHandSideExpression Q "^=" Q AssignmentExpression { $$ = new(driver.pool_) CYBitwiseXOrAssign($1, $5); }
    | LeftHandSideExpression Q "|=" Q AssignmentExpression { $$ = new(driver.pool_) CYBitwiseOrAssign($1, $5); }
    ;

Expression_ /*Qq*/
    : "," Q Expression { $$ = $3; }
    | { $$ = NULL; }
    ;

ExpressionOpt /*Qq*/
    : Expression { $$ = $1; }
    | Q { $$ = NULL; }
    ;

QExpressionOpt /*q*/
    : Q Expression { $$ = $2; }
    | Q { $$ = NULL; }
    ;

Expression /*Qq*/
    : AssignmentExpression Expression_ { if ($1) { $1->SetNext($2); $$ = $1; } else $$ = $2; }
    ;

Statement /*Qq*/
    : Block Q { $$ = $1; }
    | VariableStatement Q { $$ = $1; }
    | EmptyStatement Q { $$ = $1; }
    | ExpressionStatement Q { $$ = $1; }
    | IfStatement { $$ = $1; }
    | IterationStatement { $$ = $1; }
    | ContinueStatement Q { $$ = $1; }
    | BreakStatement Q { $$ = $1; }
    | ReturnStatement Q { $$ = $1; }
    | WithStatement { $$ = $1; }
    | LabelledStatement { $$ = $1; }
    | SwitchStatement Q { $$ = $1; }
    | ThrowStatement Q { $$ = $1; }
    | TryStatement Q { $$ = $1; }
    ;

Block /*Q*/
    : "{" StatementListOpt "}" { $$ = $2 ?: new(driver.pool_) CYEmpty(); }
    ;

StatementListOpt /*Qq*/
    : Statement StatementListOpt { $1->SetNext($2); $$ = $1; }
    | { $$ = NULL; }
    ;

VariableStatement /*Q*/
    : "var" VariableDeclarationList Terminator { $$ = $2; }
    ;

VariableDeclarationList_ /*Qq*/
    : "," VariableDeclarationList { $$ = $2; }
    | { $$ = NULL; }
    ;

VariableDeclarationList /*q*/
    : VariableDeclaration VariableDeclarationList_ { $$ = new(driver.pool_) CYDeclarations($1, $2); }
    ;

VariableDeclaration /*q*/
    : Q Identifier InitialiserOpt { $$ = new(driver.pool_) CYDeclaration($2, $3); }
    ;

InitialiserOpt /*q*/
    : Initialiser { $$ = $1; }
    | Q { $$ = NULL; }
    ;

Initialiser /*q*/
    : Q "=" Q AssignmentExpression { $$ = $4; }
    ;

EmptyStatement /*Q*/
    : ";" { $$ = new(driver.pool_) CYEmpty(); }
    ;

ExpressionStatement /*Q*/
    : Expression Terminator { $$ = new(driver.pool_) CYExpress($1); }
    ;

ElseStatementOpt /*Qq*/
    : "else" Q Statement { $$ = $3; }
    | %prec "if" { $$ = NULL; }
    ;

IfStatement /*Qq*/
    : "if" Q "(" Q Expression ")" Q Statement ElseStatementOpt { $$ = new(driver.pool_) CYIf($5, $8, $9); }
    ;

IterationStatement /*Qq*/
    : DoWhileStatement Q { $$ = $1; }
    | WhileStatement { $$ = $1; }
    | ForStatement { $$ = $1; }
    | ForInStatement { $$ = $1; }
    ;

DoWhileStatement /*Q*/
    : "do" Q Statement "while" Q "(" Q Expression ")" TerminatorOpt { $$ = new(driver.pool_) CYDoWhile($8, $3); }
    ;

WhileStatement /*Qq*/
    : "while" Q "(" Q Expression ")" Q Statement { $$ = new(driver.pool_) CYWhile($5, $8); }
    ;

ForStatement /*Qq*/
    : "for" Q "(" Q ForStatementInitialiser ";" QExpressionOpt ";" QExpressionOpt ")" Q Statement { $$ = new(driver.pool_) CYFor($5, $7, $9, $12); }
    ;

ForStatementInitialiser /*Qq*/
    : ExpressionOpt { $$ = $1; }
    | "var" VariableDeclarationList { $$ = $2; }
    ;

ForInStatement /*Qq*/
    : "for" Q "(" Q ForInStatementInitialiser "in" Q Expression ")" Q Statement { $$ = new(driver.pool_) CYForIn($5, $8, $11); }
    ;

ForInStatementInitialiser /*Qq*/
    : LeftHandSideExpression Q { $$ = $1; }
    | "var" VariableDeclaration { $$ = $2; }
    ;

ContinueStatement /*Q*/
    : "continue" IdentifierOpt QTerminator { $$ = new(driver.pool_) CYContinue($2); }
    ;

BreakStatement /*Q*/
    : "break" IdentifierOpt QTerminator { $$ = new(driver.pool_) CYBreak($2); }
    ;

ReturnStatement /*Q*/
    : "return" ExpressionOpt Terminator { $$ = new(driver.pool_) CYReturn($2); }
    ;

WithStatement /*Qq*/
    : "with" Q "(" Q Expression ")" Q Statement { $$ = new(driver.pool_) CYWith($5, $8); }
    ;

SwitchStatement /*Qq*/
    : "switch" Q "(" Q Expression ")" CaseBlock { $$ = new(driver.pool_) CYSwitch($5, $7); }
    ;

CaseBlock
    : Q "{" Q CaseClausesOpt "}" { $$ = $4; }
    ;

CaseClausesOpt /*Qq*/
    : CaseClause CaseClausesOpt { $1->SetNext($2); $$ = $1; }
    | DefaultClause CaseClausesOpt { $1->SetNext($2); $$ = $1; }
    | { $$ = NULL; }
    ;

CaseClause /*Qq*/
    : "case" Q Expression ":" StatementListOpt { $$ = new(driver.pool_) CYClause($3, $5); }
    ;

DefaultClause /*Qq*/
    : "default" Q ":" StatementListOpt { $$ = new(driver.pool_) CYClause(NULL, $4); }
    ;

LabelledStatement /*Qq*/
    : Identifier Q ":" Q Statement { $5->AddLabel($1); $$ = $5; }
    ;

ThrowStatement /*Q*/
    : "throw" Expression Terminator { $$ = new(driver.pool_) CYThrow($2); }
    ;

TryStatement /*Q*/
    : "try" Block CatchOpt FinallyOpt { $$ = new(driver.pool_) CYTry($2, $3, $4); }
    ;

CatchOpt
    : Q "catch" Q "(" Q Identifier Q ")" Block { $$ = new(driver.pool_) CYCatch($6, $9); }
    | { $$ = NULL; }
    ;

FinallyOpt
    : Q "finally" Block { $$ = $3; }
    | { $$ = NULL; }
    ;

FunctionDeclaration /*Q*/
    : "function" Q Identifier Q "(" FormalParameterList Q ")" Q "{" FunctionBody "}" { $$ = new(driver.pool_) CYFunction($3, $6, $11); }
    ;

FunctionExpression /*Q*/
    : "function" QIdentifierOpt Q "(" FormalParameterList Q ")" Q "{" FunctionBody "}" { $$ = new(driver.pool_) CYLambda($2, $5, $10); }
    ;

FormalParameterList_
    : Q "," FormalParameterList { $$ = $3; }
    | { $$ = NULL; }
    ;

FormalParameterList
    : Q Identifier FormalParameterList_ { $$ = new(driver.pool_) CYParameter($2, $3); }
    | { $$ = NULL; }
    ;

FunctionBody /*q*/
    : Q SourceElements { $$ = $2; }
    ;

Program
    : Q SourceElements { driver.source_.push_back($2); $$ = $2; }
    ;

SourceElements /*Qq*/
    : SourceElement Q SourceElements { $1->SetNext($3); $$ = $1; }
    | { $$ = NULL; }
    ;

/*Command
    : Q SourceElement { driver.source_.push_back($2); if (driver.filename_.empty() && false) YYACCEPT; $2->Show(std::cout); }
    ;*/

SourceElement /*Qq*/
    : Statement { $$ = $1; }
    | FunctionDeclaration Q { $$ = $1; }
    ;

%%
