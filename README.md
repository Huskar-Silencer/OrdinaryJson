# OrdinaryJson
![888](https://github.com/user-attachments/assets/c31c4edc-5a45-4a5d-a332-12ea39e81316)

A lightweight C++ JSON library for parsing and serializing JSON, built on
standard-library containers (`std::map`, `std::vector`, `std::string`). No
external dependencies, C++14 or later.

## Getting started

The library consists of two files:

- `src/ordinary_json.hpp` — the header
- `src/ordinary_json.cpp` — the implementation

Copy both into your project, add their directory to the include path, and
compile `ordinary_json.cpp` together with your sources. There are no external
dependencies beyond the C++ standard library (C++14 or later).

```c++
#include "ordinary_json.hpp"

// Everything lives in the `ordinaryjson` namespace.
```

### Compiling from the command line

```bash
# MSVC (from a Developer Command Prompt)
cl /nologo /EHsc /std:c++14 /W4 /O2 /I src your_program.cpp src/ordinary_json.cpp
your_program.exe

# GCC / Clang
g++ -std=c++14 -O2 -I src your_program.cpp src/ordinary_json.cpp -o your_program
```

`/W4` (MSVC) / `-Wall -Wextra` (GCC, Clang) enable all warnings; `/O2` /
`-O2` enable optimization.

Compile and run the test suite (257 cases):

```bash
# MSVC
cl /nologo /EHsc /std:c++14 /W4 /O2 /I src tests/test_ordinary_json.cpp src/ordinary_json.cpp
test_ordinary_json.exe   # -> "Total: 257, Passed: 257, Failed: 0"

# GCC / Clang
g++ -std=c++14 -Wall -Wextra -O2 -I src tests/test_ordinary_json.cpp src/ordinary_json.cpp -o oj_test
./oj_test                # -> "Total: 257, Passed: 257, Failed: 0"
                         # (Windows PowerShell: .\oj_test)
```

Compile and run the benchmark:

```bash
# MSVC
cl /nologo /EHsc /std:c++14 /O2 /I src benchmark/benchmark.cpp src/ordinary_json.cpp
benchmark.exe

# GCC / Clang
g++ -std=c++14 -O2 -I src benchmark/benchmark.cpp src/ordinary_json.cpp -o oj_bench
./oj_bench               # (Windows PowerShell: .\oj_bench)
```

### Building with CMake

`CMakeLists.txt` defines two targets:

- `OrdinaryJson` — the example program (`src/main.cpp`)
- `ordinaryjson_tests` — the test suite (`tests/test_ordinary_json.cpp`),
  registered with CTest

```bash
# MSVC (Visual Studio on Windows)
cmake -S . -B build                          # default generator (Visual Studio on Windows)
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure   # run the tests
build\Release\OrdinaryJson.exe                            # run the example
```

```bash
# GCC / Clang (Linux, macOS, or MinGW on Windows)
cmake -S . -B build -G "Ninja"                          # or "Unix Makefiles" / "MinGW Makefiles"
cmake --build build
ctest --test-dir build --output-on-failure              # run the tests
./build/OrdinaryJson                                    # run the example
                                                       # (Windows PowerShell: .\build\Release\OrdinaryJson.exe)
```

> On Windows, if the default generator does not match your Visual Studio
> version, pass `-G` explicitly, e.g. `-G "Visual Studio 18 2026" -A x64`.

### Using CMake in your own project

The two sources are plain C++ — just add them to your target:

```cmake
add_executable(MyApp main.cpp src/ordinary_json.cpp)
target_include_directories(MyApp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_features(MyApp PRIVATE cxx_std_14)
```

## Usage

### Parsing (deserialization)

`Parse` takes a JSON string and returns a fully materialized `OrdinaryJsonNode`.
It throws `ordinaryjson::ParseError` when the input is not valid JSON.

```c++
std::string json = R"(
  {"name":"OrdinaryJson","version":1,"tags":["json","parser"],"active":true}
)";

ordinaryjson::OrdinaryJsonNode node = ordinaryjson::Parse(json);
```

Read JSON from a file or any `std::istream` by reading it into a string first:

```c++
#include <fstream>
#include <iterator>

std::ifstream in("config.json");
std::string content((std::istreambuf_iterator<char>(in)),
                    std::istreambuf_iterator<char>());
ordinaryjson::OrdinaryJsonNode config = ordinaryjson::Parse(content);
```

On invalid input, `ParseError` carries the message and the 0-based byte offset
where parsing stopped:

```c++
try {
  auto n = ordinaryjson::Parse(R"({"a": })");
} catch (const ordinaryjson::ParseError& e) {
  std::cerr << e.what() << "\n";   // parse error at byte N: ...
  std::cerr << "at byte " << e.byte() << "\n";
}
```

Empty / whitespace-only input, trailing garbage, or any input that is not a
single JSON value throws `ParseError`. There is no "null on failure" mode.

### Node types and type checking

A node holds exactly one of the following value types:

| Type | Underlying storage |
| --- | --- |
| `object` | `std::map<std::string, OrdinaryJsonNode>` |
| `array` | `std::vector<OrdinaryJsonNode>` |
| `string` | `std::string` |
| `integer` | `int64_t` |
| `double` | `double` (fractional or scientific notation) |
| `bool` | `bool` |
| `null` | JSON `null` |
| *(unknown)* | a default-constructed node with no value |

Check the type with the `Is*` predicates, or get the name as a string:

```c++
if (node.IsObject())   { /* ... */ }
if (node.IsArray())    { /* ... */ }
if (node.IsString())   { /* ... */ }
if (node.IsInteger())  { /* ... */ }
if (node.IsDouble())   { /* ... */ }
if (node.IsBoolean())  { /* ... */ }
if (node.IsNull())     { /* ... */ }
if (node.HasValue())   { /* false for a default-constructed node */ }

std::string t = node.GetValueTypeToString();  // "object", "array", "integer", ...
```

### Accessing values

Use the matching `GetAs*` accessor. Accessors throw `TypeError` when the node
has a different type, and `OutOfRangeError` when a key or index does not exist.

```c++
std::string name    = node.GetAsObject("name").GetAsString();
int64_t    version  = node.GetAsObject("version").GetAsInteger();
double     rating   = node.GetAsObject("rating").GetAsDouble();
bool       active   = node.GetAsObject("active").GetAsBool();
std::string tag     = node.GetAsObject("tags").GetAsArray(0).GetAsString();
```

| Accessor | Returns (non-const / const) |
| --- | --- |
| `GetObjectNodeRef()` | `JsonObjectType&` / `const JsonObjectType&` |
| `GetAsObject(key)` | `OrdinaryJsonNode&` / `const OrdinaryJsonNode&` |
| `GetArrayNodeRef()` | `JsonArrayType&` / `const JsonArrayType&` |
| `GetAsArray(index)` | `OrdinaryJsonNode&` / `const OrdinaryJsonNode&` |
| `GetAsString()` | `std::string&` / `const std::string&` |
| `GetAsInteger()` | `int64_t&` / `const int64_t&` |
| `GetAsDouble()` | `double&` / `const double&` |
| `GetAsBool()` | `bool&` / `const bool&` |

Every accessor has a `const` overload: on a `const` node they return a const
reference (read-only); on a non-const node they return a mutable reference.

Accessors chain to descend into nested structures:

```c++
int first_issue = node.GetAsObject("metrics")
                      .GetAsObject("issues")
                      .GetAsArray(0)
                      .GetAsInteger();
```

### Numbers

Integers are stored as `int64_t`; fractional or scientific values as `double`.
Use `IsInteger()` / `IsDouble()` to tell them apart, then read with the matching
accessor:

```c++
if (node.IsInteger()) {
  int64_t n = node.GetAsInteger();
} else if (node.IsDouble()) {
  double x = node.GetAsDouble();
}
```

Integer literals that overflow `int64_t` (e.g. `9223372036854775808`) throw
`ParseError`. `double` values are serialized with enough digits to round-trip
losslessly.

### Strings

Escape sequences are decoded on parse (`\n`, `\t`, `\"`, `\\`, `\uXXXX`,
surrogate pairs, ...), so `GetAsString()` returns the actual characters:

```c++
ordinaryjson::Parse(R"("a\nb")").GetAsString();          // "a\nb" (real newline)
ordinaryjson::Parse(R"("\u4e2d")").GetAsString();        // "中"
ordinaryjson::Parse(R"("\ud83d\ude00")").GetAsString();  // "😀"
```

### Iterating over containers

`GetObjectNodeRef()` / `GetArrayNodeRef()` return the underlying
`std::map` / `std::vector`, so you can iterate, query the size, and mutate:

```c++
for (auto& kv : node.GetObjectNodeRef()) {
  std::cout << kv.first << " -> " << kv.second.GetValueTypeToString() << "\n";
}
std::cout << "fields: " << node.GetObjectNodeRef().size() << "\n";

for (auto& e : node.GetAsObject("tags").GetArrayNodeRef()) {
  std::cout << e.GetAsString() << "\n";
}
```

### Modifying values in place

`GetAs*` accessors return references, so values can be updated directly:

```c++
node.GetAsObject("version").GetAsInteger() = 2;
node.GetAsObject("name").GetAsString() = "NewName";
```

Add or remove elements through the container reference:

```c++
node.GetAsObject("tags").GetArrayNodeRef().push_back(OrdinaryJsonNode(std::string("new")));
node.GetObjectNodeRef()["note"] = OrdinaryJsonNode(std::string("hello"));
node.GetObjectNodeRef().erase("note");
```

### Building values

Construct a node with the matching constructor (all value constructors are
`explicit`), or reuse a variable with `Reset(...)`:

```c++
OrdinaryJsonNode a(std::string("a string"));  // string
OrdinaryJsonNode b(42);                       // integer
OrdinaryJsonNode c(4.5);                      // double
OrdinaryJsonNode d(true);                     // bool
OrdinaryJsonNode e = ordinaryjson::Parse("null");  // null

OrdinaryJsonNode node;
node.Reset(std::string("reused"));  // clears and reassigns
node.Reset(42);
```

Build an object or array from the underlying containers:

```c++
OrdinaryJsonNode obj;
obj.Reset(OrdinaryJsonNode::JsonObjectType{
    {"name",   OrdinaryJsonNode(std::string("alice"))},
    {"age",    OrdinaryJsonNode(30)},
    {"score",  OrdinaryJsonNode(4.5)},
    {"active", OrdinaryJsonNode(true)},
});

OrdinaryJsonNode arr;
arr.Reset(OrdinaryJsonNode::JsonArrayType{
    OrdinaryJsonNode(1), OrdinaryJsonNode(2), OrdinaryJsonNode(3)});
```

> **`const char*` becomes `bool`**: there is no `const char*` constructor, so
> `OrdinaryJsonNode("hello")` resolves to the `bool` overload (creating `true`).
> Always use `std::string(...)` for string nodes. Constructing `null` manually
> uses the `void*` constructor,
> e.g. `OrdinaryJsonNode(static_cast<void*>(nullptr))`.

### Serializing (Stringify)

```c++
std::string out = node.Stringify();              // member function
std::string out = ordinaryjson::Stringify(node); // free function
```

The output is compact JSON: object keys are emitted in sorted order
(`std::map`), strings are escaped, and `double` values round-trip losslessly.

### Error handling

All exceptions derive from `ordinaryjson::OrdinaryJsonException`:

| Exception | Thrown when |
| --- | --- |
| `ParseError` | the input is not valid JSON (has `byte()` offset) |
| `TypeError` | a value is accessed with an incompatible type |
| `OutOfRangeError` | a requested key or index does not exist |

```c++
try {
  auto node = ordinaryjson::Parse(json);
  int64_t v = node.GetAsObject("version").GetAsInteger();
} catch (const ordinaryjson::ParseError& e) {
  std::cerr << "invalid JSON at byte " << e.byte() << ": " << e.what() << "\n";
} catch (const ordinaryjson::TypeError& e) {
  std::cerr << "type error: " << e.what() << "\n";
} catch (const ordinaryjson::OrdinaryJsonException& e) {
  std::cerr << e.what() << "\n";
}
```

### Complete example

Parse a config, read and modify fields, iterate, and serialize back:

```c++
#include <iostream>
#include "ordinary_json.hpp"

int main() {
  std::string json = R"({
    "app": {"name": "myservice", "debug": false, "ports": [8080, 8081]}
  })";

  ordinaryjson::OrdinaryJsonNode node = ordinaryjson::Parse(json);

  // read
  std::cout << node.GetAsObject("app").GetAsObject("name").GetAsString() << "\n";

  // iterate
  for (auto& kv : node.GetAsObject("app").GetObjectNodeRef()) {
    std::cout << "  " << kv.first << " : "
              << kv.second.GetValueTypeToString() << "\n";
  }

  // modify
  node.GetAsObject("app").GetAsObject("debug").GetAsBool() = true;
  node.GetAsObject("app").GetAsObject("ports").GetArrayNodeRef().push_back(
      ordinaryjson::OrdinaryJsonNode(9090));

  // serialize
  std::cout << node.Stringify() << "\n";
  return 0;
}
```

Output:

```
myservice
  debug : bool
  name : string
  ports : array
{"app":{"debug":true,"name":"myservice","ports":[8080,8081,9090]}}
```

## Notes and limitations

- **`const char*` becomes `bool`**: there is no `const char*` constructor, so
  `OrdinaryJsonNode("literal")` resolves to the `bool` overload. Use
  `std::string("literal")` for string nodes. (Parsing via `Parse` is unaffected.)
- **Nesting depth**: parsing is limited to 100 levels of nesting to guard
  against stack overflow (`ParseError` beyond that).
- **`-0` is normalized to integer `0`** (the integer path does not preserve the
  negative zero sign).
- **Number conversion uses the C++ locale**: `std::stoll`/`std::stod` are
  affected by `std::locale::global()`; this only matters if the global locale is
  changed to one whose decimal point is not `.`.

## Testing

A comprehensive test suite lives in `tests/`, with full documentation in
[`docs/TESTING.md`](docs/TESTING.md).

- **257 test cases, all passing**, covering:
  - basic values, integer/double boundaries (incl. `INT64_MAX`/`INT64_MIN` and overflow)
  - string escapes and Unicode (surrogate pairs)
  - objects/arrays, whitespace handling, nesting-depth limit (100)
  - 60+ invalid-input rejection cases
  - real-world business scenarios (API response, user profile, e-commerce order,
    config file, GeoJSON, log entry, multilingual content)
  - serialization round-trip stability
  - type accessors and the exception hierarchy (`ParseError` / `TypeError` / `OutOfRangeError`)
  - value semantics (copy/move/reset)

Build and run:

```bash
# MSVC command line
cl /nologo /EHsc /std:c++14 /W4 /O2 /I src tests/test_ordinary_json.cpp src/ordinary_json.cpp
test_ordinary_json.exe

# GCC / Clang
g++ -std=c++14 -Wall -Wextra -O2 -I src tests/test_ordinary_json.cpp src/ordinary_json.cpp -o oj_test
./oj_test                # (Windows PowerShell: .\oj_test)

# or via CMake + CTest (see "Building with CMake" above)
ctest --test-dir build -C Release --output-on-failure
```

## Benchmark

`benchmark/benchmark.cpp` measures parse (deserialization) and stringify
(serialization) throughput across several document shapes. Numbers below are
indicative (MSVC `/O2`, x64, single run; auto-tuned iteration count, reported
as µs per operation).

| Document | size | Parse | Stringify |
| --- | --- | --- | --- |
| tiny `{"a":1}` | 7 B | 0.21 µs | 0.42 µs |
| flat object (100 fields) | 2.4 KB | 18.6 µs | 43.1 µs |
| nested object | 238 B | 2.22 µs | 5.16 µs |
| array of 1000 ints | 3.9 KB | 67.7 µs | 165 µs |
| array of 1000 strings | 8.0 KB | 90.7 µs | 138 µs |
| array of 200 unicode strings | 3.8 KB | 27.1 µs | 66.8 µs |
| realistic (50 users) | 3.5 KB | 49.1 µs | 99.1 µs |
| 100-level nested arrays | 201 B | 11.4 µs | 5.06 µs |

Build and run (release):

```bash
# MSVC
cl /nologo /O2 /EHsc /std:c++14 /I src benchmark/benchmark.cpp src/ordinary_json.cpp

# GCC / Clang
g++ -std=c++14 -O2 -I src benchmark/benchmark.cpp src/ordinary_json.cpp -o oj_bench
./oj_bench               # (Windows PowerShell: .\oj_bench)
```
