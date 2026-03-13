# Грамматика EBNF

**Стартовый нетерминал / Start symbol:** `<Program>`

## 1) Программа и объявления

```
<Program>      := { <Declaration> }

<Declaration>  := <FunctionDecl>
               | <StructDecl>
               | <VarDecl>

<FunctionDecl> := "fn" IDENTIFIER "(" [ <Parameters> ] ")" [ <ReturnType> ] <Block>
<ReturnType>   := "->" <Type>

<StructDecl>   := "struct" IDENTIFIER "{" { <VarDecl> } "}" [ ";" ]

<VarDecl>      := <Type> IDENTIFIER [ <VarInit> ] ";"
<VarInit>      := "=" <Expression>

<Parameters>   := <Parameter> { "," <Parameter> }
<Parameter>    := <Type> IDENTIFIER

<Type>         := "int" | "float" | "bool" | "void"
```

---

## 2) Операторы (statements)

```
<Statement>    := <Block>
               | <IfStmt>
               | <WhileStmt>
               | <ForStmt>
               | <ReturnStmt>
               | <VarDecl>
               | <ExprStmt>
               | ";"

<Block>        := "{" { <Statement> } "}"

<IfStmt>       := "if" "(" <Expression> ")" <Statement> [ <ElsePart> ]
<ElsePart>     := "else" <Statement>

<WhileStmt>    := "while" "(" <Expression> ")" <Statement>

<ForStmt>      := "for" "(" <ForInit> [ <Expression> ] ";" [ <Expression> ] ")" <Statement>
<ForInit>      := ";"
               | <VarDecl>
               | <Expression> ";"

<ReturnStmt>   := "return" [ <Expression> ] ";"
<ExprStmt>     := <Expression> ";"
```

## 3) Выражения (приоритеты, LL(1))

```
<Expression>        := <Assignment>

<Assignment>        := <LogicalOr> [ <AssignTail> ]
<AssignTail>        := <AssignOp> <Assignment>
<AssignOp>          := "=" | "+=" | "-=" | "*=" | "/="

<LogicalOr>         := <LogicalAnd> { "||" <LogicalAnd> }
<LogicalAnd>        := <Equality>   { "&&" <Equality> }

<Equality>          := <Relational> [ <EqTail> ]
<EqTail>            := ( "==" | "!=" ) <Relational>

<Relational>        := <Additive> [ <RelTail> ]
<RelTail>           := ( "<" | "<=" | ">" | ">=" ) <Additive>

<Additive>          := <Multiplicative> { ( "+" | "-" ) <Multiplicative> }
<Multiplicative>    := <Unary>          { ( "*" | "/" | "%" ) <Unary> }

<Unary>             := ( "-" | "!" | "++" | "--" ) <Unary>
                    | <Postfix>

<Postfix>           := <Primary> [ <PostfixOp> ]
<PostfixOp>         := "++" | "--"

<Primary>           := INT_LITERAL
                    | FLOAT_LITERAL
                    | STRING_LITERAL
                    | BOOL_LITERAL
                    | IDENTIFIER [ <CallSuffix> ]
                    | "(" <Expression> ")"

<CallSuffix>        := "(" [ <Arguments> ] ")"
<Arguments>         := <Expression> { "," <Expression> }
```