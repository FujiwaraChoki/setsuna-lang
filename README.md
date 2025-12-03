# Setsuna

A modern functional programming language with Hindley-Milner type inference, pattern matching, and a clean, familiar syntax.

## Features

- **First-class functions** - Lambdas, closures, and higher-order functions
- **Pattern matching** - Powerful match expressions with destructuring
- **Type inference** - Automatic types via Hindley-Milner algorithm
- **Algebraic data types** - Define custom types with multiple constructors
- **Records & Tuples** - Structured data with named/positional fields
- **Modules** - Organize code into namespaces
- **Rich standard library** - `map`, `filter`, `fold`, and 50+ utility functions

## Quick Start

### Build

```bash
mkdir build && cd build
cmake ..
make
```

### Run

```bash
# Interactive REPL
./build/setsuna

# Execute a file
./build/setsuna examples/factorial.stsn
```

### Install System-Wide

```bash
sudo make install
```

This installs the `setsuna` command to `/usr/local/bin` and the standard library to `/usr/local/share/setsuna/`.

## Examples

### Functions & Pattern Matching

```setsuna
fn factorial(n) {
    match n {
        0 => 1,
        _ => n * factorial(n - 1)
    }
}

print(factorial(10))  // 3628800
```

### Lambdas & Higher-Order Functions

```setsuna
let numbers = [1, 2, 3, 4, 5]
let doubled = map((x) => x * 2, numbers)
let evens = filter((x) => x % 2 == 0, numbers)

print(doubled)  // [2, 4, 6, 8, 10]
print(evens)    // [2, 4]
```

### Algebraic Data Types

```setsuna
type Option { Some(x), None }
type Tree { Leaf(x), Node(left, right) }

fn tree_sum(tree) {
    match tree {
        Leaf(x) => x,
        Node(l, r) => tree_sum(l) + tree_sum(r)
    }
}

let t = Node(Node(Leaf(1), Leaf(2)), Leaf(3))
print(tree_sum(t))  // 6
```

### Records

```setsuna
let person = { name: "Alice", age: 30 }

fn greet(p) {
    match p {
        { name: n, age: a } => println("Hello, " + n + "!")
    }
}

greet(person)  // Hello, Alice!
```

### Modules

```setsuna
module Math {
    fn square(x) => x * x
    fn cube(x) => x * x * x
}

print(Math.square(5))  // 25
print(Math.cube(3))    // 27
```

## Language Reference

### Types

| Type | Examples |
|------|----------|
| Integer | `42`, `-7`, `0` |
| Float | `3.14`, `-0.5` |
| Boolean | `true`, `false` |
| String | `"hello"`, `"world"` |
| List | `[1, 2, 3]`, `[]` |
| Tuple | `(1, "a", true)` |
| Record | `{ x: 1, y: 2 }` |
| Function | `(x) => x + 1` |

### Built-in Functions

**I/O**: `print`, `println`

**Lists**: `head`, `tail`, `cons`, `len`, `append`, `concat`, `reverse`, `nth`, `sort`

**Math**: `abs`, `sqrt`, `pow`, `sin`, `cos`, `tan`, `log`, `exp`, `min`, `max`, `floor`, `ceil`, `round`

**Strings**: `substr`, `split`, `join`, `uppercase`, `lowercase`, `trim`, `contains`, `starts_with`, `ends_with`, `replace`

**Conversion**: `int`, `float`, `str`

**Constants**: `pi`, `e`

### Standard Library (Prelude)

The prelude is automatically loaded and provides:

- `map`, `filter`, `fold`, `reduce`, `fold_right`, `flat_map`
- `take`, `drop`, `take_while`, `drop_while`
- `zip`, `zip_with`, `unzip`
- `any`, `all`, `none`, `find`, `find_index`
- `sum`, `product`, `maximum`, `minimum`, `count`
- `compose`, `pipe`, `identity`, `constant`, `flip`
- `flatten`, `intersperse`, `partition`, `enumerate`, `group`, `unique`
- `last`, `init`, `span`, `range`

## Requirements

- C++17 compiler (GCC 8+, Clang 7+, MSVC 2019+)
- CMake 3.16+

## License

MIT
