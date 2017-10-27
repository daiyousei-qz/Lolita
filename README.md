# Project Lolita

## Overview
Project Lolita is an exprimental lexer and parser generator that supports parser generation with minimal regular expression and LALR(1) grammar. With a builtin AST construction engine, the syntax tree Project Lolita produce is automatically folded on grammar reduction and a visitor interface for klasses of the same category is also generated to ease the postprocess of the parsing tree.

## License
This is a personal project released in MIT license. You may feel free to read, copy, modify and distribute the code, but reliability is **ABSOLUTELY NOT GUARANTEED**. Any help and question on the project is welcomed.

## Example Usage
```
# This is a example file of Project Lolita
# that parses a list of simple calculator expressions

ignore whitespace = "[ \t\r\n]+";
token int = "[0-9]+";
token op_add = "\+";
token op_minus = "-";
token op_asterisk = "\*";
token op_slash = "/";
token lp = "\(";
token rp = "\)";
token lb = "\[";
token rb = "\]";
token comma = ",";

enum BinaryOp { Plus; Minus; Asterisk; Slash; }

base Expression;
node LiteralExpression : Expression
{
    token value;
}
node BinaryExpression : Expression
{
    Expression lhs;
    BinaryOp op;
    Expression rhs;
}

rule AddOp : BinaryOp
    = "\+" -> Plus
    = "-" -> Minus
    ;
rule MulOp : BinaryOp
    = "\*" -> Asterisk
    = "/" -> Slash
    ;

rule Factor : Expression
    = int:value -> LiteralExpression
    = lp Expr! rp
    ;
rule MulExpr : BinaryExpression
    = MulExpr:lhs MulOp:op Factor:rhs -> _
    = Factor!
    ;
rule AddExpr : BinaryExpression
    = AddExpr:lhs AddOp:op MulExpr:rhs -> _
    = MulExpr!
    ;
rule Expr : Expression
    = AddExpr!
    ;
    
rule ExprList : Expression'vec
	= Expr& -> _
	= ExprList! comma Expr&
	;
rule InputExpr : Expression'vec
	= lb ExprList! rb
	;
```
## Configuration File



## TODO

- [ ] GLR support for some ambiguous grammar
- [ ] Error detection for configuration file
- [ ] Token post-processing injection
- [ ] Parsing table dump