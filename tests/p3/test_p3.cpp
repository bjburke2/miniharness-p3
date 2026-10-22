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

TEST(reg_null_and_size) {
    ToolRegistry r;
    CHECK(r.size() == 0);
    CHECK(r.bucket_count() >= 1 && r.bucket_count() <= 64);
    CHECK(r.find("nothing_here") == nullptr);
    CHECK(!r.add(nullptr));
    CHECK(r.size() == 0);
    REQUIRE(r.add(std::make_unique<NamedTool>("solo")));
    CHECK(r.size() == 1);
    CHECK(!r.add(nullptr));
    CHECK(r.size() == 1);
    CHECK(r.find("other") == nullptr);
    CHECK(r.size() == 1);
}

TEST(reg_for_each_visits_each_once) {
    ToolRegistry r;
    std::vector<std::string> names = {"read", "write", "list", "calc", "echo", "time", "dice", "sort", "grep"};
    for (const std::string& n : names) REQUIRE(r.add(std::make_unique<NamedTool>(n)));
    std::vector<int> visits(names.size(), 0);
    int total = 0;
    r.for_each([&](const Tool& t) {
        ++total;
        for (std::size_t i = 0; i < names.size(); ++i) {
            if (t.name() == names[i]) ++visits[i];
        }
    });
    CHECK(total == static_cast<int>(names.size()));
    for (int v : visits) CHECK(v == 1);
}

TEST(reg_hash_properties) {
    CHECK(ToolRegistry::hash("ab") != ToolRegistry::hash("ba"));
    CHECK(ToolRegistry::hash("abc") == ToolRegistry::hash("abc"));
    CHECK(ToolRegistry::hash("tool_12") != ToolRegistry::hash("tool_21"));
    std::vector<std::size_t> values;
    for (int i = 0; i < 10000; ++i) values.push_back(ToolRegistry::hash("tool_" + std::to_string(i)));
    std::sort(values.begin(), values.end());
    values.erase(std::unique(values.begin(), values.end()), values.end());
    CHECK(values.size() >= 9990);
}

TEST(reg_10000_tools) {
    ToolRegistry r;
    auto head = std::make_unique<NamedTool>("tool_0");
    Tool* head_ptr = head.get();
    REQUIRE(r.add(std::move(head)));
    double worst_load = 0.0;
    for (int i = 1; i < 10000; ++i) {
        REQUIRE(r.add(std::make_unique<NamedTool>("tool_" + std::to_string(i))));
        worst_load = std::max(worst_load, r.load_factor());
    }
    CHECK(worst_load <= 1.0);
    CHECK(r.size() == 10000);
    CHECK(r.find("tool_0") == head_ptr);
    int missing = 0;
    for (int i = 0; i < 10000; ++i) {
        Tool* t = r.find("tool_" + std::to_string(i));
        if (t == nullptr || t->name() != "tool_" + std::to_string(i)) ++missing;
    }
    CHECK(missing == 0);
    CHECK(r.find("tool_10000") == nullptr);
    CHECK(r.max_probe_length() <= 64);
    std::printf("  [10000 tools] bucket_count=%zu load_factor=%.4f max_probe_length=%zu\n",
                r.bucket_count(), r.load_factor(), r.max_probe_length());
}

TEST(reg_alternative_hash) {
    ToolRegistry r;
    for (int i = 0; i < 10000; ++i) REQUIRE(r.add(std::make_unique<NamedTool>("tool_" + std::to_string(i))));
    const std::size_t buckets = r.bucket_count();
    std::vector<int> weak_counts(buckets, 0);
    for (int i = 0; i < 10000; ++i) {
        const std::string name = "tool_" + std::to_string(i);
        const std::size_t weak = static_cast<unsigned char>(name[0]) + name.size();
        ++weak_counts[weak % buckets];
    }
    const int weak_longest = *std::max_element(weak_counts.begin(), weak_counts.end());
    std::printf("  [alternative hash] real max_probe_length=%zu, weak hash s[0]+size longest chain=%d\n",
                r.max_probe_length(), weak_longest);
    CHECK(weak_longest > 64);
    CHECK(r.max_probe_length() <= 64);
}

TEST(ws_accepts_valid_paths) {
    TempDir d;
    Workspace ws(d.path());
    fs::path out;
    std::string reason;
    struct Row { const char* in; fs::path expect; };
    const std::vector<Row> rows = {
        {"notes/todo.txt", ws.root() / "notes" / "todo.txt"},
        {"./notes//todo.txt", ws.root() / "notes" / "todo.txt"},
        {".", ws.root()},
        {"a/../b", ws.root() / "b"},
        {"a/..", ws.root()},
        {"new/dir/file.txt", ws.root() / "new" / "dir" / "file.txt"},
        {"my..notes.txt", ws.root() / "my..notes.txt"},
    };
    for (const Row& row : rows) {
        out.clear();
        CHECK(ws.resolve(row.in, &out, &reason));
        CHECK(out == row.expect);
    }
    CHECK(!fs::exists(ws.root() / "new"));
}

TEST(ws_rejects_bad_paths) {
    TempDir d;
    Workspace ws(d.path());
    fs::path out;
    std::string reason;
    const std::vector<std::string> bad = {
        "",
        std::string("notes") + '\0' + "/x",
        "/etc/passwd",
        "..",
        "../etc/passwd",
        "a/../../b",
        "notes/../../" + ws.root().filename().string() + "/notes/todo.txt",
    };
    for (const std::string& b : bad) {
        reason.clear();
        CHECK(!ws.resolve(b, &out, &reason));
        CHECK(!reason.empty());
    }
}

TEST(calc_spec_table) {
    struct Row { const char* expr; double expect; };
    const std::vector<Row> rows = {
        {"2 + 3 * 4", 14},
        {"10 - 4 - 3", 3},
        {"100 / 10 / 5", 2},
        {"(2 + 3) * 4", 20},
        {"2 * (3 + 4) - 5", 9},
        {"-3 * -3", 9},
        {"7 / 2", 3.5},
    };
    for (const Row& row : rows) {
        double v = 0;
        std::string err;
        CHECK(eval(row.expr, &v, &err));
        CHECK(v == row.expect);
    }
    double v = 0;
    std::string err;
    CHECK(!eval("1 / (2 - 2)", &v, &err));
    CHECK(err == "division by zero");
}

TEST(calc_own_expressions) {
    double v = 0;
    std::string err;
    REQUIRE(eval("8 / 4 / 2 + 1", &v, &err));
    CHECK(v == 2);
    REQUIRE(eval("(1 + 2) * (3 + 4)", &v, &err));
    CHECK(v == 21);
    REQUIRE(eval("((((5))))", &v, &err));
    CHECK(v == 5);
    REQUIRE(eval("0.5 * 4 - -1", &v, &err));
    CHECK(v == 3);
}

TEST(calc_huge_inputs) {
    double v = 0;
    std::string err;
    std::string flat = "0";
    for (int i = 0; i < 20000; ++i) flat += " + 1";
    REQUIRE(eval(flat, &v, &err));
    CHECK(v == 20000);
    const std::string deep = std::string(5000, '(') + "7" + std::string(5000, ')');
    REQUIRE(eval(deep, &v, &err));
    CHECK(v == 7);
}

int main(int argc, char** argv) { return mt::run_all(argc, argv); }
