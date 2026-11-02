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


TEST(reg_null_rejected) {
    ToolRegistry r;
    CHECK(!r.add(nullptr));
    CHECK(r.size() == 0);
}
 
TEST(reg_for_each_visits_each_once) {
    ToolRegistry r;
    for (const char* n : {"a", "b", "c", "d", "e"}) {
        REQUIRE(r.add(std::make_unique<NamedTool>(n)));
    }
 
    int count = 0;
    r.for_each([&](const Tool& t) {
        CHECK(!t.name().empty());
        ++count;
    });
    CHECK(count == 5);
}
 
TEST(reg_10000_tools) {
    ToolRegistry r;
    auto first = std::make_unique<NamedTool>("tool_0");
    Tool* first_ptr = first.get();
    REQUIRE(r.add(std::move(first)));
 
    for (int i = 1; i < 10000; ++i) {
        REQUIRE(r.add(std::make_unique<NamedTool>("tool_" + std::to_string(i))));
        CHECK(r.load_factor() <= 1.0);
    }
 
    for (int i = 0; i < 10000; ++i) {
        CHECK(r.find("tool_" + std::to_string(i)) != nullptr);
    }
 
    CHECK(r.find("tool_0") == first_ptr);
    CHECK(r.max_probe_length() <= 64);
 
    std::printf("  FNV-1a: buckets=%zu load=%.4f max_probe=%zu\n",
                r.bucket_count(), r.load_factor(), r.max_probe_length());
}
 
TEST(reg_alternative_hash) {
    ToolRegistry r;
    for (int i = 0; i < 10000; ++i) {
        REQUIRE(r.add(std::make_unique<NamedTool>("tool_" + std::to_string(i))));
    }
 
    std::size_t nb = r.bucket_count();
    std::vector<int> counts(nb, 0);
    for (int i = 0; i < 10000; ++i) {
        std::string s = "tool_" + std::to_string(i);
        std::size_t weak = static_cast<unsigned char>(s[0]) + s.size();
        ++counts[weak % nb];
    }
 
    int longest = 0;
    for (int c : counts) {
        if (c > longest) longest = c;
    }
 
    std::printf("  weak hash: longest chain=%d (FNV-1a: %zu)\n",
                longest, r.max_probe_length());
    CHECK(static_cast<std::size_t>(longest) > r.max_probe_length());
}
 
TEST(ws_accepts_normalized) {
    TempDir d;
    Workspace ws(d.path());
    fs::path out;
    std::string reason;
 
    REQUIRE(ws.resolve("a/./b/../c", &out, &reason));
    CHECK(out == ws.root() / "a" / "c");
 
    REQUIRE(ws.resolve("./notes//todo.txt", &out, &reason));
    CHECK(out == ws.root() / "notes" / "todo.txt");
 
    REQUIRE(ws.resolve(".", &out, &reason));
    CHECK(out == ws.root());
 
    REQUIRE(ws.resolve("x/..", &out, &reason));
    CHECK(out == ws.root());
 
    // Spec §4.4 rows
    REQUIRE(ws.resolve("a/../b", &out, &reason));
    CHECK(out == ws.root() / "b");
 
    REQUIRE(ws.resolve("a/..", &out, &reason));
    CHECK(out == ws.root());
}
 
TEST(ws_accepts_new_and_dotdot_names) {
    TempDir d;
    Workspace ws(d.path());
    fs::path out;
    std::string reason;
 
    REQUIRE(ws.resolve("new_dir/new_file.txt", &out, &reason));
    CHECK(out == ws.root() / "new_dir" / "new_file.txt");
 
    REQUIRE(ws.resolve("..hidden", &out, &reason));
    CHECK(out == ws.root() / "..hidden");
 
    REQUIRE(ws.resolve("a..b/file", &out, &reason));
    CHECK(out == ws.root() / "a..b" / "file");
 
    REQUIRE(ws.resolve("...", &out, &reason));
    CHECK(out == ws.root() / "...");
 
    // Spec §4.4 rows
    REQUIRE(ws.resolve("new/dir/file.txt", &out, &reason));
    CHECK(out == ws.root() / "new" / "dir" / "file.txt");
 
    REQUIRE(ws.resolve("my..notes.txt", &out, &reason));
    CHECK(out == ws.root() / "my..notes.txt");
}
 
TEST(ws_rejects_bad_paths) {
    TempDir d;
    Workspace ws(d.path());
 
    const std::string_view bad[] = {
        "",
        std::string_view("a\0b", 3),
        "/etc/passwd",
        "..",
        "../x",
        "a/../../b",
        "a/b/../../../c",
        // Spec §4.4 rows
        std::string_view("notes\0/x", 8),
        "notes/../../ws/notes/todo.txt",
    };
 
    for (std::string_view p : bad) {
        fs::path out;
        std::string reason;
        CHECK(!ws.resolve(p, &out, &reason));
        CHECK(!reason.empty());
    }
}
 
TEST(calc_left_to_right) {
    double v = 0;
    std::string err;
 
    REQUIRE(eval("10 - 3 - 2", &v, &err));
    CHECK(v == 5);
 
    REQUIRE(eval("100 / 10 / 2", &v, &err));
    CHECK(v == 5);
 
    REQUIRE(eval("2 * 3 / 4", &v, &err));
    CHECK(v == 1.5);
 
    // Spec §4.6 rows
    REQUIRE(eval("10 - 4 - 3", &v, &err));
    CHECK(v == 3);
 
    REQUIRE(eval("100 / 10 / 5", &v, &err));
    CHECK(v == 2);
}
 
TEST(calc_parens_and_negatives) {
    double v = 0;
    std::string err;
 
    REQUIRE(eval("(2 + 3) * 4", &v, &err));
    CHECK(v == 20);
 
    REQUIRE(eval("((7))", &v, &err));
    CHECK(v == 7);
 
    REQUIRE(eval("-3 + 5", &v, &err));
    CHECK(v == 2);
 
    REQUIRE(eval("(-2 + -3)", &v, &err));
    CHECK(v == -5);
 
    REQUIRE(eval("2 * -3", &v, &err));
    CHECK(v == -6);
 
    REQUIRE(eval("2 * (3 + 4) - 5", &v, &err));
    CHECK(v == 9);
 
    REQUIRE(eval("-3 * -3", &v, &err));
    CHECK(v == 9);
 
    REQUIRE(eval("7 / 2", &v, &err));
    CHECK(v == 3.5);
}
 
TEST(calc_my_own) {
    double v = 0;
    std::string err;
 
    REQUIRE(eval("2 * (3 + 4) - 10 / 5", &v, &err));
    CHECK(v == 12);
 
    err.clear();
    CHECK(!eval("(1 + 2", &v, &err));
    CHECK(!err.empty());
 
    err.clear();
    CHECK(!eval("1 + 2)", &v, &err));
    CHECK(!err.empty());
 
    err.clear();
    CHECK(!eval("1 / (2 - 2)", &v, &err));
    CHECK(err == "division by zero");
}

int main(int argc, char** argv) { return mt::run_all(argc, argv); }
