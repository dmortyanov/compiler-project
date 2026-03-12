# Formal Grammar Specification

## 1. Overview

This document defines the Context-Free Grammar (CFG) for the MiniCompiler language using EBNF notation.

- **Start symbol:** `Program`
- **Terminal symbols:** Token types from Sprint 1 (keywords, identifiers, literals, operators, delimiters)
- **Non-terminals:** Grammar constructs listed below

## 2. Program Structure

```
Program        ::= { Declaration }
Declaration    ::= FunctionDecl | StructDecl | VarDecl
```

## 3. Declarations

```
FunctionDecl   ::= "fn" IDENTIFIER "(" [ Parameters ] ")" [ "->" Type ] Block
StructDecl     ::= "struct" IDENTIFIER "{" { VarDecl } "}"
VarDecl        ::= Type IDENTIFIER [ "=" Expression ] ";"
Parameters     ::= Parameter { "," Parameter }
Parameter      ::= Type IDENTIFIER
Type           ::= "int" | "float" | "bool" | "void"
```

## 4. Statements

```
Statement      ::= Block
                  | IfStmt
                  | WhileStmt
                  | ForStmt
                  | ReturnStmt
                  | VarDecl
                  | ExprStmt
                  | ";"

Block          ::= "{" { Statement } "}"
IfStmt         ::= "if" "(" Expression ")" Statement [ "else" Statement ]
WhileStmt      ::= "while" "(" Expression ")" Statement
ForStmt        ::= "for" "(" [ VarDecl | ExprStmt ] [ Expression ] ";" [ Expression ] ")" Statement
ReturnStmt     ::= "return" [ Expression ] ";"
ExprStmt       ::= Expression ";"
```

Note: The "dangling else" is resolved by binding `else` to the nearest `if`.

## 5. Expressions (by precedence, lowest to highest)

```
Expression     ::= Assignment

Assignment     ::= LogicalOr [ ("=" | "+=" | "-=" | "*=" | "/=") Assignment ]

LogicalOr      ::= LogicalAnd { "||" LogicalAnd }

LogicalAnd     ::= Equality { "&&" Equality }

Equality       ::= Relational { ("==" | "!=") Relational }

Relational     ::= Additive { ("<" | "<=" | ">" | ">=") Additive }

Additive       ::= Multiplicative { ("+" | "-") Multiplicative }

Multiplicative ::= Unary { ("*" | "/" | "%") Unary }

Unary          ::= ( "-" | "!" ) Unary
                  | Primary

Primary        ::= INT_LITERAL
                  | FLOAT_LITERAL
                  | STRING_LITERAL
                  | BOOL_LITERAL
                  | IDENTIFIER [ "(" [ Arguments ] ")" ]
                  | "(" Expression ")"

Arguments      ::= Expression { "," Expression }
```

## 6. Precedence & Associativity Table

| Level | Operators            | Associativity   |
|-------|----------------------|-----------------|
| 1     | `=` `+=` `-=` `*=` `/=` | Right       |
| 2     | `\|\|`               | Left            |
| 3     | `&&`                 | Left            |
| 4     | `==` `!=`            | Non-associative |
| 5     | `<` `<=` `>` `>=`    | Non-associative |
| 6     | `+` `-`              | Left            |
| 7     | `*` `/` `%`          | Left            |
| 8     | `-` (unary) `!`      | Right (prefix)  |
| 9     | Primary, Call, `()`  | —               |

## 7. Lexical References

Terminal symbols correspond to token types defined in Sprint 1:
- Keywords: `KW_IF`, `KW_ELSE`, `KW_WHILE`, `KW_FOR`, `KW_INT`, `KW_FLOAT`, `KW_BOOL`, `KW_RETURN`, `KW_VOID`, `KW_STRUCT`, `KW_FN`
- Literals: `INT_LITERAL`, `FLOAT_LITERAL`, `STRING_LITERAL`, `BOOL_LITERAL`
- Identifiers: `IDENTIFIER`
- Operators and delimiters: as enumerated in `token.h`
