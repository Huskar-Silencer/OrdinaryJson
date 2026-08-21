# OrdinaryJson Test Documentation

This document describes the complete test plan and results for the `ordinaryjson`
lightweight JSON library, with a focus on business edge cases.

## 1. Environment and Method

- Language standard: C++14 (matching `CMakeLists.txt`)
- Compiler: MSVC (`cl.exe`, `/W4`, no warnings)
- Code under test: `src/ordinary_json.hpp` / `src/ordinary_json.cpp`
- Test program: `../tests/test_ordinary_json.cpp`

### Build and run

```bash
# MSVC
cl /nologo /EHsc /std:c++14 /W4 /I src tests/test_ordinary_json.cpp src/ordinary_json.cpp

# GCC / Clang
g++ -std=c++14 -Wall -Wextra -I src tests/test_ordinary_json.cpp src/ordinary_json.cpp -o oj_test
```

The test program uses a lightweight assertion framework, prints `[PASS]/[FAIL]`
grouped by category, and reports the totals at the end.

## 2. Result Summary

| Item        | Value |
|-------------|-------|
| Total cases | 257   |
| Passed      | 257   |
| Failed      | 0     |

### By category

| Category                                | Cases | Passed |
|-----------------------------------------|-------|--------|
| 1. Basic values                         | 3     | 3      |
| 2. Integer boundaries                   | 9     | 9      |
| 3. Double / fractional boundaries       | 7     | 7      |
| 4. Scientific notation                  | 6     | 6      |
| 5. String basics                        | 4     | 4      |
| 6. String escapes                       | 8     | 8      |
| 7. Unicode escapes                      | 4     | 4      |
| 8. Invalid strings                      | 9     | 9      |
| 9. Objects                              | 9     | 9      |
| 10. Arrays                              | 7     | 7      |
| 11. Whitespace                          | 6     | 6      |
| 12. Nesting depth                       | 5     | 5      |
| 13. Invalid documents                   | 31    | 31     |
| 14. Serialization                       | 14    | 14     |
| 15. Round-trip stability                | 18    | 18     |
| 16. Accessors / type errors             | 8     | 8      |
| 17. Value semantics                     | 11    | 11     |
| 18. Duplicate keys                      | 1     | 1      |
| 19. Exception hierarchy                 | 4     | 4      |
| 22. Numeric overload disambiguation     | 6     | 6      |
| 23. Real-world business scenarios       | 41    | 41     |
| 24. More invalid inputs                 | 30    | 30     |
| 25. Container access (iteration / size) | 16    | 16     |

## 3. Case Details

### 3.1 Basic values

| Input   | Expected               |
|---------|------------------------|
| `null`  | `IsNull()`             |
| `true`  | `GetAsBool() == true`  |
| `false` | `GetAsBool() == false` |

### 3.2 Integer boundaries (key)

| Input                                      | Expected                          |
|--------------------------------------------|-----------------------------------|
| `0` / `1` / `-1` / `42`                    | parsed exactly as `int64_t`       |
| `9223372036854775807` (`INT64_MAX`)        | parsed exactly, no precision loss |
| `-9223372036854775808` (`INT64_MIN`)       | parsed exactly, no precision loss |
| `9223372036854775808` (`INT64_MAX+1`)      | throws `ParseError` (overflow)    |
| `18446744073709551616` (`2^64`)            | throws `ParseError` (overflow)    |
| `-9223372036854775809` (below `INT64_MIN`) | throws `ParseError` (overflow)    |

> Integers are stored as `int64_t`, so values above `2^53` are preserved exactly.

### 3.3 Double / fractional boundaries

| Input                                      | Expected                                   |
|--------------------------------------------|--------------------------------------------|
| `1.5` / `-2.5` / `0.5` / `123.456` / `0.1` | parsed exactly as `double`                 |
| `1.0`                                      | stored as `double` (distinct from integer) |

### 3.4 Scientific notation

| Input         | Expected   |
|---------------|------------|
| `1e5` / `1E5` | `100000.0` |
| `1e-5`        | `0.00001`  |
| `2.5e3`       | `2500.0`   |
| `-1.5E-2`     | `-0.015`   |
| `1e+5`        | `100000.0` |

### 3.5 ~ 3.7 Strings

| Input                                   | Expected                                           |
|-----------------------------------------|----------------------------------------------------|
| `""` / `"hello"`                        | empty / plain string                               |
| `"  a  "`                               | leading/trailing spaces preserved                  |
| `\n` `\t` `\r` `\b` `\f` `\"` `\\` `\/` | decoded to the corresponding character             |
| `\u0041`                                | `A`                                                |
| `\u4e2d`                                | `中` (UTF-8 `E4 B8 AD`)                            |
| `\ud83d\ude00`                          | 😀 (surrogate pair → U+1F600, UTF-8 `F0 9F 98 80`) |

### 3.8 Invalid strings (all throw `ParseError`)

| Input                         | Reason                                       |
|-------------------------------|----------------------------------------------|
| `"a\x"`                       | invalid escape `\x`                          |
| `"\uZZZZ"` / `"\u123"`        | invalid / truncated Unicode escape           |
| `"\ud800"` / `"\udc00"`       | lone high/low surrogate                      |
| `"\ud800\u0041"`              | high surrogate followed by non-low surrogate |
| `"unterminated`               | unterminated string                          |
| `"line\nbreak"` (raw newline) | raw control character in string              |
| `"ctrl\x01char"`              | raw control character 0x01                   |

### 3.9 Objects / 3.10 Arrays

- Empty object/array, empty containers with whitespace, single/multiple elements, nested structures, mixed types, empty
  keys, escaped keys, Unicode keys.
- Whitespace may appear between any tokens (`{"a" : 1}`, `[ 1 , 2 ]`).

### 3.11 Whitespace

`' '`, `'\t'`, `'\n'`, `'\r\n'` and mixed whitespace are correctly ignored whether leading, trailing, or between tokens.

### 3.12 Nesting depth (key boundary)

| Depth               | Expected                                                                            |
|---------------------|-------------------------------------------------------------------------------------|
| 1 / 50 / 100 levels | parsed normally                                                                     |
| 101 / 500 levels    | throws `ParseError` (exceeds `kMaxParseDepth = 100`, guards against stack overflow) |

### 3.13 Invalid documents (all throw `ParseError`)

| Input                                  | Reason                                     |
|----------------------------------------|--------------------------------------------|
| `""`, `"   "`                          | empty / whitespace-only input              |
| `-`                                    | lone minus                                 |
| `01`, `-01`                            | leading zero                               |
| `1.`, `1e`, `1e+`                      | incomplete fractional / exponent part      |
| `+1`, `.5`, `1.2.3`                    | leading plus / leading dot / multiple dots |
| `[1,]`, `{"a":1,}`                     | trailing comma                             |
| `{"a" 1}`, `{"a":1 "b":2}`, `[1 2]`    | missing colon / missing comma              |
| `[`, `{`, `"abc`                       | unterminated container / string            |
| `tru`, `fals`, `nul`, `truex`, `nullx` | truncated literal / trailing junk          |
| `abc`, `1 2`, `{} {}`, `"a" "b"`       | garbage / multiple root values             |
| `'single'`, `/*comment*/`              | single quotes / comments                   |
| `NaN`, `Infinity`                      | non-standard literals                      |

### 3.14 Serialization (Stringify)

| Input                                    | Output                        |
|------------------------------------------|-------------------------------|
| `null` / `true` / `false` / `42` / `1.5` | unchanged                     |
| `{}` / `[]`                              | `{}` / `[]`                   |
| `"hello"`                                | `"hello"` (quoted)            |
| `[1,"two",null]`                         | `[1,"two",null]`              |
| `{"b":1,"a":2}`                          | `{"a":2,"b":1}` (keys sorted) |
| `"a\nb"` etc.                            | escaped as `"a\nb"`           |

### 3.15 Round-trip stability

For 18 samples, `Parse → Stringify → Parse → Stringify` produces identical serialized output both times (idempotent).

### 3.16 Accessors / type errors

| Operation                                          | Thrown exception  |
|----------------------------------------------------|-------------------|
| `GetAsString()` on an integer                      | `TypeError`       |
| `GetAsInteger()` on a string                       | `TypeError`       |
| `GetAsObject(key)` on an array                     | `TypeError`       |
| `GetAsArray(index)` on an object                   | `TypeError`       |
| `GetAsBool()` on null                              | `TypeError`       |
| accessing a missing key                            | `OutOfRangeError` |
| accessing an out-of-range index                    | `OutOfRangeError` |
| serializing a valueless node (default-constructed) | `TypeError`       |

### 3.17 Value semantics

- Copy construction: deep copy; modifying the copy does not affect the original (no pointer aliasing).
- Move construction / move assignment / copy assignment all work correctly.
- `Reset()` clears the value; `Reset(int/double/string&&/bool)` reassigns (integral/floating go through SFINAE templates
  to avoid `int` ambiguity), and `Reset(static_cast<void*>(nullptr))` sets a `null` node.

### 3.18 Duplicate keys (implementation-defined)

`{"a":1,"a":2}` → `GetAsObject("a")` returns `1` (`std::map::emplace` keeps the first occurrence). Duplicate-key
behavior is implementation-defined (not specified by RFC 8259).

### 3.19 Exception hierarchy

```
ordinaryjson::OrdinaryJsonException   ← base class (std::runtime_error)
├── ParseError        syntax error, exposes byte() offset
├── TypeError         type mismatch / serializing an empty node
└── OutOfRangeError   missing key or index
```

- `ParseError` / `TypeError` / `OutOfRangeError` can all be caught via
  `OrdinaryJsonException`.
- `ParseError::byte()` returns the 0-based byte offset for locating the error.

### 3.22 Numeric overload disambiguation (regression)

The `OrdinaryJsonNode` constructor and `Reset` use `std::enable_if` + type-trait templates for integral/floating
arguments, with a non-template `bool` overload. Verifies `int`/`long`/`float`/`double`/`bool` literals are unambiguous:

| Call                      | Result       |
|---------------------------|--------------|
| `OrdinaryJsonNode(42)`    | integer node |
| `OrdinaryJsonNode(42L)`   | integer node |
| `OrdinaryJsonNode(42.5)`  | double node  |
| `OrdinaryJsonNode(3.14f)` | double node  |
| `OrdinaryJsonNode(true)`  | bool node    |
| `node.Reset(7)`           | integer node |

### 3.23 Real-world business scenarios (key)

Covers common production JSON shapes, validating key fields and round-trip stability:

| Scenario                    | Key checks                                                                                                                  |
|-----------------------------|-----------------------------------------------------------------------------------------------------------------------------|
| REST API paginated response | `code`/`message`, nested `items[]` `id`/`price`/`in_stock`, `total`                                                         |
| User profile                | large `id` (`9007199254740993`, above `2^53`) stored exactly as `int64_t`, `age:null`, `tags[]`, `profile.bio` with newline |
| E-commerce order            | order-id string, CJK name `张三`, `qty` integer, `total` amount, `paid` bool                                                |
| Config file                 | nested `app`/`database`, `ports[]`, `retry.max_attempts`, `password:null`                                                   |
| GeoJSON                     | `type`/`geometry.type`, `coordinates[]` floats, `population` integer                                                        |
| Log entry                   | epoch-millis timestamp (`1700000000000`, `int64_t`), `level`, `context.ip`, `attempts`                                      |
| Multilingual content        | Chinese/Japanese/Korean greetings, emoji (surrogate pair), `translations.zh`                                                |

> Key business boundaries: large IDs, epoch-millis timestamps, money/coordinate
> floats, null values, multilingual UTF-8, emoji surrogate pairs.

### 3.24 More invalid inputs (all throw `ParseError`)

| Category | Examples                                                                                                |
|----------|---------------------------------------------------------------------------------------------------------|
| Numbers  | `00`, `0.`, `--1`, `1-`, `1e++5`, `0x1F`, `0b101`, `1_000`, `1,000`                                     |
| Objects  | `{a:1}` (unquoted key), `{'a':1}`, `{"a"::1}`, `{"a":}`, `{,}`, `{,"a":1}`, `{"a":1,,}`                 |
| Arrays   | `[,]`, `[,1]`, `[1,,2]`                                                                                 |
| Literals | `True`/`TRUE`/`False`/`Null` (wrong case), `truu`                                                       |
| Strings  | `"\uD800\uD800"` (two high surrogates), `"\uDC00\uD800"` (low before high), `"\"` (trailing backslash)  |
| Other    | `[1]x` (junk after value), `\xEF\xBB\xBF{}` (UTF-8 BOM, strictly rejected), leading control char `\x01` |

### 3.25 Container access (iteration / size)

`GetObjectNodeRef()` / `GetArrayNodeRef()` return the underlying `std::map` /
`std::vector` reference:

| Check                                                                    | Result                                                       |
|--------------------------------------------------------------------------|--------------------------------------------------------------|
| `GetObjectNodeRef().size()` / `GetArrayNodeRef().size()`                 | correct element count                                        |
| range-based `for` over object / array                                    | correctly accumulates values                                 |
| `GetArrayNodeRef().push_back(...)` mutates the container                 | works                                                        |
| `GetObjectNodeRef()["key"] = ...` inserts                                | works                                                        |
| container access on a const node                                         | goes through the `const` overload, returns a const reference |
| reading scalars (int/double/bool/string) on a const node                 | works via the `const` overload                               |
| calling `GetObjectNodeRef()` / `GetArrayNodeRef()` on a non-object/array | throws `TypeError`                                           |

## 4. Known Edge Behavior / Limitations

1. **`-0` is normalized to integer `0`**: `Parse("-0")` yields integer `0` (the sign is lost). JSON allows `-0`, but the
   integer path does not preserve negative zero.
2. **Nesting depth limit of 100**: deeper nesting throws `ParseError` (guards against stack overflow).
3. **Number conversion depends on the C++ locale**: `std::stoll`/`std::stod` are affected by `std::locale::global()`;
   this only matters if the global locale is changed to one whose decimal point is not `.` (an extreme edge case).

## 5. Conclusion

All 257 test cases pass, covering basic values, numeric/string/container boundaries, whitespace handling, nesting depth,
invalid-input rejection, serialization and round-trip stability, type-access exceptions, value semantics, numeric
overload disambiguation, the exception hierarchy, container iteration/size access, const-access correctness, real-world
business scenarios (API response, user profile, e-commerce order, config file, GeoJSON, log entry, multilingual
content), and 30+ additional invalid inputs. The library's parsing and serialization behavior conforms to json.org / RFC

8259.
