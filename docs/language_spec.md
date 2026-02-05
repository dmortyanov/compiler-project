# Language Specification (Sprint 1)

## 1. Character Set

- Input encoding: UTF-8.

## 2. Lexical Grammar (EBNF)

```
letter         = "A"…"Z" | "a"…"z" ;
digit          = "0"…"9" ;
underscore     = "_" ;
newline        = "\n" | "\r\n" ;
whitespace     = " " | "\t" | newline ;

identifier     = letter , { letter | digit | underscore } ;
int_literal    = digit , { digit } ;
float_literal  = digit , { digit } , "." , digit , { digit } ;
bool_literal   = "true" | "false" ;
string_literal = '"' , { character - '"' - newline | escape } , '"' ;
escape         = "\" , ( "n" | "t" | "r" | '"' | "\" ) ;

keyword        =
    "if" | "else" | "while" | "for" | "int" | "float" | "bool" |
    "return" | "true" | "false" | "void" | "struct" | "fn" ;

operator       =
    "+" | "-" | "*" | "/" | "%" |
    "==" | "!=" | "<" | "<=" | ">" | ">=" |
    "&&" | "||" | "!" |
    "=" | "+=" | "-=" | "*=" | "/=" ;

delimiter      = "(" | ")" | "{" | "}" | "[" | "]" | ";" | "," | ":" ;

comment        = line_comment | block_comment ;
line_comment   = "//" , { character - newline } ;
block_comment  = "/*" , { character - "*/" } , "*/" ;
```

## 3. Token Categories

- Keywords (reserved words)
- Identifiers
- Literals: integer, float, string, boolean
- Operators
- Delimiters

## 4. Regular Expressions (Informal)

- **Identifier:** `[A-Za-z][A-Za-z0-9_]*` (max length 255)
- **Integer:** `[0-9]+` with range [-2^31, 2^31-1]
- **Float:** `[0-9]+\.[0-9]+`
- **String:** `"([^"\r\n]|\\.)*"`
- **Boolean:** `true|false`

## 5. Whitespace & Comments

- Whitespace: space, tab, newline (`\n`), carriage return (`\r`)
- Line comments: `//` to end-of-line
- Block comments: `/* ... */` (nested comments not required)
