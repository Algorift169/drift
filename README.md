# Drift

Drift is a small interpreted scripting language written in C. It follows a basic interpreter pipeline:

Source code -> Lexer -> Tokens -> Parser -> AST -> Interpreter -> Output

## Features

- `say` statements for output
- variable declarations with `var`
- string, integer, float, and boolean literals
- simple REPL mode
- file-based execution
- interpolation support for values like `say "value: {name}"`

## Build

From the project root:

```bash
make
```

This creates the binary in `./build/drift`.

To remove build artifacts:

```bash
make clean
```

## Run

### REPL

```bash
./build/drift
```

Then enter commands such as:

```text
>>> say "Hello, Drift!"
>>> var name = "Israfil"
>>> say "Hello, {name}!"
>>> exit
```

### File execution

```bash
./build/drift examples/hello.drift
```

## Example programs

```drift
say "Hello, World!"
var name = "Israfil"
var age = 20
say "Name: {name}"
say "Age: {age}"
```

## Notes

- The interpreter keeps a runtime environment for declared variables.
- Variable declarations are literal-based and intentionally simple.
- The project is organized into lexer, parser, token, interpreter, and runtime pieces.

## Project layout

```text
.
├── main.c
├── Makefile
├── include/
├── lexer/
├── parser/
├── token/
├── interpreter/
├── docs/
└── build/
```

## Documentation

See the docs folder for language notes and milestones:

- [docs/printing/p1.md](docs/printing/p1.md)
- [docs/variables/v1.md](docs/variables/v1.md)
