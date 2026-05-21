# MiniCompiler Language Specification

## Грамматика (EBNF)

```ebnf
program        = { decl } ;
decl           = var_decl | func_decl | struct_decl | extern_decl ;
var_decl       = type ident [ "[" [ int_lit ] "]" ] [ "=" expr | "{" expr_list "}" ] ";" ;
extern_decl    = "extern" "fn" ident "(" [ param_list ] ")" "->" type ";" ;
func_decl      = "fn" ident "(" [ param_list ] ")" "->" type block ;
struct_decl    = "struct" ident "{" { var_decl } "}" ;

param_list     = param { "," param } ;
param          = type ident [ "[" "]" ] ;
type           = "int" | "float" | "bool" | "string" | "void" | ident ;

block          = "{" { stmt } "}" ;
stmt           = var_decl | assign_stmt | if_stmt | while_stmt | for_stmt | return_stmt | expr_stmt ;

assign_stmt    = expr "=" expr ";" ;
if_stmt        = "if" "(" expr ")" block [ "else" block ] ;
while_stmt     = "while" "(" expr ")" block ;
for_stmt       = "for" "(" var_decl expr ";" assign_stmt_no_semi ")" block ;
return_stmt    = "return" [ expr ] ";" ;
expr_stmt      = expr ";" ;

expr           = logic_or ;
logic_or       = logic_and { "||" logic_and } ;
logic_and      = equality { "&&" equality } ;
equality       = comparison { ("==" | "!=") comparison } ;
comparison     = term { ("<" | "<=" | ">" | ">=") term } ;
term           = factor { ("+" | "-") factor } ;
factor         = unary { ("*" | "/" | "%") unary } ;
unary          = ("-" | "!") unary | postfix ;
postfix        = primary { "[" expr "]" | "(" [ expr_list ] ")" | "." ident } ;
primary        = int_lit | float_lit | bool_lit | string_lit | ident | "(" expr ")" ;

expr_list      = expr { "," expr } ;
```

## Типы данных
- `int` (32-bit integer)
- `float` (64-bit float - in progress)
- `bool` (1-bit, stored as 8-bit/32-bit)
- `string` (64-bit pointer)
- `void` (only for return types)
- `struct` (user-defined types)
- Массивы: `type[]`, статический размер, индексация с 0.

## Вызовы C/C++ функций
Вы можете объявлять внешние функции из libc (например, `printf`, `malloc`) с помощью ключевого слова `extern`. 
Внешние функции линкуются на этапе сборки GCC. Вариативные функции поддерживаются частично.
