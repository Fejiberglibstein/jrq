
# Examples

```json
[
    {
        "userID": "heuwhiuasj",
        "message": "Hello world",
        "username": "bej"
    },
    {
        "userID": "heuwhiuasj",
        "message": "whats up",
        "username": "bej"
    },
    {
        "userID": "chuwhvuisd",
        "message": "nothing",
        "username": "gfsg"
    },
]
```

.filter{(v) v.userID == "heuwhiuasj"}.map{(v) v.keys{}.enumerate{}.map{(\[key, i]) {key: i}}.flatten{}}.collect{}
.filter(|v| v.userID == "heuwhiuasj").map(|v| v.keys().enumerate().map(|\[key, i]| {key: i}).flatten()).collect()
filter (v) v.userID == "heuwhiuasj" |> map ((v) v |> keys |> enumerate |> map (\[key, i]) {key: i} |> flatten ) |> collect


this outputs

```json
[
    {
        "userID": 0,
        "message": 1,
        "username": 2
    },
    {
        "userID": 0,
        "message": 1,
        "username": 2
    },
]

```

# Grammar


```js

EXPRESSION -> LOGIC_OR
LOGIC_OR   -> LOGIC_AND ( "||" LOGIC_AND )*
LOGIC_AND  -> EQUALITY ( "&&" EQUALITY )*
EQUALITY   -> COMPARISON ( ( "!=" | "==" ) COMPARISON )*
COMPARISON -> TERM ( ( ">" | ">=" | "<" | "<=" ) TERM )*
TERM       -> FACTOR ( ( "-" | "+" ) FACTOR )*
FACTOR     -> UNARY ( ( "/" | "*" ) UNARY )*
UNARY      -> ( "!" | "-" ) unary | call
ARGUMENTS  -> EXPRESSION ("," EXPRESSION)*
CALL       -> PRIMARY ( "(" ARGUMENTS ")" | "." IDENTIFIER)*
PRIMARY    -> "true" |  "false" | "null"  | NUMBER | STRING |
            IDENT  | "(" EXPRESSION ")" | JSON_OBJ | LIST

JSON_OBJ   -> "{" PRIMARY ":" EXPRESSION ( "," PRIMARY ":" EXPRESSION)* "}"
LIST       -> "[" EXPRESSION ("," EXPRESSION)* "]"
```


# Functions
