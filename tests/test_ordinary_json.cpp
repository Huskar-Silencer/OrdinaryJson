// Comprehensive test suite for ordinaryjson.
//
// Build (MSVC):
//   cl /nologo /EHsc /std:c++14 /I src tests/test_ordinary_json.cpp src/ordinary_json.cpp
// Build (GCC/Clang):
//   g++ -std=c++14 -I src tests/test_ordinary_json.cpp src/ordinary_json.cpp -o oj_test

#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>

#include "ordinary_json.hpp"

using namespace ordinaryjson;

static int g_total = 0;
static int g_failed = 0;

static void expect(bool cond, const std::string& name) {
  ++g_total;
  if (cond) {
    std::cout << "  [PASS] " << name << "\n";
  } else {
    ++g_failed;
    std::cout << "  [FAIL] " << name << "\n";
  }
}

static void section(const std::string& title) {
  std::cout << "\n=== " << title << " ===\n";
}

static bool parse_ok(const std::string& s) {
  try {
    Parse(s);
    return true;
  } catch (const ParseError&) {
    return false;
  }
}

static void expect_valid(const std::string& s, const std::string& name) {
  expect(parse_ok(s), name);
}

static void expect_invalid(const std::string& s, const std::string& name) {
  expect(!parse_ok(s), name);
}

static void expect_double(double actual, double expected,
                          const std::string& name) {
  expect(std::fabs(actual - expected) < 1e-9, name);
}

template <typename ExceptionType, typename Fn>
static bool throws(Fn fn) {
  try {
    fn();
    return false;
  } catch (const ExceptionType&) {
    return true;
  }
}

static std::string nested_arrays(size_t depth) {
  return std::string(depth, '[') + "0" + std::string(depth, ']');
}

int main() {
  // -------------------------------------------------------------------------
  section("1. Basic values");
  {
    expect(Parse("null").IsNull(), "null is null");
    expect(Parse("true").GetAsBool() == true, "true is true");
    expect(Parse("false").GetAsBool() == false, "false is false");
  }

  // -------------------------------------------------------------------------
  section("2. Integer boundaries");
  {
    expect(Parse("0").IsInteger() && Parse("0").GetAsInteger() == 0, "0");
    expect(Parse("1").IsInteger() && Parse("1").GetAsInteger() == 1, "1");
    expect(Parse("-1").IsInteger() && Parse("-1").GetAsInteger() == -1, "-1");
    expect(Parse("42").IsInteger() && Parse("42").GetAsInteger() == 42, "42");

    const int64_t i64max = std::numeric_limits<int64_t>::max();
    const int64_t i64min = std::numeric_limits<int64_t>::min();
    std::ostringstream oss_max, oss_min, oss_over, oss_over2;
    oss_max << i64max;
    oss_min << i64min;
    oss_over << static_cast<uint64_t>(i64max) + 1;
    oss_over2 << "18446744073709551616";  // 2^64

    expect(Parse(oss_max.str()).IsInteger() &&
               Parse(oss_max.str()).GetAsInteger() == i64max,
           "INT64_MAX parses exactly");
    expect(Parse(oss_min.str()).IsInteger() &&
               Parse(oss_min.str()).GetAsInteger() == i64min,
           "INT64_MIN parses exactly");
    expect_invalid(oss_over.str(), "INT64_MAX+1 overflows -> ParseError");
    expect_invalid(oss_over2.str(), "2^64 overflows -> ParseError");
    expect_invalid("-9223372036854775809", "below INT64_MIN -> ParseError");
  }

  // -------------------------------------------------------------------------
  section("3. Double / fractional boundaries");
  {
    expect_double(Parse("1.5").GetAsDouble(), 1.5, "1.5");
    expect_double(Parse("-2.5").GetAsDouble(), -2.5, "-2.5");
    expect_double(Parse("0.5").GetAsDouble(), 0.5, "0.5");
    expect_double(Parse("123.456").GetAsDouble(), 123.456, "123.456");
    expect_double(Parse("0.1").GetAsDouble(), 0.1, "0.1");
    expect(Parse("1.5").IsDouble(), "1.5 is stored as double");
    expect(Parse("1.0").IsDouble(), "1.0 is stored as double");
  }

  // -------------------------------------------------------------------------
  section("4. Scientific notation");
  {
    expect_double(Parse("1e5").GetAsDouble(), 100000.0, "1e5");
    expect_double(Parse("1E5").GetAsDouble(), 100000.0, "1E5");
    expect_double(Parse("1e-5").GetAsDouble(), 0.00001, "1e-5");
    expect_double(Parse("2.5e3").GetAsDouble(), 2500.0, "2.5e3");
    expect_double(Parse("-1.5E-2").GetAsDouble(), -0.015, "-1.5E-2");
    expect_double(Parse("1e+5").GetAsDouble(), 100000.0, "1e+5");
  }

  // -------------------------------------------------------------------------
  section("5. String basics");
  {
    expect(Parse(R"("")").GetAsString() == "", "empty string");
    expect(Parse(R"("hello")").GetAsString() == "hello", "plain string");
    expect(Parse(R"("hello world")").GetAsString() == "hello world",
           "string with space");
    expect(Parse(R"("  leading/trailing  ")").GetAsString() ==
               "  leading/trailing  ",
           "leading/trailing spaces preserved");
  }

  // -------------------------------------------------------------------------
  section("6. String escapes");
  {
    expect(Parse(R"("a\nb")").GetAsString() == "a\nb", "\\n -> newline");
    expect(Parse(R"("a\tb")").GetAsString() == "a\tb", "\\t -> tab");
    expect(Parse(R"("a\rb")").GetAsString() == "a\rb", "\\r -> CR");
    expect(Parse(R"("a\bb")").GetAsString() == "a\bb", "\\b -> backspace");
    expect(Parse(R"("a\fb")").GetAsString() == "a\fb", "\\f -> formfeed");
    expect(Parse(R"("a\"b")").GetAsString() == "a\"b", "\\\" -> quote");
    expect(Parse(R"("a\\b")").GetAsString() == "a\\b", "\\\\ -> backslash");
    expect(Parse(R"("a\/b")").GetAsString() == "a/b", "\\/ -> slash");
  }

  // -------------------------------------------------------------------------
  section("7. Unicode escapes");
  {
    expect(Parse(R"("\u0041")").GetAsString() == "A", "\\u0041 -> A");
    expect(Parse(R"("\u4e2d")").GetAsString() == "\xE4\xB8\xAD",
           "\\u4e2d -> U+4E2D (UTF-8)");
    expect(Parse(R"("\ud83d\ude00")").GetAsString() == "\xF0\x9F\x98\x80",
           "surrogate pair -> U+1F600");
    expect(Parse(R"("raw \u00e9 text")").GetAsString() == "raw \xC3\xA9 text",
           "embedded unicode escape");
  }

  // -------------------------------------------------------------------------
  section("8. Invalid strings");
  {
    expect_invalid(R"("a\x")", "invalid escape \\x");
    expect_invalid(R"("\uZZZZ")", "invalid unicode escape \\uZZZZ");
    expect_invalid(R"("\u123")", "truncated unicode escape \\u123");
    expect_invalid(R"("\ud800")", "lone high surrogate");
    expect_invalid(R"("\udc00")", "lone low surrogate");
    expect_invalid(R"("\ud800\u0041")", "high surrogate + non-low");
    expect_invalid("\"unterminated", "unterminated string");
    expect_invalid("\"line\nbreak\"", "raw newline in string");
    expect_invalid("\"ctrl\x01char\"", "raw control char 0x01 in string");
  }

  // -------------------------------------------------------------------------
  section("9. Objects");
  {
    expect(Parse("{}").IsObject(), "empty object");
    expect(Parse("{ }").IsObject(), "empty object with space");
    expect(Parse(R"({"a":1})").GetAsObject("a").GetAsInteger() == 1,
           "single pair");
    expect(Parse(R"({"a":1,"b":2})").GetAsObject("b").GetAsInteger() == 2,
           "multiple pairs");
    expect(Parse(R"({ "a" : 1 , "b" : 2 })").IsObject(),
           "whitespace around tokens");
    expect(Parse(R"({"a":{"b":{"c":3}}})")
               .GetAsObject("a")
               .GetAsObject("b")
               .GetAsObject("c")
               .GetAsInteger() == 3,
           "nested objects");
    expect(Parse(R"({"":1})").GetAsObject("").GetAsInteger() == 1,
           "empty key");
    expect(Parse(R"({"a\nb":1})").IsObject(), "escaped key");
    expect(Parse(R"({"\u4e2d":1})").IsObject(), "unicode key");
  }

  // -------------------------------------------------------------------------
  section("10. Arrays");
  {
    expect(Parse("[]").IsArray(), "empty array");
    expect(Parse("[ ]").IsArray(), "empty array with space");
    expect(Parse("[1]").GetAsArray(0).GetAsInteger() == 1, "single element");
    expect(Parse("[1,2,3]").GetAsArray(2).GetAsInteger() == 3,
           "multiple elements");
    expect(Parse("[1, [2, [3]]]").GetAsArray(1).GetAsArray(1).GetAsArray(0)
               .GetAsInteger() == 3,
           "nested arrays");
    expect(Parse(R"([1,"two",null,true])").GetAsArray(1).GetAsString() ==
               "two",
           "mixed-type array");
    expect(Parse(" [ 1 , 2 , 3 ] ").IsArray(), "whitespace in array");
  }

  // -------------------------------------------------------------------------
  section("11. Whitespace handling");
  {
    expect(Parse("   42").GetAsInteger() == 42, "leading spaces");
    expect(Parse("42   ").GetAsInteger() == 42, "trailing spaces");
    expect(Parse("\t42").GetAsInteger() == 42, "leading tab");
    expect(Parse("\n42").GetAsInteger() == 42, "leading newline");
    expect(Parse("\r\n42").GetAsInteger() == 42, "leading CRLF");
    expect(Parse("  \t\n\r42  ").GetAsInteger() == 42, "all whitespace");
  }

  // -------------------------------------------------------------------------
  section("12. Nesting depth");
  {
    expect_valid(nested_arrays(1), "1 level");
    expect_valid(nested_arrays(50), "50 levels");
    expect_valid(nested_arrays(100), "100 levels (limit)");
    expect_invalid(nested_arrays(101), "101 levels -> ParseError");
    expect_invalid(nested_arrays(500), "500 levels -> ParseError");
  }

  // -------------------------------------------------------------------------
  section("13. Invalid documents");
  {
    expect_invalid("", "empty input");
    expect_invalid("   ", "whitespace-only input");
    expect_invalid("-", "lone minus");
    expect_invalid("01", "leading zero 01");
    expect_invalid("-01", "leading zero -01");
    expect_invalid("1.", "trailing dot 1.");
    expect_invalid("1e", "1e without exponent digits");
    expect_invalid("1e+", "1e+ without exponent digits");
    expect_invalid("+1", "leading plus");
    expect_invalid(".5", "leading dot");
    expect_invalid("1.2.3", "multiple dots");
    expect_invalid("[1,]", "trailing comma in array");
    expect_invalid(R"({"a":1,})", "trailing comma in object");
    expect_invalid(R"({"a" 1})", "missing colon");
    expect_invalid(R"({"a":1 "b":2})", "missing comma");
    expect_invalid("[1 2]", "missing comma in array");
    expect_invalid("[", "unterminated array");
    expect_invalid("{", "unterminated object");
    expect_invalid("tru", "truncated true");
    expect_invalid("fals", "truncated false");
    expect_invalid("nul", "truncated null");
    expect_invalid("truex", "true followed by junk");
    expect_invalid("nullx", "null followed by junk");
    expect_invalid("abc", "garbage input");
    expect_invalid("1 2", "multiple root values");
    expect_invalid("{} {}", "two root objects");
    expect_invalid(R"("a" "b")", "two root strings");
    expect_invalid("'single'", "single-quoted string");
    expect_invalid("/*comment*/", "comment");
    expect_invalid("NaN", "NaN literal");
    expect_invalid("Infinity", "Infinity literal");
  }

  // -------------------------------------------------------------------------
  section("14. Serialization (Stringify)");
  {
    expect(Stringify(Parse("null")) == "null", "null");
    expect(Stringify(Parse("true")) == "true", "true");
    expect(Stringify(Parse("false")) == "false", "false");
    expect(Stringify(Parse("42")) == "42", "integer");
    expect(Stringify(Parse("1.5")) == "1.5", "double 1.5");
    expect(Stringify(Parse("{}")) == "{}", "empty object");
    expect(Stringify(Parse("[]")) == "[]", "empty array");
    expect(Stringify(Parse(R"("hello")")) == "\"hello\"", "string quoted");
    expect(Stringify(Parse(R"([1,"two",null])")) == "[1,\"two\",null]",
           "mixed array");
    expect(Stringify(Parse(R"({"b":1,"a":2})")) == "{\"a\":2,\"b\":1}",
           "object keys sorted");
    expect(Stringify(Parse(R"("a\nb")")) == R"("a\nb")", "newline escaped");
    expect(Stringify(Parse(R"("a\"b")")) == R"("a\"b")", "quote escaped");
    expect(Stringify(Parse(R"("a\\b")")) == R"("a\\b")", "backslash escaped");
    expect(Stringify(Parse(R"("a\tb")")) == R"("a\tb")", "tab escaped");
  }

  // -------------------------------------------------------------------------
  section("15. Round-trip stability");
  {
    const char* samples[] = {
        "null", "true", "false", "0", "42", "-7", "1.5", "0.125", "1e5",
        R"("")", R"("hello")", R"("a\nb")", R"("\u4e2d")", "[]", "[1,2,3]",
        "{}", R"({"a":1})", R"({"a":[1,{"b":"x"}],"c":null})",
    };
    for (const char* s : samples) {
      std::string out1, out2;
      try {
        out1 = Stringify(Parse(s));
        out2 = Stringify(Parse(out1));
      } catch (...) {
        expect(false, std::string("round-trip threw for: ") + s);
        continue;
      }
      expect(out1 == out2, std::string("round-trip stable: ") + s);
    }
  }

  // -------------------------------------------------------------------------
  section("16. Accessors / type errors");
  {
    expect(throws<TypeError>([] { Parse("42").GetAsString(); }),
           "GetAsString on integer -> TypeError");
    expect(throws<TypeError>([] { Parse(R"("x")").GetAsInteger(); }),
           "GetAsInteger on string -> TypeError");
    expect(throws<TypeError>([] { Parse("[]").GetAsObject("k"); }),
           "GetAsObject on array -> TypeError");
    expect(throws<TypeError>([] { Parse("{}").GetAsArray(0); }),
           "GetAsArray on object -> TypeError");
    expect(throws<TypeError>([] { Parse("null").GetAsBool(); }),
           "GetAsBool on null -> TypeError");
    expect(throws<OutOfRangeError>([] { Parse("{}").GetAsObject("nope"); }),
           "missing key -> OutOfRangeError");
    expect(throws<OutOfRangeError>([] { Parse("[1]").GetAsArray(1); }),
           "index out of range -> OutOfRangeError");
    expect(throws<TypeError>([] { OrdinaryJsonNode().Stringify(); }),
           "Stringify of empty node -> TypeError");
  }

  // -------------------------------------------------------------------------
  section("17. Value semantics (copy / move / reset)");
  {
    OrdinaryJsonNode a = Parse(R"({"k":[1,2,3]})");
    OrdinaryJsonNode b = a;  // deep copy
    expect(b.Stringify() == a.Stringify(), "copy constructor deep copies");
    b.GetAsObject("k").GetAsArray(0).GetAsInteger() = 999;
    expect(a.GetAsObject("k").GetAsArray(0).GetAsInteger() == 1,
           "copy is independent (no aliasing)");

    OrdinaryJsonNode c = Parse("true");
    OrdinaryJsonNode d = std::move(c);
    expect(d.GetAsBool() == true, "move constructor");

    OrdinaryJsonNode e;
    e = a;
    expect(e.Stringify() == a.Stringify(), "copy assignment");
    e = std::move(b);
    expect(e.IsObject(), "move assignment");

    e.Reset();
    expect(!e.HasValue(), "Reset() clears value");
    e.Reset(42);  // plain int, no longer ambiguous
    expect(e.IsInteger() && e.GetAsInteger() == 42, "Reset(int) -> integer");
    e.Reset(3.14);
    expect(e.IsDouble() && std::fabs(e.GetAsDouble() - 3.14) < 1e-12,
           "Reset(double) -> double");
    e.Reset(std::string("hi"));
    expect(e.IsString() && e.GetAsString() == "hi", "Reset(string&&)");
    e.Reset(false);
    expect(e.IsBoolean() && e.GetAsBool() == false, "Reset(bool)");
  }

  // -------------------------------------------------------------------------
  section("22. Numeric overload disambiguation (regression for Reset(int))");
  {
    OrdinaryJsonNode a(42);  // int literal -> integer node
    expect(a.IsInteger() && a.GetAsInteger() == 42, "OrdinaryJsonNode(42) -> integer");

    OrdinaryJsonNode b(42L);  // long literal -> integer node
    expect(b.IsInteger() && b.GetAsInteger() == 42, "OrdinaryJsonNode(42L) -> integer");

    OrdinaryJsonNode c(42.5);  // double literal -> double node
    expect(c.IsDouble() && std::fabs(c.GetAsDouble() - 42.5) < 1e-12,
           "OrdinaryJsonNode(42.5) -> double");

    OrdinaryJsonNode d(3.14f);  // float literal -> double node
    expect(d.IsDouble() && std::fabs(d.GetAsDouble() - 3.14) < 1e-2,
           "OrdinaryJsonNode(3.14f) -> double");

    OrdinaryJsonNode e(true);  // bool literal -> bool node
    expect(e.IsBoolean() && e.GetAsBool() == true,
           "OrdinaryJsonNode(true) -> bool");

    OrdinaryJsonNode f;  // default -> unknown
    f.Reset(7);           // int literal -> integer
    expect(f.IsInteger() && f.GetAsInteger() == 7, "Reset(7) -> integer");
  }

  // -------------------------------------------------------------------------
  section("18. Duplicate keys (implementation-defined)");
  {
    // std::map::emplace keeps the first occurrence.
    expect(Parse(R"({"a":1,"a":2})").GetAsObject("a").GetAsInteger() == 1,
           "duplicate key keeps first value");
  }

  // -------------------------------------------------------------------------
  section("19. Exception hierarchy");
  {
    expect(throws<OrdinaryJsonException>([] { Parse("["); }),
           "ParseError is OrdinaryJsonException");
    expect(throws<OrdinaryJsonException>([] { Parse("1").GetAsString(); }),
           "TypeError is OrdinaryJsonException");
    expect(throws<OrdinaryJsonException>([] { Parse("{}").GetAsObject("x"); }),
           "OutOfRangeError is OrdinaryJsonException");

    size_t byte = 0;
    try {
      Parse(R"({"a": })");
    } catch (const ParseError& e) {
      byte = e.byte();
    }
    expect(byte > 0, "ParseError::byte() reports offset");
  }

  // -------------------------------------------------------------------------
  section("23. Real-world business scenarios");
  {
    // REST API response (paginated list)
    const std::string api_response = R"JSON({
      "code": 0,
      "message": "ok",
      "data": {
        "items": [
          {"id": 1001, "title": "First", "price": 19.99, "in_stock": true},
          {"id": 1002, "title": "Second", "price": 29.5, "in_stock": false}
        ],
        "page": 1,
        "page_size": 10,
        "total": 2
      }
    })JSON";
    {
      auto n = Parse(api_response);
      expect(n.GetAsObject("code").GetAsInteger() == 0, "api: code");
      expect(n.GetAsObject("message").GetAsString() == "ok", "api: message");
      auto& items = n.GetAsObject("data").GetAsObject("items");
      expect(items.GetAsArray(0).GetAsObject("id").GetAsInteger() == 1001,
             "api: items[0].id");
      expect_double(items.GetAsArray(0).GetAsObject("price").GetAsDouble(),
                    19.99, "api: items[0].price");
      expect(items.GetAsArray(0).GetAsObject("in_stock").GetAsBool() == true,
             "api: items[0].in_stock");
      expect(items.GetAsArray(1).GetAsObject("in_stock").GetAsBool() == false,
             "api: items[1].in_stock");
      expect(n.GetAsObject("data").GetAsObject("total").GetAsInteger() == 2,
             "api: data.total");
    }

    // User profile (large int64 id, null field, nested profile)
    const std::string user_profile = R"JSON({
      "id": 9007199254740993,
      "username": "alice",
      "email": "alice@example.com",
      "is_verified": true,
      "age": null,
      "tags": ["admin", "beta"],
      "profile": {"bio": "hello\nworld", "website": "https://example.com"}
    })JSON";
    {
      auto n = Parse(user_profile);
      expect(n.GetAsObject("id").GetAsInteger() == 9007199254740993LL,
             "user: large id preserved exactly (int64)");
      expect(n.GetAsObject("username").GetAsString() == "alice",
             "user: username");
      expect(n.GetAsObject("age").IsNull(), "user: age is null");
      expect(n.GetAsObject("tags").GetAsArray(1).GetAsString() == "beta",
             "user: tags[1]");
      expect(n.GetAsObject("profile").GetAsObject("bio").GetAsString() ==
                 "hello\nworld",
             "user: profile.bio with newline");
    }

    // E-commerce order (CJK name, money, quantities)
    const std::string order = R"JSON({
      "order_id": "ORD-2024-0001",
      "customer": {"name": "\u5f20\u4e09", "phone": "13800138000"},
      "items": [
        {"sku": "A100", "qty": 2, "unit_price": 12.5, "discount": 0.1},
        {"sku": "B200", "qty": 1, "unit_price": 99.0, "discount": 0.0}
      ],
      "currency": "CNY",
      "total": 123.5,
      "paid": true
    })JSON";
    {
      auto n = Parse(order);
      expect(n.GetAsObject("order_id").GetAsString() == "ORD-2024-0001",
             "order: order_id");
      expect(n.GetAsObject("customer").GetAsObject("name").GetAsString() ==
                 "\xE5\xBC\xA0\xE4\xB8\x89",
             "order: CJK customer name");
      expect(n.GetAsObject("items").GetAsArray(0).GetAsObject("qty")
                 .GetAsInteger() == 2,
             "order: items[0].qty");
      expect_double(n.GetAsObject("total").GetAsDouble(), 123.5,
                    "order: total");
      expect(n.GetAsObject("paid").GetAsBool() == true, "order: paid");
    }

    // Config file (nested settings, ports array, null password)
    const std::string config = R"JSON({
      "app": {
        "name": "myservice",
        "version": "1.2.3",
        "debug": false,
        "ports": [8080, 8081],
        "retry": {"max_attempts": 3, "backoff_ms": 500}
      },
      "database": {
        "host": "localhost",
        "port": 5432,
        "user": "admin",
        "password": null
      }
    })JSON";
    {
      auto n = Parse(config);
      expect(n.GetAsObject("app").GetAsObject("version").GetAsString() ==
                 "1.2.3",
             "config: app.version");
      expect(n.GetAsObject("app").GetAsObject("ports").GetAsArray(1)
                 .GetAsInteger() == 8081,
             "config: app.ports[1]");
      expect(n.GetAsObject("app").GetAsObject("retry").GetAsObject(
                 "max_attempts").GetAsInteger() == 3,
             "config: app.retry.max_attempts");
      expect(n.GetAsObject("database").GetAsObject("port").GetAsInteger() ==
                 5432,
             "config: database.port");
      expect(n.GetAsObject("database").GetAsObject("password").IsNull(),
             "config: database.password is null");
    }

    // GeoJSON feature
    const std::string geojson = R"JSON({
      "type": "Feature",
      "geometry": {"type": "Point", "coordinates": [116.397, 39.909]},
      "properties": {"name": "Beijing", "population": 21540000}
    })JSON";
    {
      auto n = Parse(geojson);
      expect(n.GetAsObject("type").GetAsString() == "Feature",
             "geojson: type");
      expect(n.GetAsObject("geometry").GetAsObject("type").GetAsString() ==
                 "Point",
             "geojson: geometry.type");
      expect_double(n.GetAsObject("geometry").GetAsObject("coordinates")
                        .GetAsArray(0).GetAsDouble(),
                    116.397, "geojson: coordinates[0]");
      expect(n.GetAsObject("properties").GetAsObject("population")
                 .GetAsInteger() == 21540000,
             "geojson: population");
    }

    // Log entry (epoch millis, request context)
    const std::string log_entry = R"JSON({
      "timestamp": 1700000000000,
      "level": "error",
      "logger": "auth.service",
      "message": "failed to authenticate user 'alice'",
      "context": {"ip": "192.168.1.1", "request_id": "req-123", "attempts": 3}
    })JSON";
    {
      auto n = Parse(log_entry);
      expect(n.GetAsObject("timestamp").GetAsInteger() == 1700000000000LL,
             "log: epoch millis timestamp (int64)");
      expect(n.GetAsObject("level").GetAsString() == "error", "log: level");
      expect(n.GetAsObject("context").GetAsObject("ip").GetAsString() ==
                 "192.168.1.1",
             "log: context.ip");
      expect(n.GetAsObject("context").GetAsObject("attempts").GetAsInteger() ==
                 3,
             "log: context.attempts");
    }

    // Multilingual content (CJK + Japanese + Korean + emoji)
    const std::string multilingual = R"JSON({
      "greetings": ["hello", "\u4f60\u597d", "\u3053\u3093\u306b\u3061\u306f", "\uc548\ub155\ud558\uc138\uc694", "\ud83d\ude00"],
      "translations": {"en": "Good morning", "zh": "\u65e9\u4e0a\u597d"}
    })JSON";
    {
      auto n = Parse(multilingual);
      expect(n.GetAsObject("greetings").GetAsArray(1).GetAsString() ==
                 "\xE4\xBD\xA0\xE5\xA5\xBD",
             "i18n: greetings[1] = \xE4\xBD\xA0\xE5\xA5\xBD");
      expect(n.GetAsObject("greetings").GetAsArray(2).GetAsString() ==
                 "\xE3\x81\x93\xE3\x82\x93\xE3\x81\xAB\xE3\x81\xA1\xE3\x81\xAF",
             "i18n: greetings[2] = Japanese");
      expect(n.GetAsObject("greetings").GetAsArray(4).GetAsString() ==
                 "\xF0\x9F\x98\x80",
             "i18n: greetings[4] = emoji");
      expect(n.GetAsObject("translations").GetAsObject("zh").GetAsString() ==
                 "\xE6\x97\xA9\xE4\xB8\x8A\xE5\xA5\xBD",
             "i18n: translations.zh");
    }

    // Round-trip stability across all realistic docs.
    const std::string* docs[] = {&api_response, &user_profile, &order,
                                 &config,       &geojson,      &log_entry,
                                 &multilingual};
    const char* doc_names[] = {"api_response", "user_profile", "order",
                               "config",       "geojson",      "log_entry",
                               "multilingual"};
    for (size_t i = 0; i < sizeof(docs) / sizeof(docs[0]); ++i) {
      std::string out1 = Stringify(Parse(*docs[i]));
      std::string out2 = Stringify(Parse(out1));
      expect(out1 == out2,
             std::string("round-trip stable: ") + doc_names[i]);
    }
  }

  // -------------------------------------------------------------------------
  section("24. More invalid inputs");
  {
    expect_invalid("00", "leading zero 00");
    expect_invalid("0.", "trailing dot 0.");
    expect_invalid("--1", "double minus");
    expect_invalid("1-", "trailing minus");
    expect_invalid("1e++5", "double plus in exponent");
    expect_invalid("0x1F", "hex literal");
    expect_invalid("0b101", "binary literal");
    expect_invalid("1_000", "underscore separator");
    expect_invalid("1,000", "comma separator");
    expect_invalid("{a:1}", "unquoted key");
    expect_invalid("{'a':1}", "single-quoted key");
    expect_invalid(R"({"a"::1})", "double colon");
    expect_invalid(R"({"a":})", "missing value");
    expect_invalid("{,}", "empty object with comma");
    expect_invalid(R"({,"a":1})", "leading comma in object");
    expect_invalid(R"({"a":1,,})", "double comma in object");
    expect_invalid("[,]", "array with only comma");
    expect_invalid("[,1]", "leading comma in array");
    expect_invalid("[1,,2]", "double comma in array");
    expect_invalid("True", "capitalized True");
    expect_invalid("TRUE", "uppercase TRUE");
    expect_invalid("False", "capitalized False");
    expect_invalid("Null", "capitalized Null");
    expect_invalid("truu", "misspelled true");
    expect_invalid("[1]x", "trailing junk after array");
    expect_invalid(R"("\uD800\uD800")", "two high surrogates");
    expect_invalid(R"("\uDC00\uD800")", "low surrogate before high");
    expect_invalid(R"("\)", "backslash at end of string");
    expect_invalid("\xEF\xBB\xBF{}", "UTF-8 BOM prefix (strict reject)");
    expect_invalid("\x01", "leading control char");
  }

  // -------------------------------------------------------------------------
  section("25. Container access (iteration / size)");
  {
    auto obj = Parse(R"({"a":1,"b":2,"c":3})");
    expect(obj.GetObjectNodeRef().size() == 3, "object size");
    int64_t sum = 0;
    for (auto& kv : obj.GetObjectNodeRef()) {
      sum += kv.second.GetAsInteger();
    }
    expect(sum == 6, "iterate object values");

    auto arr = Parse("[10,20,30]");
    expect(arr.GetArrayNodeRef().size() == 3, "array size");
    int64_t total = 0;
    for (auto& e : arr.GetArrayNodeRef()) {
      total += e.GetAsInteger();
    }
    expect(total == 60, "iterate array elements");

    // mutate the underlying container directly
    arr.GetArrayNodeRef().push_back(OrdinaryJsonNode(40));
    expect(arr.GetArrayNodeRef().size() == 4, "push_back into array");
    expect(arr.GetAsArray(3).GetAsInteger() == 40, "appended element readable");

    obj.GetObjectNodeRef()["d"] = OrdinaryJsonNode(4);
    expect(obj.GetObjectNodeRef().size() == 4, "operator[] insert into object");
    expect(obj.GetAsObject("d").GetAsInteger() == 4, "inserted object value");

    // const access
    const auto& cobj = obj;
    expect(cobj.GetObjectNodeRef().size() == 4, "const object size");
    const auto& carr = arr;
    expect(carr.GetArrayNodeRef().size() == 4, "const array size");

    // const read of scalar values via const overloads
    const auto& cnode = Parse(R"({"i":1,"d":2.5,"b":true,"s":"x"})");
    expect(cnode.GetAsObject("i").GetAsInteger() == 1, "const read integer");
    expect(cnode.GetAsObject("d").GetAsDouble() == 2.5, "const read double");
    expect(cnode.GetAsObject("b").GetAsBool() == true, "const read bool");
    expect(cnode.GetAsObject("s").GetAsString() == "x", "const read string");

    // type mismatch
    expect(throws<TypeError>([] { Parse("42").GetObjectNodeRef(); }),
           "GetObjectNodeRef() on integer -> TypeError");
    expect(throws<TypeError>([] { Parse("42").GetArrayNodeRef(); }),
           "GetArrayNodeRef() on integer -> TypeError");
  }

  // -------------------------------------------------------------------------
  std::cout << "\n==============================================\n";
  std::cout << "Total: " << g_total << ", Passed: " << (g_total - g_failed)
            << ", Failed: " << g_failed << "\n";
  std::cout << (g_failed == 0 ? "ALL TESTS PASSED" : "SOME TESTS FAILED")
            << "\n";
  return g_failed == 0 ? 0 : 1;
}
