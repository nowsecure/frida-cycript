%code top {
#include "Cycript.tab.hh"
int cylex(YYSTYPE *lvalp, YYLTYPE *llocp, void *scanner);
#define scanner driver.scanner_
}

%code requires {
#include "Parser.hpp"
}

%union {
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

%%

%start Program;

WordOpt
    : Word { $$ = $1; }
    | { $$ = NULL; }
    ;

Word
    : Identifier { $$ = $1; }
    | "break" { $$ = $1; }
    | "case" { $$ = $1; }
    | "catch" { $$ = $1; }
    | "continue" { $$ = $1; }
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
    | "return" { $$ = $1; }
    | "switch" { $$ = $1; }
    | "this" { $$ = $1; }
    | "throw" { $$ = $1; }
    | "true" { $$ = $1; }
    | "try" { $$ = $1; }
    | "typeof" { $$ = $1; }
    | "var" { $$ = $1; }
    | "void" { $$ = $1; }
    | "while" { $$ = $1; }
    | "with" { $$ = $1; }
    ;

IdentifierOpt
    : Identifier { $$ = $1; }
    | { $$ = NULL; }
    ;

Literal
    : NullLiteral { $$ = $1; }
    | BooleanLiteral { $$ = $1; }
    | NumericLiteral { $$ = $1; }
    | StringLiteral { $$ = $1; }
    ;

NullLiteral
    : "null" { $$ = $1; }
    ;

BooleanLiteral
    : "true" { $$ = $1; }
    | "false" { $$ = $1; }
    ;

/* Objective-C Extensions {{{ */
VariadicCall
    : "," AssignmentExpression VariadicCall { $$ = new CYArgument(NULL, $2, $3); }
    | { $$ = NULL; }
    ;

SelectorCall_
    : SelectorCall { $$ = $1; }
    | VariadicCall { $$ = $1; }
    ;

SelectorCall
    : WordOpt ":" AssignmentExpression SelectorCall_ { $$ = new CYArgument($1 ?: new CYBlank(), $3, $4); }
    ;

SelectorList
    : SelectorCall { $$ = $1; }
    | Word { $$ = new CYArgument($1, NULL); }
    ;

MessageExpression
    : "[" AssignmentExpression SelectorList "]" { $$ = new CYMessage($2, $3); }
    ;
/* }}} */

/* 11.1 Primary Expressions {{{ */
PrimaryExpression
    : "this" { $$ = $1; }
    | Identifier { $$ = new CYVariable($1); }
    | Literal { $$ = $1; }
    | ArrayLiteral { $$ = $1; }
    | ObjectLiteral { $$ = $1; }
    | "(" Expression ")" { $$ = $2; }
    | MessageExpression { $$ = $1; }
    ;
/* }}} */
/* 11.1.4 Array Initialiser {{{ */
ArrayLiteral
    : "[" ElementList "]" { $$ = $2; }
    ;

Element
    : AssignmentExpression { $$ = $1; }
    | { $$ = NULL; }
    ;

ElementList_
    : "," ElementList { $$ = $2; }
    | { $$ = NULL; }
    ;

ElementList
    : Element ElementList_ { $$ = new CYElement($1, $2); }
    ;
/* }}} */
/* 11.1.5 Object Initialiser {{{ */
ObjectLiteral
    : "{" PropertyNameAndValueListOpt "}" { $$ = $2; }
    ;

PropertyNameAndValueList_
    : "," PropertyNameAndValueList { $$ = $2; }
    | { $$ = NULL; }
    ;

PropertyNameAndValueListOpt
    : PropertyNameAndValueList { $$ = $1; }
    | { $$ = NULL; }
    ;

PropertyNameAndValueList
    : PropertyName ":" AssignmentExpression PropertyNameAndValueList_ { $$ = new CYProperty($1, $3, $4); }
    ;

PropertyName
    : Identifier { $$ = $1; }
    | StringLiteral { $$ = $1; }
    | NumericLiteral { $$ = $1; }
    ;
/* }}} */

MemberExpression
    : PrimaryExpression { $$ = $1; }
    | FunctionExpression { $$ = $1; }
    | MemberExpression "[" Expression "]" { $$ = new CYMember($1, $3); }
    | MemberExpression "." Identifier { $$ = new CYMember($1, new CYString($3)); }
    | "new" MemberExpression Arguments { $$ = new CYNew($2, $3); }
    ;

NewExpression
    : MemberExpression { $$ = $1; }
    | "new" NewExpression { $$ = new CYNew($2, NULL); }
    ;

CallExpression
    : MemberExpression Arguments { $$ = new CYCall($1, $2); }
    | CallExpression Arguments { $$ = new CYCall($1, $2); }
    | CallExpression "[" Expression "]" { $$ = new CYMember($1, $3); }
    | CallExpression "." Identifier { $$ = new CYMember($1, new CYString($3)); }
    ;

ArgumentList_
    : "," ArgumentList { $$ = $2; }
    | { $$ = NULL; }
    ;

ArgumentListOpt
    : ArgumentList { $$ = $1; }
    | { $$ = NULL; }
    ;

ArgumentList
    : AssignmentExpression ArgumentList_ { $$ = new CYArgument(NULL, $1, $2); }
    ;

Arguments
    : "(" ArgumentListOpt ")" { $$ = $2; }
    ;

LeftHandSideExpression
    : NewExpression { $$ = $1; }
    | CallExpression { $$ = $1; }
    | "*" LeftHandSideExpression { $$ = new CYIndirect($2); }
    ;

PostfixExpression
    : LeftHandSideExpression { $$ = $1; }
    | LeftHandSideExpression "++" { $$ = new CYPostIncrement($1); }
    | LeftHandSideExpression "--" { $$ = new CYPostDecrement($1); }
    ;

UnaryExpression
    : PostfixExpression { $$ = $1; }
    | "delete" UnaryExpression { $$ = new CYDelete($2); }
    | "void" UnaryExpression { $$ = new CYVoid($2); }
    | "typeof" UnaryExpression { $$ = new CYTypeOf($2); }
    | "++" UnaryExpression { $$ = new CYPreIncrement($2); }
    | "--" UnaryExpression { $$ = new CYPreDecrement($2); }
    | "+" UnaryExpression { $$ = $2; }
    | "-" UnaryExpression { $$ = new CYNegate($2); }
    | "~" UnaryExpression { $$ = new CYBitwiseNot($2); }
    | "!" UnaryExpression { $$ = new CYLogicalNot($2); }
    | "&" UnaryExpression { $$ = new CYAddressOf($2); }
    ;

MultiplicativeExpression
    : UnaryExpression { $$ = $1; }
    | MultiplicativeExpression "*" UnaryExpression { $$ = new CYMultiply($1, $3); }
    | MultiplicativeExpression "/" UnaryExpression { $$ = new CYDivide($1, $3); }
    | MultiplicativeExpression "%" UnaryExpression { $$ = new CYModulus($1, $3); }
    ;

AdditiveExpression
    : MultiplicativeExpression { $$ = $1; }
    | AdditiveExpression "+" MultiplicativeExpression { $$ = new CYAdd($1, $3); }
    | AdditiveExpression "-" MultiplicativeExpression { $$ = new CYSubtract($1, $3); }
    ;

ShiftExpression
    : AdditiveExpression { $$ = $1; }
    | ShiftExpression "<<" AdditiveExpression { $$ = new CYShiftLeft($1, $3); }
    | ShiftExpression ">>" AdditiveExpression { $$ = new CYShiftRightSigned($1, $3); }
    | ShiftExpression ">>>" AdditiveExpression { $$ = new CYShiftRightUnsigned($1, $3); }
    ;

RelationalExpression
    : ShiftExpression { $$ = $1; }
    | RelationalExpression "<" ShiftExpression { $$ = new CYLess($1, $3); }
    | RelationalExpression ">" ShiftExpression { $$ = new CYGreater($1, $3); }
    | RelationalExpression "<=" ShiftExpression { $$ = new CYLessOrEqual($1, $3); }
    | RelationalExpression ">=" ShiftExpression { $$ = new CYGreaterOrEqual($1, $3); }
    | RelationalExpression "instanceof" ShiftExpression { $$ = new CYInstanceOf($1, $3); }
    | RelationalExpression "in" ShiftExpression { $$ = new CYIn($1, $3); }
    ;

EqualityExpression
    : RelationalExpression { $$ = $1; }
    | EqualityExpression "==" RelationalExpression { $$ = new CYEqual($1, $3); }
    | EqualityExpression "!=" RelationalExpression { $$ = new CYNotEqual($1, $3); }
    | EqualityExpression "===" RelationalExpression { $$ = new CYIdentical($1, $3); }
    | EqualityExpression "!==" RelationalExpression { $$ = new CYNotIdentical($1, $3); }
    ;

BitwiseANDExpression
    : EqualityExpression { $$ = $1; }
    | BitwiseANDExpression "&" EqualityExpression { $$ = new CYBitwiseAnd($1, $3); }
    ;

BitwiseXORExpression
    : BitwiseANDExpression { $$ = $1; }
    | BitwiseXORExpression "^" BitwiseANDExpression { $$ = new CYBitwiseXOr($1, $3); }
    ;

BitwiseORExpression
    : BitwiseXORExpression { $$ = $1; }
    | BitwiseORExpression "|" BitwiseXORExpression { $$ = new CYBitwiseOr($1, $3); }
    ;

LogicalANDExpression
    : BitwiseORExpression { $$ = $1; }
    | LogicalANDExpression "&&" BitwiseORExpression { $$ = new CYLogicalAnd($1, $3); }
    ;

LogicalORExpression
    : LogicalANDExpression { $$ = $1; }
    | LogicalORExpression "||" LogicalANDExpression { $$ = new CYLogicalOr($1, $3); }
    ;

ConditionalExpression
    : LogicalORExpression { $$ = $1; }
    | LogicalORExpression "?" AssignmentExpression ":" AssignmentExpression { $$ = new CYCondition($1, $3, $5); }
    ;

AssignmentExpression
    : ConditionalExpression { $$ = $1; }
    | LeftHandSideExpression "=" AssignmentExpression { $$ = new CYAssign($1, $3); }
    | LeftHandSideExpression "*=" AssignmentExpression { $$ = new CYMultiplyAssign($1, $3); }
    | LeftHandSideExpression "/=" AssignmentExpression { $$ = new CYDivideAssign($1, $3); }
    | LeftHandSideExpression "%=" AssignmentExpression { $$ = new CYModulusAssign($1, $3); }
    | LeftHandSideExpression "+=" AssignmentExpression { $$ = new CYAddAssign($1, $3); }
    | LeftHandSideExpression "-=" AssignmentExpression { $$ = new CYSubtractAssign($1, $3); }
    | LeftHandSideExpression "<<=" AssignmentExpression { $$ = new CYShiftLeftAssign($1, $3); }
    | LeftHandSideExpression ">>=" AssignmentExpression { $$ = new CYShiftRightSignedAssign($1, $3); }
    | LeftHandSideExpression ">>>=" AssignmentExpression { $$ = new CYShiftRightUnsignedAssign($1, $3); }
    | LeftHandSideExpression "&=" AssignmentExpression { $$ = new CYBitwiseAndAssign($1, $3); }
    | LeftHandSideExpression "^=" AssignmentExpression { $$ = new CYBitwiseXOrAssign($1, $3); }
    | LeftHandSideExpression "|=" AssignmentExpression { $$ = new CYBitwiseOrAssign($1, $3); }
    ;

Expression_
    : "," Expression { $$ = $2; }
    | { $$ = NULL; }
    ;

ExpressionOpt
    : Expression { $$ = $1; }
    | { $$ = NULL; }
    ;

Expression
    : AssignmentExpression Expression_ { if ($1 == NULL) $$ = $2; else { $1->SetNext($2); $$ = $1; } }
    ;

Statement
    : Block { $$ = $1; }
    | VariableStatement { $$ = $1; }
    | EmptyStatement { $$ = $1; }
    | ExpressionStatement { $$ = $1; }
    | IfStatement { $$ = $1; }
    | IterationStatement { $$ = $1; }
    | ContinueStatement { $$ = $1; }
    | BreakStatement { $$ = $1; }
    | ReturnStatement { $$ = $1; }
    | WithStatement { $$ = $1; }
    | LabelledStatement { $$ = $1; }
    | SwitchStatement { $$ = $1; }
    | ThrowStatement { $$ = $1; }
    | TryStatement { $$ = $1; }
    ;

Block
    : "{" StatementListOpt "}" { $$ = $2 ?: new CYEmpty(); }
    ;

StatementListOpt
    : Statement StatementListOpt { $1->SetNext($2); $$ = $1; }
    | { $$ = NULL; }
    ;

VariableStatement
    : "var" VariableDeclarationList ";" { $$ = $2; }
    ;

VariableDeclarationList_
    : "," VariableDeclarationList { $$ = $2; }
    | { $$ = NULL; }
    ;

VariableDeclarationList
    : VariableDeclaration VariableDeclarationList_ { $$ = new CYDeclarations($1, $2); }
    ;

VariableDeclaration
    : Identifier InitialiserOpt { $$ = new CYDeclaration($1, $2); }
    ;

InitialiserOpt
    : Initialiser { $$ = $1; }
    | { $$ = NULL; }
    ;

Initialiser
    : "=" AssignmentExpression { $$ = $2; }
    ;

EmptyStatement
    : ";" { $$ = new CYEmpty(); }
    ;

ExpressionStatement
    : Expression ";" { $$ = new CYExpress($1); }
    ;

ElseStatementOpt
    : "else" Statement { $$ = $2; }
    | { $$ = NULL; }
    ;

IfStatement
    : "if" "(" Expression ")" Statement ElseStatementOpt { $$ = new CYIf($3, $5, $6); }
    ;

IterationStatement
    : DoWhileStatement { $$ = $1; }
    | WhileStatement { $$ = $1; }
    | ForStatement { $$ = $1; }
    | ForInStatement { $$ = $1; }
    ;

DoWhileStatement
    : "do" Statement "while" "(" Expression ")" ";" { $$ = new CYDoWhile($5, $2); }
    ;

WhileStatement
    : "while" "(" Expression ")" Statement { $$ = new CYWhile($3, $5); }
    ;

ForStatement
    : "for" "(" ForStatementInitialiser ";" ExpressionOpt ";" ExpressionOpt ")" Statement { $$ = new CYFor($3, $5, $7, $9); }
    ;

ForStatementInitialiser
    : ExpressionOpt { $$ = $1; }
    | "var" VariableDeclarationList { $$ = $2; }
    ;

ForInStatement
    : "for" "(" ForInStatementInitialiser "in" Expression ")" Statement { $$ = new CYForIn($3, $5, $7); }
    ;

ForInStatementInitialiser
    : LeftHandSideExpression { $$ = $1; }
    | "var" VariableDeclaration { $$ = $2; }
    ;

ContinueStatement
    : "continue" IdentifierOpt ";" { $$ = new CYContinue($2); }
    ;

BreakStatement
    : "break" IdentifierOpt ";" { $$ = new CYBreak($2); }
    ;

ReturnStatement
    : "return" ExpressionOpt ";" { $$ = new CYReturn($2); }
    ;

WithStatement
    : "with" "(" Expression ")" Statement { $$ = new CYWith($3, $5); }
    ;

SwitchStatement
    : "switch" "(" Expression ")" CaseBlock { $$ = new CYSwitch($3, $5); }
    ;

CaseBlock
    : "{" CaseClausesOpt "}" { $$ = $2; }
    ;

CaseClausesOpt
    : CaseClause CaseClausesOpt { $1->SetNext($2); $$ = $1; }
    | DefaultClause CaseClausesOpt { $1->SetNext($2); $$ = $1; }
    | { $$ = NULL; }
    ;

CaseClause
    : "case" Expression ":" StatementListOpt { $$ = new CYClause($2, $4); }
    ;

DefaultClause
    : "default" ":" StatementListOpt { $$ = new CYClause(NULL, $3); }
    ;

LabelledStatement
    : Identifier ":" Statement { $3->AddLabel($1); $$ = $3; }
    ;

ThrowStatement
    : "throw" Expression ";" { $$ = new CYThrow($2); }
    ;

TryStatement
    : "try" Block CatchOpt FinallyOpt { $$ = new CYTry($2, $3, $4); }
    ;

CatchOpt
    : "catch" "(" Identifier ")" Block { $$ = new CYCatch($3, $5); }
    | { $$ = NULL; }
    ;

FinallyOpt
    : "finally" Block { $$ = $2; }
    | { $$ = NULL; }
    ;

FunctionDeclaration
    : "function" Identifier "(" FormalParameterList ")" "{" FunctionBody "}" { $$ = new CYFunction($2, $4, $7); }
    ;

FunctionExpression
    : "function" IdentifierOpt "(" FormalParameterList ")" "{" FunctionBody "}" { $$ = new CYLambda($2, $4, $7); }
    ;

FormalParameterList_
    : "," FormalParameterList { $$ = $2; }
    | { $$ = NULL; }
    ;

FormalParameterList
    : Identifier FormalParameterList_ { $$ = new CYParameter($1, $2); }
    | { $$ = NULL; }
    ;

FunctionBody
    : SourceElements { $$ = $1; }
    ;

Program
    : SourceElements { driver.source_ = $1; $$ = $1; }
    ;

SourceElements
    : SourceElement SourceElements { $1->SetNext($2); $$ = $1; }
    | { $$ = NULL; }
    ;

SourceElement
    : Statement { $$ = $1; }
    | FunctionDeclaration { $$ = $1; }
    ;

%%
