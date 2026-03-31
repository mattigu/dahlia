# Dahlia
Dahlia is a general purpouse scripting language.

## Key features
- Statically and weakly typed
- Immutable bindings by default
- Reference semantics for objects, value semantics for primitives

## Interpreter usage

### Interpreter options
```
$ dal [options] [file]
```

| Option | Description |
|---|---|
| `--help`, `-h` | Print help information |
| `--version`, `-v` | Print the interpreter version |
| `--no-warnings`, `-w` | Disable all warnings |

### Running from a file
```
# example.dal
fn main() {
    println("Hello from file.");
}
```

```
$ dal example.dal
Hello from file.
```

### Running from a stream

```
$ echo 'fn main() { println("Hello from stream."); }' | dal
Hello from stream.
```

## Language features

### Types
- [`int`](#int) - 64-bit signed integer
- [`float`](#float) - IEEE 754 64-bit float
- [`bool`](#bool) - true or false
- [`str`](#str) - a sequence of ASCII characters
- [`struct`](#struct) - a user defined type that groups named fields together
- [`vector`](#vec) `vec<T>` - a homogeneous collection where `T` is the element type. See [Collections](#collections)

#### int
`int` is a 64-bit signed integer type with a range of `-2^63` to `2^63 - 1`. Overflow and underflow produce a [runtime error](#integer-underflow-and-overflow).

#### float
`float` is an IEEE 754 64-bit double precision floating point number. Inf and NaN are not allowed and produce a [runtime error](#invalid-float-states).

#### bool
`bool` can be either `true` or `false`.

#### str
`str` is a string of ascii characters. `\` can be used to escape special characters: `\`, `"`, `\r`, `\n`, `\t`.

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

Strings are indexable.
```
let a = "123";
let b = a[0];   # 1 (copy)
```
Individual characters can be modified.
```
let mut a = "123";
a[1] = "8";         # "183"
```
When indexing out of bound. This is the same indexing a vector out of bounds.


The `@` operator returns the size of the string.
```
let a = "123";
let size = @a;  # 3
```
#### struct
!!! Structs might not be implemented, but this is roughly how they would work.

`struct` is a user defined type that groups named fields together.
Structures can only be defined in the top level scope.
```
struct Point {
    x: int,
    y: int,     # Comma after last field is optional
}


let p = Point { x: 1, y: 2 };   # named field initialization
let x = 1;
let p = Point { x, y: 2 };      # shorthand when variable name matches field name
```
Field mutability follows the binding - if the binding is `mut`, all fields are mutable:
```
let p = Point { x: 1, y: 2 };
p.x = 5;                        # error - p is immutable

let mut p = Point { x: 1, y: 2 };
p.x = 5;                        # fine
```

Invalid states
```
# A struct must have at least 1 field. ( May be changed if methods are added )

struct Point {}
ERROR: struct "Point" must have at least 1 field.


# Field names must not repeat inside a struct.
struct Point {
    a : int,
    a : int,
}
ERROR: field "a" is already defined in struct "Point"

# A struct with the same name can't be redefined.
struct Point { ... }
struct Point { ... }
ERROR: struct "Point" is already defined.
```

#### Numeric literals
`int` and `float` support "_" separators for readability
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

`clone(obj : T) -> T` - creates a deep copy of the object.

```
let a = "123"

let b = a;              # immutable reference
let mut c = a           # illegal
let mut d = clone(a);   # mutable independent copy
```

`str(obj : str)` `float(obj : float)` `int(obj : int)` are functions that cast the argument to the given type.

```
str(1);         # "1"
str(1.23);      # "1,23"

int(2.71);      # 2         Floating point numbers are truncated towards 0.
int("617");     # 617

float("1.23");  # "1.23"
float(1);       # "1.0"
```
If the conversion is impossible an error is raised.
```
int("a");
ERROR: str "a" can not be converted to "int".
```

### Comments
The `#` character is used for comments
```
# This is a single line comment
```

### Variables

Variables are declared with the `let` keyword.
```
let a : int = 1;
```

If no type is specified, the variable’s type is inferred from the assigned expression. The inferred types are `int`, `float` or `str`.

```
let a = 3;      # int
let b = 1.0;    # float
let c = "abc";  # str
```

When a variable is assigned from another variable, the source variable’s type is used.

```
let a : float = 3.0;
let b = a;              # float
let c : int = a;        # int
```

Variables are weakly(loosely) typed. Numbers and strings are easily convertible both ways.
When a target type is known then `let a : type = expression` is equivalent to `let a = type(expression)`

```
let a : str = 123;      # "123"
let b : str = 1.12;     # "1.12"

let c : int = "123";    # 123
let d : float = "1.12"; # 1.12
```

If the type of the expression is not convertible to the type of the variable, an error is raised.
```
let a : int = [1, 2, 3];    # ERROR: vector "[1, 2, 3]" is not convertible to int
```

Variables are immutable and non reassignable by default unless specified otherwise with the `mut` keyword.

```
let a = 1;
a = 3;
ERROR: assignment to immutable variable "a".

let mut a = 1
a = 3;
```

When assigning from another variable, `int`, `float` and `bool` are copied. All other types are referenced.

```
let a = 3;
let b = a;  # copy

let c = [1, 2, 3]
let d = c;  # immutable reference
```

This requires that a mutable binding cannot reference an immutable object.
```
let a = 3;
let mut b = a;      # copy

let c = [1, 2, 3]
let mut d = c;      # error

let mut c = [1, 2, 3];
let d = c;                  # immutable reference

let mut e = [1, 2, 3];
let mut f = e;              # mutable reference
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

Variables can be redefined in the same scope.
```
let a = "abc";
let b = a;
let a = 1;

# "abc" still exists, but can only be accessed through b.

println(b);
>>> abc
println(a);
>>> 1
```
### Functions
Functions are declared with the `fn` keyword. Functions can only be defined in the top level scope.

```
# A simple function which adds two numbers.
fn add(a: int, b: int) -> int { return a + b; }

# A function with no return type (void).
fn test_void() { return; }
```

Parameters can be made mutable with the `mut` keyword.
```
fn add(mut a : int, b : int) -> int {
    a += b;
    # b += a will not work since b is immutable
    return a;
}
```

All arguments are passed by reference. Return types (`int`, `float`, `bool`) are copied, while everything else is referenced.
```
fn test(mut a : vec<int>, b : vec<int>) -> int {
    # a is a mutable reference
    # b is an immutable reference
}
```

The arguments will be converted to match the functions signature if possible.
```
fn add(a : int) { ... }

add("12") # equivalent to add(12)

add("abc")
ERROR: argument "abc" is not convertible to "int"
```

There can't exist two functions with the same name.
```
fn test() {}
fn test() {}
ERROR: function "test" was already defined.
```

Parameter names can't be the same within a function.
```
fn test(a : int, a : int) {}
ERROR: multiple parameters with the identifier "a" in function "fn test(a : int, a : int)"
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
ERROR: maximum call depth of 1000 exceeded
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

It's also possible to do if/else expressions.

```
# Type is mandatory, the result will attempt to get coerced into that type.
let a : int = if condition { 5 } else { "10" }; # else is mandatory

let a = if ...
ERROR: if expression requires a specified type.
```
If expressions are non recursive, and only work when assigning, or creating a new binding.

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

Providing an incorrect step results in an error.

for x in 0..4..-2 {}
ERROR: step must be positive for forward range (0..4..-2)
HINT: did you mean 0..4..2
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
    println("{}", a);   # prints 0, 1, - a is the loop variable
}
println(a);         # prints 10 - outer a is back
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

It's possible to use expressions in the `a..b` for loop syntax. Parentheses are required. `..` is not an operator, just syntax sugar for loops.
```
for i in 0..(1 + 2) { println(i); }
>>> 0
>>> 1
>>> 2
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

`int` get promoted to float when needed. `str` always concatenates with string of the second operand.
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
| vec | T | vec | element appended, must match element type |


__` - * / %` operators__

`int` get promoted to float when needed, `bool` is either promoted to `str` or `int`. Operations including `str`, convert the other operand to float (except `int` * `str`).
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
ERROR: cannot evaluate (3.4 * "a") - str "a" is not parseable as float.
```

Other interactions.
```
# int / int is always integer division.
5 / 2   # 2

# modulo for negative numbers
-5 % 2      # 1
5 % -2      # -1
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
1 == 1.0    # true

2 == 1.0 + 1.0          # false
2 == int(1.0 + 1.0)     # true
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
- `== and !=` nevers throws, while `< > <= >=` can throw if comparison is impossible
- `== and !=` for `str` and anything other than `str` is always false
- `< > <= >=` tries to parse all strings as floats.

__`== and !=` operators__

| Operands | Result | Notes |
|---|---|---|
| int, float | bool | int promoted to float |
| bool, int | bool | bool promoted to int |
| bool, float | bool | bool promoted to int, then float |
| str, str | bool | exact character match |
| str, non-str | bool | always false |
| vec, vec | bool | element-wise, incompatible element types always false |
| incompatible types | bool | always `false`, never throws |

__`< > <= >=` operators__

| Operands | Result | Notes |
|---|---|---|
| int, float | bool | int promoted to float |
| bool, int | bool | bool promoted to int |
| bool, float | bool | bool promoted to int, then float |
| str, str | bool | lexicographical |
| str, int | bool | str parsed as float, int promoted to float |
| str, float | bool | str parsed as float |
| str, bool | bool | str parsed as float, bool promoted to int |
| vec, vec | bool | lexicographical, element types must match |
| incompatible types | ERROR | |

All other combinations throw.
If the type is not convertible also throw.
```
"abc" > 4;
ERROR: cannot evaluate ("abc" > 4) - str "abc" is not parseable as float
```

#### Integer underflow and overflow
Integers can overflow and underflow. A runtime error is thrown when that happens.
```
# Integer overflow
let a = 9223372036854775807;    # INT64_MAX
let b = a + 1;
ERROR: Integer overflow in expression (9223372036854775807 + 1)

# Integer underflow
let a = -9223372036854775808;   # INT64_MIN
let b = a - 1;
ERROR: Integer underflow in expression (-9223372036854775808 - 1)
```
Creating a variable with a value too big or too low to also throws an error.

#### Invalid float states
Floats can reach states like `NaN`, `-Inf`, `Inf`. A runtime error is thrown when that happens.

```
# Note that 2e308 syntax is not supported and is only here to show the number.

# Float overflow
let a = 2e308;
let b = a * 10.0;
ERROR: Float overflow in expression (1.8e308 * 10.0)

# Float invalid operation (same applies for int division by 0)
let a = 0.0 / 0.0;
ERROR: Invalid float operation (0.0 / 0.0) - result is NaN

# Float underflow
let a = 5e-323;
let b = a / 100.0;
ERROR: Float underflow in expression (5e-324 / 10.0)
```

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

Comparison is done by comparing values.
```
"123" == "123"  # Always true
```

#### Logic operators

`and`, `or` and `!` work on any type and always produce a `bool`. Each operand is converted to a bool. The rules were already described in more detail in the if/else section.

### Collections

#### vec
Vector or `vec<T>` is an ordered collection of elements packed consecutively in memory.

Initialization
```
let nums : vec<int> = [1, 2, 3, 4, 5,]; # optional trailing comma
let nums = [1, 2, 3, 4, 5];             # The type can be skipped, if the list is not empty.

let nums : vec<int> = [];
let nums = [];
ERROR: The type of "nums" cannot be inferred.

# The values must all be the same type
let nums : vec<int> = [1, 2, "3"];      # No coercion
ERROR: "3" is not of type "int"
```

Elements can be indexed with the `[index]` operator.
```
let nums = [1, 2, 3, 4, 5,];

let a = nums[1];    # 2
let b = nums[10];
ERROR: index out of bounds - index 10 is out of range for vec<int> "b" of length 3

# Indexing follows existing reference semantics.
let a = nums[0];        # copy
let mut b = nums[1];    # copy

let letters = ["a", "b", "c"];
let a = letters[0];         # immutable reference
let mut b = letters[1];     # illegal - letters is not mut
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
Appending follows existing reference semantics. `int`, `float`, `bool` are appended as copies, others referenced.

```
let mut a = [1, 2];
a += 3;             # [1, 2, 3]

let b = a + 4;      # independent vector [1, 2, 3, 4]
```
Elements can also be inserted at the start when creating a vector with the `+` operator.
```
let a = 1 + [2, 3, 4]   # [1, 2, 3, 4]
```
Appending a non vector, will attempt to convert the appended element to match the vector,
```
let a = [1, 2] + "3";
```

Vectors can be concatenated with the `+` and `+=` operator. Types of vectors must explictly match.
```
let a = [1, 2] + [3, 4];    # [1, 2, 3, 4]

let a = [1, 2] + ["3"]
ERROR: Can't concatenate vectors vec<int>[1, 2] with vec<str>["3"] of non matching types.
```

When appending or concatenating, the types of vectors decide the correct operation. Order of operands only affects the side on which the operation happens.
```
[1, 2, 3] + 0               # [1, 2, 3, 0] - append, int matches element type
[1, 2, 3] + "0"             # [1, 2, 3, 0] - append, str converted to int
[1, 2, 3] + "abc"           # ERROR: cannot append str "abc" to vec<int>
[1, 2, 3] + [4]             # [1, 2, 3, 4] - concatenation
[[1, 2], [3, 4]] + [5, 6]       # [[1, 2], [3, 4], [5, 6]] - append, vec<int> matches element type
[[1, 2], [3, 4]] + [[5, 6]]     # [[1, 2], [3, 4], [5, 6]] - concatenation

[1, 2] + []                     # [1, 2]
[[1, 2]] + []                   # [[1, 2], []]

```
The `in` operator can be used, to check if an element is contained in the vector.
```
let nums = [1, 2, 3];
let t = 1 in nums;      # true
let f = -1 in nums;     # false
```

Vectors can be filtered by a predicate with the `?` operator. The ? operator returns a new vector containing only the elements that satisfy the predicate.

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
fn min(n : vec<int>) -> int { ... }

let a = nums :> min;
ERROR: predicate "min" and "nums" do not match.
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

`ERROR in {{file_path} at line { line } column { column }: {message}`

An additional hint can be provided if the cause of the error easy to detect.

Example

```
ERROR in ./example.dal at line 10, column 43
    Error description
HINT: Hint description
```

### Warnings
Warnings might be reported when code that is likely to be a mistake is detected.

The warning is logged in the following format.
`WARNING in { file_path } at line { line } column { column }: { message }`

```
if 1 { ... }
WARNING: expression "1" always evaluates to true.
```

```
let a = 5;
# a never used
WARNING: variable "a" is declared but never used.
```

### Possible features
- Importing other files
- Vector initialization similar to `for x in 0..5` loop syntax.
```
let a = [0..5..2]   # [0, 2, 4]
```
- indexing vectors with the syntax described above. `let a = some_vec[0..5]`. Could also make a slice type for vecs along with this.
- release mode which doens't check number safety?
- separation of variable declaration and definition.

Variable declaration and definition can be separate, although the type must be specified before assignment.
```
let a : int;
a = 10;

let b;
b = 3
ERROR: The type of "b" cannot be inferred.
```

The value must be initialized before use.
```
let a: int;
myfunction(a);
ERROR: use of uninitialized variable "a".
```
- `for x in vec/str` loops
- `Option<T>` type
- `struct` methods
- `tuple` or `pair<T, V>` with unpacking syntax
- `enum`
- `Result<T, E>` for errors