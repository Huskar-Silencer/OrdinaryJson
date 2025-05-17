// Parse (deserialization) and stringify (serialization) benchmark. Each
// measurement auto-tunes its iteration count to accumulate at least ~100 ms of
// work, then reports time per operation and throughput.
//
// Build (MSVC, release):
//   cl /nologo /O2 /EHsc /std:c++14 /I src benchmark/benchmark.cpp src/ordinary_json.cpp
// Build (GCC/Clang, release):
//   g++ -O3 -std=c++14 -I src benchmark/benchmark.cpp src/ordinary_json.cpp -o oj_bench

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

#include "ordinary_json.hpp"

namespace {

using Clock = std::chrono::steady_clock;

uint64_t g_sink = 0;

double Measure(const std::function<void()>& fn) {
  for (int i = 0; i < 200; ++i) fn();  // warm-up
  int iters = 100;
  double elapsed_ms = 0.0;
  for (;;) {
    const auto start = Clock::now();
    for (int i = 0; i < iters; ++i) fn();
    const auto end = Clock::now();
    elapsed_ms =
        std::chrono::duration<double, std::milli>(end - start).count();
    if (elapsed_ms >= 100.0 || iters >= 1000000) break;
    iters *= 4;
  }
  return elapsed_ms / iters;
}

double ThroughputMBps(size_t bytes, double ms_per_op) {
  if (ms_per_op <= 0.0) return 0.0;
  return bytes / (ms_per_op * 1000.0);
}

std::string BuildFlatDocument(int fields) {
  std::string doc = "{";
  char buf[64];
  for (int i = 0; i < fields; ++i) {
    if (i > 0) doc += ',';
    std::snprintf(buf, sizeof(buf), "\"field_%03d\":\"value_%03d\"", i, i);
    doc += buf;
  }
  doc += '}';
  return doc;
}

std::string BuildIntArray(int count) {
  std::string doc = "[";
  for (int i = 0; i < count; ++i) {
    if (i > 0) doc += ',';
    doc += std::to_string(i);
  }
  doc += ']';
  return doc;
}

std::string BuildStringArray(int count, const std::string& value) {
  std::string doc = "[";
  for (int i = 0; i < count; ++i) {
    if (i > 0) doc += ',';
    doc += '"' + value + '"';
  }
  doc += ']';
  return doc;
}

std::string BuildRealisticDocument(int users) {
  std::string doc = "{\"users\":[";
  for (int i = 0; i < users; ++i) {
    if (i > 0) doc += ',';
    doc += "{\"id\":" + std::to_string(i) + ",\"name\":\"user_" +
           std::to_string(i) + "\",\"active\":true,\"score\":4.5,\"tags\":[\"a\",\"b\"]}";
  }
  doc += "],\"total\":" + std::to_string(users) + ",\"meta\":{\"ok\":true}}";
  return doc;
}

std::string BuildDeepDocument(int depth) {
  return std::string(depth, '[') + "0" + std::string(depth, ']');
}

const std::string kNestedDocument = R"({
  "name": "OrdinaryJson",
  "version": 1,
  "released": true,
  "rating": 4.5,
  "tags": ["json", "parser", "c++"],
  "author": {"name": "dev", "email": "dev@example.com"},
  "metrics": {"stars": 100, "forks": 20, "issues": [1, 2, 3]}
})";

struct Case {
  const char* name;
  std::string doc;
  std::function<void(const ordinaryjson::OrdinaryJsonNode&)> touch;
};

}  // namespace

int main() {
  const std::string cjk = "\xE4\xB8\xAD\xE6\x96\x87";        // 中文
  const std::string emoji = "\xF0\x9F\x98\x80";              // 😀
  const std::string unicode_value = cjk + "_" + emoji + "_text";

  const std::vector<Case> cases = {
      {"tiny", R"({"a":1})",
       [](const auto& n) { g_sink += n.GetAsObject("a").GetAsInteger(); }},

      {"flat_100", BuildFlatDocument(100),
       [](const auto& n) {
         g_sink += n.GetAsObject("field_000").GetAsString().size();
       }},

      {"nested", kNestedDocument,
       [](const auto& n) {
         g_sink += n.GetAsObject("name").GetAsString().size();
       }},

      {"array_int_1000", BuildIntArray(1000),
       [](const auto& n) { g_sink += n.GetAsArray(0).GetAsInteger(); }},

      {"array_str_1000", BuildStringArray(1000, "value"),
       [](const auto& n) { g_sink += n.GetAsArray(0).GetAsString().size(); }},

      {"unicode_200", BuildStringArray(200, unicode_value),
       [](const auto& n) { g_sink += n.GetAsArray(0).GetAsString().size(); }},

      {"realistic_50", BuildRealisticDocument(50),
       [](const auto& n) {
         g_sink += n.GetAsObject("users")
                       .GetAsArray(0)
                       .GetAsObject("name")
                       .GetAsString()
                       .size();
       }},

      {"deep_100", BuildDeepDocument(100),
       [](const auto& n) { g_sink += n.GetAsArray(0).HasValue(); }},
  };

  std::printf("%-16s %8s | %-10s %-8s | %-10s %-8s\n", "case", "bytes",
              "parse", "MB/s", "serde", "MB/s");
  std::printf("%s\n", std::string(72, '-').c_str());

  for (const auto& c : cases) {
    const size_t bytes = c.doc.size();

    const double parse_ms = Measure([&] {
      ordinaryjson::OrdinaryJsonNode n = ordinaryjson::Parse(c.doc);
      c.touch(n);
    });

    ordinaryjson::OrdinaryJsonNode node = ordinaryjson::Parse(c.doc);
    const double serde_ms =
        Measure([&] { g_sink += node.Stringify().size(); });

    std::printf("%-16s %8zu | %7.3f us %7.1f | %7.3f us %7.1f\n", c.name,
                bytes, parse_ms * 1000.0, ThroughputMBps(bytes, parse_ms),
                serde_ms * 1000.0, ThroughputMBps(bytes, serde_ms));
  }

  std::printf("\n(sink=%llu)\n", (unsigned long long)g_sink);
  return 0;
}
