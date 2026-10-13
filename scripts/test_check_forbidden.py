"""Self-test of scripts/check_forbidden.py: every forbidden token fires, look-alikes do not."""

import os
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import check_forbidden as cf  # noqa: E402


def rules_in(text: str) -> list[str]:
    return [v.rule for v in cf.scan_text(text) if v.severity == "error"]


class PositiveTokens(unittest.TestCase):
    CASES = {
        "float": "float x = 0;",
        "double": "double x = 0;",
        "float-literal": "auto x = 1.5;",
        "random": "#include <random>\n",
        "unordered": "#include <unordered_map>\nstd::unordered_set<int> s;",
        "std-hash": "auto h = std::hash<int>{}(3);",
        "chrono": "#include <chrono>\n",
        "throw": "void f() { throw 1; }",
        "try": "void f() { try { g(); } catch (...) {} }",
        "catch": "void f() { catch (...) {} }",
        "long": "long x = 0;",
        "memcpy": "std::memcpy(&a, &b, 4);",
        "sort": "std::sort(v.begin(), v.end());",
    }

    def test_each_rule_fires(self):
        for rule, snippet in self.CASES.items():
            with self.subTest(rule=rule):
                self.assertIn(rule, rules_in(snippet))

    def test_every_error_rule_has_a_case(self):
        covered = set(self.CASES) | {cf.FLOAT_LITERAL}
        errors = {r.ident for r in cf.RULES if r.severity == "error"}
        self.assertEqual(errors - covered, set())

    def test_rand_and_srand(self):
        self.assertIn("random", rules_in("int x = rand();"))
        self.assertIn("random", rules_in("srand(3);"))
        self.assertIn("random", rules_in("std::shuffle(a, b, g);"))

    def test_clock_variants(self):
        for snippet in ("auto t = std::chrono::steady_clock::now();", "#include <ctime>",
                        "clock_gettime(0, &ts);", "std::time(nullptr);", "system_clock c;"):
            with self.subTest(snippet=snippet):
                self.assertIn("chrono", rules_in(snippet))

    def test_long_variants(self):
        for snippet in ("unsigned long x;", "long long x;", "using T = long;"):
            with self.subTest(snippet=snippet):
                self.assertIn("long", rules_in(snippet))

    def test_memmove(self):
        self.assertIn("memcpy", rules_in("memmove(a, b, 3);"))

    def test_float_literal_forms(self):
        for literal in ("1.", ".5", "1e3", "1E-3", "2.5f", "0x1p3", "0x1.8p1", "1'000.5", "3e+2"):
            with self.subTest(literal=literal):
                self.assertIn("float-literal", rules_in(f"auto x = {literal};"))

    def test_line_numbers_and_path_in_report(self):
        violations = cf.scan_text("int a;\n\nfloat b;\n", "core/src/x.cpp")
        self.assertEqual([(v.path, v.line, v.rule) for v in violations], [("core/src/x.cpp", 3, "float")])
        self.assertIn("core/src/x.cpp:3:", violations[0].render())


class LookAlikes(unittest.TestCase):
    CLEAN = [
        "int longest = 0;",
        "auto it = m.try_emplace(1, 2);",
        "std::stable_sort(v.begin(), v.end());",
        "void f() noexcept;",
        "v.sort();",
        "p->sort();",
        "int sorted = 1;",
        "int partial_sort_count = 0;",
        "auto big = 1'000'000;",
        "auto big = 5'000'000u;",
        "auto h = 0xDEADBEEFull;",
        "auto b = 0b1010'0101;",
        "int doubled = 2;",
        "int floating = 3;",
        "int operand = 4;",
        "int grand_total = 5;",
        "int retry = 6;",
        "int entry = 7;",
        "int d = 1; int e = 2; auto z = d + e;",
        "std::uint64_t x = 0x1e3;",
        "enum class K { sort, long_name };",
        "auto n = Order::sort;",
        "static_assert(sizeof(int) == 4);",
        "auto x = static_cast<int>(y);",
    ]

    def test_clean_snippets(self):
        for snippet in self.CLEAN:
            with self.subTest(snippet=snippet):
                self.assertEqual(rules_in(snippet), [])

    def test_tokens_inside_comments_and_strings_are_ignored(self):
        text = (
            "// float double long throw try rand() unordered_map <chrono>\n"
            "/* float\n   double\n   long */\n"
            'const char* s = "float double long throw try rand( unordered_ <chrono> 1.5";\n'
            "char c = 'x';\n"
            "const char* q = \"quote \\\" float\";\n"
        )
        self.assertEqual(rules_in(text), [])

    def test_raw_string_is_blanked_and_keeps_line_numbers(self):
        text = 'const char* s = R"x(\nfloat double "quoted" long\n)x";\nfloat after;\n'
        violations = cf.scan_text(text)
        self.assertEqual([(v.rule, v.line) for v in violations], [("float", 4)])

    def test_prefixed_raw_string(self):
        text = 'auto s = u8R"(throw try)";\nauto t = LR"d(float)d";\n'
        self.assertEqual(rules_in(text), [])

    def test_continued_line_comment(self):
        text = "// first line \\\nfloat still_comment;\nfloat real;\n"
        violations = cf.scan_text(text)
        self.assertEqual([(v.rule, v.line) for v in violations], [("float", 3)])

    def test_digit_separator_is_not_a_char_literal(self):
        text = "auto a = 1'000;\nfloat b;\nauto c = 'x';\n"
        violations = cf.scan_text(text)
        self.assertEqual([(v.rule, v.line) for v in violations], [("float", 2)])

    def test_char_literals_with_quotes(self):
        text = "char a = '\"';\nchar b = '\\'';\nfloat c;\n"
        violations = cf.scan_text(text)
        self.assertEqual([(v.rule, v.line) for v in violations], [("float", 3)])

    def test_block_comment_between_tokens(self):
        self.assertEqual(rules_in("int /* float */ x;"), [])

    def test_unterminated_block_comment_does_not_crash(self):
        self.assertEqual(rules_in("int x; /* float"), [])


class Waivers(unittest.TestCase):
    def test_sort_waiver_same_line(self):
        text = "std::sort(a, b);  // slg-allow(sort): ids are unique\n"
        self.assertEqual(rules_in(text), [])

    def test_sort_waiver_previous_line(self):
        text = "// slg-allow(sort): keys are unique ids\nstd::sort(a, b);\n"
        self.assertEqual(rules_in(text), [])

    def test_sort_waiver_requires_reason(self):
        text = "std::sort(a, b);  // slg-allow(sort):\n"
        self.assertIn("bad-waiver", rules_in(text))
        self.assertIn("sort", rules_in(text))

    def test_waiver_two_lines_above_does_not_apply(self):
        text = "// slg-allow(sort): why\nint gap;\nstd::sort(a, b);\n"
        self.assertEqual(rules_in(text), ["sort"])

    def test_determinism_rules_cannot_be_waived(self):
        text = "float x;  // slg-allow(float): just this once\n"
        self.assertEqual(sorted(rules_in(text)), ["bad-waiver", "float"])

    def test_waiver_does_not_cover_other_rules_on_the_line(self):
        text = "std::sort(a, b); float x;  // slg-allow(sort): unique\n"
        self.assertEqual(rules_in(text), ["float"])


class Warnings(unittest.TestCase):
    def warnings(self, text: str) -> list[str]:
        return [v.rule for v in cf.scan_text(text) if v.severity == "warning"]

    def test_mutable_static_warns_but_is_not_an_error(self):
        text = "static int counter = 0;\n"
        self.assertEqual(self.warnings(text), ["mutable-static"])
        self.assertEqual(rules_in(text), [])

    def test_static_functions_and_constants_are_quiet(self):
        for snippet in ("static Status decode(int x);", "static constexpr int k = 3;", "static const int k = 3;",
                        "static bool operator==(const A&, const A&);", "static_assert(true);"):
            with self.subTest(snippet=snippet):
                self.assertEqual(self.warnings(snippet), [])

    def test_thread_local_warns(self):
        self.assertEqual(self.warnings("thread_local int t;"), ["mutable-static"])


class CommandLine(unittest.TestCase):
    def run_main(self, files: dict[str, str], *extra: str) -> int:
        with tempfile.TemporaryDirectory() as tmp:
            for name, body in files.items():
                path = Path(tmp) / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(body, encoding="utf-8")
            return cf.main([tmp, *extra])

    def test_clean_tree_passes(self):
        self.assertEqual(self.run_main({"a.hpp": "int x;\n", "sub/b.cpp": "int y;\n"}), 0)

    def test_violation_fails(self):
        self.assertEqual(self.run_main({"a.cpp": "float x;\n"}), 1)

    def test_empty_tree_is_not_a_pass(self):
        self.assertEqual(self.run_main({"notes.txt": "float\n"}), 2)
        self.assertEqual(self.run_main({"notes.txt": "float\n"}, "--allow-empty"), 0)

    def test_missing_path_is_a_usage_error(self):
        self.assertEqual(cf.main(["/nonexistent/slg/path"]), 2)

    def test_strict_warnings(self):
        files = {"a.cpp": "static int n = 0;\n"}
        self.assertEqual(self.run_main(files), 0)
        self.assertEqual(self.run_main(files, "--strict-warnings"), 1)

    def test_non_cpp_files_are_skipped(self):
        self.assertEqual(self.run_main({"a.cpp": "int x;\n", "build.py": "float\n", "notes.md": "double\n"}), 0)


if __name__ == "__main__":
    unittest.main()
