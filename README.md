# Dahlia
Dahlia is a general purpouse scripting language.

## Key features
- Statically and weakly typed
- Immutable bindings by default
- Function arguments captured by reference.\

## Compilation
The project uses c++26 for it's checked arithmetic which requires libstdc++15 (g++15), otherwise it's all c++23.

to build the project run

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```
The cli executable is at `build/app/dahlia`, the tests are at `/build/test/dahlia_test`


## Interpreter usage

### Interpreter options
```
$ dahlia [options] [file]
```

| Option | Description |
|---|---|
| `--help`, `-h` | Print help information |
| `--file`, `-f` | The file to execute |
| `--program`, `-p` | The program to execute |
| `--result`, `-r` | Print the result of the main fuction |



### Running from a file
```
# example.dal
fn main() {
    println("Hello from file.");
}
```

```
$ dahlia -f example.dal
Hello from file.
```

### Running from input string
```
dahlia -p "fn main() { println("Hello from input string."); }"
Hello from input string.
```

## Language features

### Types
- [`int`](#int) - 64-bit signed integer
- [`float`](#float) - IEEE 754 64-bit float
- [`bool`](#bool) - true or false
- [`str`](#str) - a sequence of ASCII characters.
- [`vector`](#vec) - a dynamic size array


#### int
`int` is a 64-bit signed integer type with a range of `-2^63` to `2^63 - 1`. Overflow and underflow produce a [runtime error](#integer-underflow-and-overflow).

#### float
`float` is an IEEE 754 64-bit double precision floating point number. Inf and NaN are not allowed and produce a [runtime error](#invalid-float-states).

#### bool
`bool` can be either `true` or `false`.

#### str
`str` is a string of ascii characters. `\` can be used to escape special characters: `\`, `"`, `\r` `\n`, `\t`.

```
let a = "123\" \\abc";
println(a);
>>> 123" \abc
```
There is also hex escape `\x` used to escape a byte specified by two hexadecimal characters. Example with ansi color codes.

```
println("\x1B[31mRed text\x1B[0m");
>>> Red text    # Displayed in red.
```

Invalid escape sequences result in an error.
```
let a = "abc\"
ERROR: Invalid escape sequence in "abc\".
```


The `@` operator returns the size of the string.
```
let a = "123";
let size = @a;  # 3
```


#### Numeric literals
`int` and `float` support "_" separators for readability.
```
# Valid
int a = 100_200          # 100200
float b = 1_5.00_3      # 15.003

# Invalid
let a = _100;       # illegal - leading underscore
let a = 100_;       # illegal - trailing underscore
let a = 1__000;     # illegal - double separator
let a = 1_.0;       # illegal - separator directly before decimal point
let a = 1._0;       # illegal - separator directly after decimal point
```

### Built-in functions

`println(text : str)` - printlns to stdout and appends a new line symbol at the end.

```
println(123);
>>> 123
```


### Comments
The `#` character is used for comments
```
# This is a single line comment
```

### Variables

Variables are declared with the `let` keyword.
```
let a = 3;      # int
let b = 1.0;    # float
let c = "abc";  # str
```

The type can be stated using an annotation.
```
let a : int = 3;        # int
let b : float = 1.0;    # float
let c : str = "abc";    # str
```

Variables with type annotations will do no implicit conversions while binding.
```
let a : int = "4";
Error: types must explictly match with the type annotation
```

Variables are immutable and non reassignable by default unless specified otherwise with the `mut` keyword.

```
let a = 1;
a = 3;
Error: cannot assign to immutable variable "a"

let mut a = 1
a = 3;
```

When assinging, variables will attempt to convert to the variable type
```
let mut a = 3;
a = "4";
// a is now 4
```

If the conversion fails an error is thrown.
```
let mut a = 3;
a = "hello";
Error: string "hello" unparsable to "Int"
```

Assigning a variable value always results in a copy.
```
let mut a = 4;
let b = a;  # copy
a = 4       # b is not modified.
```

### Scopes
A new scope is created with `{}`. Variables declared inside a scope are only visible within it and are destroyed when the scope ends.
```
let a = 5;
{
    let a = 10;     # shadows outer a
    let b = 20;     # only visible inside this scope
}
# a is 5 here again
# b is not accessible here
```
Scopes are created implicitly by functions, if/else blocks, and loops.
```
if condition {
    let x = 5;      # x only exists inside this if block
}
# x is not accessible here
```

Variables cannot be redefined in the same scope.
```
let a = "ab";
let a = "cd";
Error: redefinition of variable "a" (previously defined at 3:1)
```
### Functions
Functions are declared with the `fn` keyword. Functions can only be defined in the top level scope.

```
# A simple function which adds two numbers.
fn add(a: int, b: int) -> int { return a + b; }

# A function with no return type (void).
fn test_void() { return; }
```
For non `mut` parameters, functions can convert the argument to match the parameter type.

```
let a = add("1", "2");
# a is now 3
```
All arguments are passed by reference. Return types are always copies.

Parameters can be made mutable with the `mut` keyword.
```
fn increment(mut a : int) {
    a += 1;
}

# a is modified on the outside
```

`mut` parameters only take `mut` variable arguments, and perform no converions.
```
increment(1);
Error: cannot pass expression to a mutable parameter
```

```
let mut a = "1";
increment(a);
Error: mutable parameters require exact type match
```

`mut` parameters require the argument variable to be mut

```
let a = 1;
increment(a);
Error: cannot pass immutable variable to mutable parameter
```

There can't exist two functions with the same name.
```
fn test() {}
fn test() {}
Error: redefinition of function "test" (previously defined at 3:1)
```

Parameter names can't be the same within a function.
```
fn test(a : int, a : int) {}
Error: duplicate function parameter "a"
```

Recursion example
```
fn factorial(n : int) -> int {
    if n < 2 {
        return 1;
    }
    return n * factorial(n-1);
}

```
The maximum call depth is 1000. Exceeding this limit, for example through infinite recursion, results in a runtime error.
```
fn foo() {
    foo();
}
foo();
Error: max call depth of 200 exceeded
```

Functions with a no return type can't be used in expressions.
```
fn void_fun() {}

let a = void_fun();
Error: void type in expression
```
### if/else

```
if condition {
    #
} else if condition {
    #
} else {
    #
}
```
The `else if` and `else` block are optional. Any type can be used as a condition. See [bool coercion](#logic-operators-and-bool-coercion)


### Loops

While loops
```
while condition {
    # Do something
}
```

For loops
```
# The left number is included, the right one is not.

for x in 0..4 {
    # Repeat 4 times with values of x: 0, 1, 2, 3
}

for x in 4..0 {
    # Repeat 4 times with values of x: 4, 3, 2, 1
}
```

To include the right number, use `=`.

```
for x in 0..=4 {
    # Repeat 5 times with values of x: 0, 1, 2, 3, 4
}
```

A step can be specified as an optional argument
```
for x in 0..=4..2 {
    # Repeat 3 times with values of x: 0, 2, 4
}

Providing an incorrect range results in an error.

for x in 0..4..-2 {}
Error: invalid for range
```

Both loop types support the `break` and `continue` operations.
```
for i in 0..8 {
    if i == 2 { continue; }
    else if i == 4 { break; }
    println(i);
}
>>> 0
>>> 1
>>> 3
```

The loop variable shadows other variables of the same name, exactly like block scope shadowing.
```
let a = 10;
for a in 0..4 {
    println(a);
}
println(a);
>>> 0
>>> 1
>>> 10
```
The loop variable can be declared `mut`. Mutating it only mutates a local copy in one iteration.

```
for mut a in 0..3 {
    if a == 1 { a = 60; }
    println(a);
}
>>> 0
>>> 60
>>> 2
```

Unary expressions like `@vec`, or indexing expressions like `vec[idx]` work directly in ranges. Expressions like `1 + 2` require parenthesis
```
for i in 0..(1 + 2) {  }

```


### Operators

| Operator | Description |
|---|---|
| `+` `-` `*` `/` `%` | Arithmetic |
| `+=` `-=` `*=` `/=` `%=` | Compound assignment |
| `==` `!=` `<` `>` `<=` `>=` | Comparison
| `and` `or` `!` | Logical AND, OR, NOT |


#### Coercion rules for arithmetic operators.
Order of operands never affects the result type.


__`+` operator__

`int` get promoted to float when needed. `str` always concatenates with string of the second operand. Order of operations only matters for `vec + non vec`, since `non vec + vec` will not prepend.
| Operant A | Operant B | Result | Notes |
|---|---|---|---|
| int | int | int | |
| float | float | float | |
| int | float | float | int promoted to float |
| bool | bool | int | both promoted to int |
| bool | int | int | bool promoted to int |
| bool | float | float | bool promoted to int, then float |
| str | str | str | concatenation |
| str | int | str | int stringified and appended |
| str | float | str | float stringified and appended |
| str | bool | str | concatenates "true" or "false" |
| vec | vec | vec | concatenation |
| vec | T | vec | element appended, but coerced into vec type, can throw |


__` - * / %` operators__

`int` get promoted to float when needed, `bool` is either promoted to `str` or `int`. Operations including `str`, convert it to `float` (except `int` * `str`).
| Operant A | Operant B | Result | Notes |
|---|---|---|---|
| int | int | int | `/` does integer division|
| float | float | float | |
| int | float | float | int promoted to float |
| bool | bool | int | both promoted to int |
| bool | int | int | bool promoted to int |
| bool | float | float | bool promoted to int, then float |
| str | int | str | `*` only: string repeated int times |
| str | int | float | `- / %` only: str parsed as float, int promoted to float |
| str | float | float | str parsed as float |
| str | bool | float | str parsed as float, bool promoted to int |
| str | str | float | both parsed as float |
| vec | any | ERROR | |


Common operation examples.
```
1.5 + 1;        # 2.5
"abc" + 1.51;   # "abc1.51"
"abc" + 1;      # "abc1"
"135" * 2;      # "135135"
```
If the conversion is not possible an error is thrown.
```
let a = 3.4 * "a";
Error: string "a" unparsable to "Float"
```

Other interactions.
```
# int / int is always integer division.
5 / 2   # 2

# modulo for negative numbers (like in c++)
-5 % 2   →  -1
5 % -2  →   1
```

#### Logic operators and bool coercion
Logic operators can be applied to any types. Each operand gets converted into a `bool`.
Values are considered falsy if they are:
- `0`, `0.0`
- `""` (empty string)
- Empty collection
- false

Everything else is truthy.

```
"" or 4;    # true
"" and 4;   # false

![1, 2]     # false
```

Conditions in `if` statemenets behave in the same way.
```
let falsy = 0;
let truthy = "abc";

if falsy { println(1); }
else if truthy { println(2); }
```

#### Comparison operators

`int` and `float` comparisons promote the integer to `float`. Because of how floats are built though, the condition will rarely ever be true after any arithmetic operations.
```
1 == 1.0        # true

1 == 2.2 / 1.1    # false
```


Strings can be compared using `<`, `>`, `<=`, `>=`. The comparison is lexicographical characters are compared one by one from left to right by their ASCII value. The first differing character determines the result.

```
"abc" < "abd"   # true - 'c' (99) < 'd' (100)
"abc" < "abcd"  # true - "abc" is a prefix, shorter string is less
"abc" == "abc"  # true - identical
"b" > "abc"     # true - 'b' (98) > 'a' (97), first character decides
"Z" < "abc"     # true - uppercase letters have lower ASCII values
"" < "a"        # true
```
`vec` comparison works the same way.
```
[1, 2] < [2, 100];  # true
```
The types stored must be comparable.
```
[1, 2] < [["3"]];
ERROR: cannot compare vec<int> with vec<vec<str>>.
```

Cross type comparisons are described in the tables below. The general rules are:
- incompatible types will throw
- `bool` `int` `float` freely convert between each other
- `str` `vec` only compare with their own type

__`== and !=` operators__

| Operands | Result | Notes |
|---|---|---|
| int, float | bool | int promoted to float |
| bool, int | bool | bool promoted to int |
| bool, float | bool | bool promoted to float |
| str, str | bool | exact character match |
| vec, vec | bool | element-wise, types must match |
| Incompatible types | bool | Error |


__`< > <= >=` operators__

| Operands | Result | Notes |
|---|---|---|
| int, float | bool | int promoted to float |
| bool, int | bool | bool promoted to int |
| bool, float | bool | bool promoted to int, then float |
| str, str | bool | lexicographical |
| vec, vec | bool | lexicographical, element types must match |
| incompatible types | Error | |

#### Vector operator
| `vec<T>` Operator | Description |
|---|---|
| `@v` | Size/length of collection |
| `v + elem` | Append element to collection |
| `v + v2` | Concatenate two collections |
| `x in v` | Check if element is in collection |
| `v ? pred` | Filter collection by predicate |
| `v :> fn` | Map collection by function |
| `v1 >< v2` | Intersection of two collections |
| `v[i]` | Access element at index i |


#### Operator assiociativity

| Level | Operators | Associativity |
|---|---|---|
| 1 (highest) | `[]`  | Left |
| 2 | `!` `-` (unary) `@` (unary) | Right |
| 3 | `*` `/` `%` | Left |
| 4 | `+` `-` | Left |
| 5 | `><` | Left |
| 6 | `in` | Left |
| 7 | `==` `!=` `<` `>` `<=` `>=` | Left |
| 8 | `and` | Left |
| 9 | `or` | Left |
| 10 | `?` `:>` | Left |


The order of evaluation can be explicitly stated using parentheses `(`,`)`
```
let a = 8 / 1 + 1   # 9
let b = 8 / (1 + 1) # 4
```

#### Logic operators

`and`, `or` and `!` work on any type and always produce a `bool`. Each operand is converted to a bool. The rules were already described in more detail in the if/else section.

#### Integer underflow and overflow
Integers can overflow and underflow. A runtime error is thrown when that happens.
```
# Integer overflow
let a = 9223372036854775807;    # INT64_MAX
let b = a + 1;
Error: overflow in arithmetic expression

# Integer underflow
let a = -9223372036854775808;   # INT64_MIN
let b = a - 1;
Error: overflow in arithmetic expression
```
Creating a variable with a value too big or too low too also throws an error.

#### Invalid float states
Floats can reach states like `NaN`, `-Inf`, `Inf`. A runtime error is thrown when that happens.

```
# Note that 2e308 syntax is not supported and is only here to show the number.

# Float overflow
let a = 2e308;
let b = a * 10.0;
Error: overflow in arithmetic expression

# Float invalid operation (same applies for int division by 0)
let a = 0.0 / 0.0;
Error: overflow in arithmetic expression

# Float underflow
let a = 5e-323;
let b = a / 100.0;
Error: overflow in arithmetic expression
```

### Collections

#### vec
Vector or `[]` is an ordered collection of elements packed consecutively in memory.

Initialization
```
let nums : [int] = [1, 2, 3, 4, 5,]; # optional trailing comma
let nums = [1, 2, 3, 4, 5];             # The type can be skipped, if the list is not empty.

let nums = [];
Error: cannot infer type of empty vector, type annotation required

let nums : [int] = [];


# The values must all be the same type
let nums = [1, 2, "3"];     # No coercion
Error: values in vec are not of the same type
```

Elements can be indexed with the `[index]` operator.
```
let nums = [1, 2, 3, 4, 5,];

let a = nums[1];    # 2
let b = nums[10];
Error: index 10 is out of bounds

```
Indexing can be used to modify the element at a given index.
```
let mut nums = [1, 2, 3];
nums[1] = -1;
nums[2] = "10";     # Converted to vector type

# nums is now [1, -1, 10]
```
The `@` operator returns the number of elements in the vector.
```
let nums = [1, 2, 3, 4 , 5]
let size = @nums;   # 5
```


Elements can be appended with the `+` and `+=` operator.
`+` always creates a new vector.
```
let mut a = [1, 2];
a += 3;             # [1, 2, 3]

let b = a + 4;      # independent vector [1, 2, 3, 4]
```

Appending a non vector, will attempt to convert the appended element to match the vector,
```
let a = [1, 2] + "3";
# [1, 2, 3]
```

Vectors can be concatenated with the `+` and `+=` operator. Types of vectors must explictly match.
```
let a = [1, 2] + [3, 4];    # [1, 2, 3, 4]

let a = [1, 2] + ["3"];
Error: invalid operands [Int], [Str]
```

When appending or concatenating, the types of vectors decide the correct operation. Order of operands only affects the side on which the operation happens.
- if the types of vectors match -> concatenate
- if one of the vectors' contained type, matches the type of the other vector - > append

```
[1, 2, 3] + 0               # [1, 2, 3, 0] - append, int matches element type
[1, 2, 3] + "0"             # [1, 2, 3, 0] - append, str converted to int
[1, 2, 3] + "abc"           # Error: string "abc" unparsable to "Int"

[1, 2, 3] + [4]             # [1, 2, 3, 4] - concatenation
[[1, 2], [3, 4]] + [5, 6]       # [[1, 2], [3, 4], [5, 6]] - append, vec<int> matches element type
[[1, 2], [3, 4]] + [[5, 6]]     # [[1, 2], [3, 4], [5, 6]] - concatenation

[[1, 2]] + []                   # [[1, 2], []]
```
The `in` operator can be used, to check if an element is contained in the vector.
```
let nums = [1, 2, 3];
let t = 1 in nums;      # true
let f = -1 in nums;     # false
```

Vectors can be filtered by a predicate with the `?` operator. The `?` operator returns a new vector containing only the elements that satisfy the predicate.

```
fn odd(n : int) -> bool { return n % 2; }

let odd = nums ? odd;        # [1, 3, 5]
```

The `:>` operator applies a function to each element, returning a new vector. The original vector is unchanged.

```
fn square(n : int) -> int { return n*n; }

let squared = nums :> square;    # [1, 4, 9, 16, 25]
```
If the predicate signature for `:>` or `?` doesn't match the vector's element type a runtime error is produced.
```
fn min(n : [int]) -> int { ... }

let nums = [1, 2, 3]
let a = nums :> min;
Error: cannot convert from Int to [Int]
```

The `><` operator returns elements common to both vectors. Duplicates are removed.
```
let a = [1, 1, 3, 4];
let b = [1, 1, 1, 4];
let c = a >< b;         # [1, 4]
```


### Error messages
When an error is detected, the interpreted program is stopped.
The error is logged in the following format.

`ERROR in {file_name}:{line}:{column}: Error: {message}`

Example error:
```
Stacktrace:
example.dal:1:1 in main
example.dal:6:9: Error: cannot infer type of empty vector, type annotation required
  | let a = [];
  |         ^
```

## Code examples
Code examples are in the `examples` directory.
