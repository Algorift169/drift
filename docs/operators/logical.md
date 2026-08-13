# Logical Operators

Drift supports the following logical operators:

- `&&` logical AND
- `||` logical OR
- `!` logical NOT

## Syntax

```drift
var a = true && true
var b = true && false
var c = true || false
var d = !true
var e = !false
```

## Behavior

Logical operators evaluate to booleans. Numeric values are treated as truthy or falsy in the runtime. Strings with non-empty content are also considered truthy.

## Examples

```drift
say true && false
say true || false
say !false
```

## Output

```text
false
true
true
```

## Implementation notes

The logical evaluator checks the truthiness of each operand and then applies the correct boolean rule for `AND`, `OR`, or `NOT`.
