// tests/p3/test_p3.cpp — YOUR P3 TEST SUITE.
//
// The five tests below are worked examples. Keep them, and add your own
// until you have AT LEAST 15 (spec §6 lists what must be covered).
// Name every test <area>_<what>: reg_ for the registry, ws_ for the
// workspace, calc_ for the calculator.
//
// Build & run:   cmake --build build && ./build/test_p3
// One area:      ./build/test_p3 reg_        (names starting with "reg_")
// One test:      ./build/test_p3 =reg_add_and_find

#include "../mini_test.h"
#include "../test_helpers.h"     // TempDir, write_text, args1, ...

#include "tools/calculator.h"
#include "tools/tool_registry.h"
#include "tools/workspace.h"

#include <algorithm>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

// A tool whose name you choose — use it to fill the registry in tests.
class NamedTool : public Tool {
public:
    explicit NamedTool(std::string n) : n_(std::move(n)), schema_(JsonValue::object()) {}
    std::string name() const override { return n_; }
    std::string description() const override { return "test tool"; }
    const JsonValue& schema() const override { return schema_; }
    ToolResult invoke(const JsonValue&, ToolContext&) override { return ToolResult::success(n_); }
private:
    std::string n_;
    JsonValue   schema_;   // an empty JSON object: {}
};

// Tokenize with the provided tokenizer, then run YOUR calc_evaluate.
bool eval(const std::string& expr, double* value, std::string* error) {
    std::vector<CalcToken> tokens;
    if (!calc_tokenize(expr, &tokens, error)) return false;
    return calc_evaluate(tokens, value, error);
}

}  // namespace

// ---------------------------------------------------------------- examples

TEST(reg_add_and_find) {
    ToolRegistry r;
    auto t = std::make_unique<NamedTool>("alpha");
    Tool* raw = t.get();                        // remember the address before moving
    REQUIRE(r.add(std::move(t)));
    CHECK(r.size() == 1);
    CHECK(r.find("alpha") == raw);              // same object, not a copy
    CHECK(r.find("beta") == nullptr);
}

TEST(reg_duplicate_keeps_original) {
    ToolRegistry r;
    auto first = std::make_unique<NamedTool>("alpha");
    Tool* original = first.get();
    REQUIRE(r.add(std::move(first)));
    CHECK(!r.add(std::make_unique<NamedTool>("alpha")));
    CHECK(r.find("alpha") == original);
    CHECK(r.size() == 1);
}

TEST(ws_rejects_parent_escape) {
    TempDir d;
    Workspace ws(d.path());
    fs::path out;
    std::string reason;
    REQUIRE(ws.resolve("notes/todo.txt", &out, &reason));   // sanity: accepts *something*
    CHECK(out == ws.root() / "notes" / "todo.txt");
    CHECK(!ws.resolve("../etc/passwd", &out, &reason));
    CHECK(!reason.empty());
}

TEST(calc_precedence) {
    double v = 0;
    std::string err;
    REQUIRE(eval("2 + 3 * 4", &v, &err));
    CHECK(v == 14);
}

TEST(calc_division_by_zero) {
    double v = 0;
    std::string err;
    CHECK(!eval("1 / 0", &v, &err));
    CHECK(err == "division by zero");
}

// ------------------------------------------------------------- your tests
//
// Still to write (spec §6 has the full list). Suggested names:
//   reg_null_rejected, reg_for_each_visits_each_once
//   reg_10000_tools               REQUIRED: print bucket_count / load_factor /
//                                 max_probe_length, check the bound
//   reg_alternative_hash          REQUIRED: longest chain of a weak hash
//   ws_<case>                     REQUIRED: every row of the table in spec §4.4
//   calc_<case>                   left-to-right, parentheses, negatives

TEST(reg_null_rejected) {
    ToolRegistry r;
    CHECK(!r.add(nullptr));
    CHECK(r.size() == 0);
    CHECK(r.find("anything") == nullptr);
}

TEST(reg_empty_table) {
    ToolRegistry r;
    CHECK(r.size() == 0);
    CHECK(r.bucket_count() >= 1);
    CHECK(r.bucket_count() <= 64);
    CHECK(r.load_factor() == 0.0);
    CHECK(r.max_probe_length() == 0);
}

TEST(reg_size_tracks_adds) {
    ToolRegistry r;
    REQUIRE(r.add(std::make_unique<NamedTool>("a")));
    CHECK(r.size() == 1);
    CHECK(!r.add(std::make_unique<NamedTool>("a")));
    CHECK(r.size() == 1);
    CHECK(!r.add(nullptr));
    CHECK(r.size() == 1);
    REQUIRE(r.add(std::make_unique<NamedTool>("b")));
    CHECK(r.size() == 2);
}

TEST(reg_for_each_visits_each_once) {
    ToolRegistry r;
    const char* names[] = {"one", "two", "three", "four", "five", "six", "seven"};
    for (const char* n : names) REQUIRE(r.add(std::make_unique<NamedTool>(n)));
    std::vector<std::string> seen;
    r.for_each([&](const Tool& t) { seen.push_back(t.name()); });
    CHECK(seen.size() == 7);
    for (const char* n : names) {
        int count = 0;
        for (const std::string& s : seen) {
            if (s == n) ++count;
        }
        CHECK(count == 1);
    }
}

TEST(reg_hash_is_order_sensitive_and_full_width) {
    CHECK(ToolRegistry::hash("ab") != ToolRegistry::hash("ba"));
    CHECK(ToolRegistry::hash("tool_1234") != ToolRegistry::hash("tool_1243"));
    CHECK(ToolRegistry::hash("abc") == ToolRegistry::hash("abc"));
    std::vector<std::size_t> hashes;
    for (int i = 0; i < 10000; ++i) hashes.push_back(ToolRegistry::hash("tool_" + std::to_string(i)));
    std::sort(hashes.begin(), hashes.end());
    hashes.erase(std::unique(hashes.begin(), hashes.end()), hashes.end());
    CHECK(hashes.size() >= 9990);
}

TEST(reg_10000_tools) {
    ToolRegistry r;
    std::vector<std::string> names;
    for (int i = 0; i < 10000; ++i) names.push_back("tool_" + std::to_string(i));

    auto first = std::make_unique<NamedTool>(names[0]);
    Tool* first_ptr = first.get();
    REQUIRE(r.add(std::move(first)));

    bool load_ok = true;
    for (int i = 1; i < 10000; ++i) {
        REQUIRE(r.add(std::make_unique<NamedTool>(names[i])));
        if (r.load_factor() > 1.0) load_ok = false;
    }
    CHECK(load_ok);
    CHECK(r.size() == 10000);
    CHECK(r.find(names[0]) == first_ptr);
    for (const std::string& n : names) {
        Tool* t = r.find(n);
        REQUIRE(t != nullptr);
        CHECK(t->name() == n);
    }
    CHECK(r.find("tool_10000") == nullptr);
    CHECK(r.load_factor() <= 1.0);
    CHECK(r.max_probe_length() <= 64);
    std::printf("  [10000 tools] bucket_count=%zu load_factor=%.4f max_probe_length=%zu\n",
                r.bucket_count(), r.load_factor(), r.max_probe_length());
}

TEST(reg_alternative_hash) {
    ToolRegistry r;
    for (int i = 0; i < 10000; ++i) REQUIRE(r.add(std::make_unique<NamedTool>("tool_" + std::to_string(i))));
    std::size_t buckets = r.bucket_count();
    std::vector<int> weak(buckets, 0);
    std::vector<int> real(buckets, 0);
    for (int i = 0; i < 10000; ++i) {
        std::string s = "tool_" + std::to_string(i);
        std::size_t h = static_cast<unsigned char>(s[0]) + s.size();
        ++weak[h % buckets];
        ++real[ToolRegistry::hash(s) % buckets];
    }
    int weak_longest = *std::max_element(weak.begin(), weak.end());
    int real_longest = *std::max_element(real.begin(), real.end());
    std::printf("  [alternative hash] buckets=%zu  FNV-1a longest chain=%d  s[0]+size longest chain=%d\n",
                buckets, real_longest, weak_longest);
    CHECK(static_cast<std::size_t>(real_longest) == r.max_probe_length());
    CHECK(weak_longest > 64);
}

TEST(ws_accepts_valid_paths) {
    TempDir d;
    Workspace ws(d.path());
    fs::path out;
    std::string reason;
    REQUIRE(ws.resolve("notes/todo.txt", &out, &reason));
    CHECK(out == ws.root() / "notes" / "todo.txt");
    REQUIRE(ws.resolve("./notes//todo.txt", &out, &reason));
    CHECK(out == ws.root() / "notes" / "todo.txt");
    REQUIRE(ws.resolve(".", &out, &reason));
    CHECK(out == ws.root());
    REQUIRE(ws.resolve("a/../b", &out, &reason));
    CHECK(out == ws.root() / "b");
    REQUIRE(ws.resolve("a/..", &out, &reason));
    CHECK(out == ws.root());
    REQUIRE(ws.resolve("new/dir/file.txt", &out, &reason));
    CHECK(out == ws.root() / "new" / "dir" / "file.txt");
    REQUIRE(ws.resolve("my..notes.txt", &out, &reason));
    CHECK(out == ws.root() / "my..notes.txt");
}

TEST(ws_rejects_empty_and_nul) {
    TempDir d;
    Workspace ws(d.path());
    fs::path out;
    std::string reason;
    CHECK(!ws.resolve("", &out, &reason));
    CHECK(!reason.empty());
    reason.clear();
    CHECK(!ws.resolve(std::string("notes") + '\0' + "/x", &out, &reason));
    CHECK(!reason.empty());
}

TEST(ws_rejects_absolute) {
    TempDir d;
    Workspace ws(d.path());
    fs::path out;
    std::string reason;
    CHECK(!ws.resolve("/etc/passwd", &out, &reason));
    CHECK(!reason.empty());
    reason.clear();
    CHECK(!ws.resolve((ws.root() / "notes").string(), &out, &reason));
    CHECK(!reason.empty());
}

TEST(ws_rejects_dotdot) {
    TempDir d;
    Workspace ws(d.path());
    fs::path out;
    std::string reason;
    const char* bad[] = {"..", "../etc/passwd", "a/../../b"};
    for (const char* b : bad) {
        reason.clear();
        CHECK(!ws.resolve(b, &out, &reason));
        CHECK(!reason.empty());
    }
    reason.clear();
    std::string sneaky = std::string("notes/../../") + ws.root().filename().string() + "/notes/todo.txt";
    CHECK(!ws.resolve(sneaky, &out, &reason));
    CHECK(!reason.empty());
}

TEST(ws_does_not_touch_filesystem) {
    TempDir d;
    Workspace ws(d.path());
    fs::path out;
    std::string reason;
    REQUIRE(ws.resolve("never/created/file.txt", &out, &reason));
    CHECK(!fs::exists(out));
    CHECK(!fs::exists(ws.root() / "never"));
}

TEST(calc_table_rows) {
    double v = 0;
    std::string err;
    REQUIRE(eval("10 - 4 - 3", &v, &err));
    CHECK(v == 3);
    REQUIRE(eval("100 / 10 / 5", &v, &err));
    CHECK(v == 2);
    REQUIRE(eval("(2 + 3) * 4", &v, &err));
    CHECK(v == 20);
    REQUIRE(eval("2 * (3 + 4) - 5", &v, &err));
    CHECK(v == 9);
    REQUIRE(eval("-3 * -3", &v, &err));
    CHECK(v == 9);
    REQUIRE(eval("7 / 2", &v, &err));
    CHECK(v == 3.5);
}

TEST(calc_division_by_zero_in_parens) {
    double v = 0;
    std::string err;
    CHECK(!eval("1 / (2 - 2)", &v, &err));
    CHECK(err == "division by zero");
}

TEST(calc_own_expressions) {
    double v = 0;
    std::string err;
    REQUIRE(eval("1.5 + 2.25 * 2", &v, &err));
    CHECK(v == 6);
    REQUIRE(eval("((2))", &v, &err));
    CHECK(v == 2);
    REQUIRE(eval("2 - (3 - 4) * -2", &v, &err));
    CHECK(v == 0);
    REQUIRE(eval("42", &v, &err));
    CHECK(v == 42);
}

TEST(calc_very_long_expressions) {
    double v = 0;
    std::string err;
    std::string plus = "1";
    for (int i = 0; i < 20000; ++i) plus += "+1";
    REQUIRE(eval(plus, &v, &err));
    CHECK(v == 20001);
    std::string nested(5000, '(');
    nested += "1";
    nested += std::string(5000, ')');
    REQUIRE(eval(nested, &v, &err));
    CHECK(v == 1);
}

int main(int argc, char** argv) { return mt::run_all(argc, argv); }
