"""Self-test of scripts/check_golden.py with fake engines: matches, mismatches and every way to pass vacuously."""

import contextlib
import io
import os
import shlex
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import check_golden as cg  # noqa: E402

ECHO_TRACE = (
    "import sys\n"
    "sys.stdout.write(open(sys.argv[2], encoding='utf-8').read().replace('lscn', 'ltrace'))\n"
)
WRITE_OUT = (
    "import sys\n"
    "open(sys.argv[3], 'w', encoding='utf-8').write(open(sys.argv[2], encoding='utf-8').read().replace('lscn', 'ltrace'))\n"
)


class CheckGolden(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self._tmp.cleanup)
        self.dir = Path(self._tmp.name)
        self.golden = self.dir / "golden"
        self.golden.mkdir()
        (self.dir / "echo.py").write_text(ECHO_TRACE, encoding="utf-8")
        (self.dir / "write_out.py").write_text(WRITE_OUT, encoding="utf-8")
        self.echo = f"{shlex.quote(sys.executable)} {shlex.quote(str(self.dir / 'echo.py'))} {{lnet}} {{lscn}}"
        self.write_out = f"{shlex.quote(sys.executable)} {shlex.quote(str(self.dir / 'write_out.py'))} {{lnet}} {{lscn}} {{out}}"

    def add(self, name, scn="lscn 1\nticks 2\n", trace=None, net="lnet 1\n"):
        (self.golden / f"{name}.lnet").write_text(net, encoding="utf-8")
        (self.golden / f"{name}.lscn").write_text(scn, encoding="utf-8")
        expected = scn.replace("lscn", "ltrace") if trace is None else trace
        (self.golden / f"{name}.ltrace").write_text(expected, encoding="utf-8")

    def run_check(self, *extra):
        argv = ["--golden-dir", str(self.golden), *extra]
        with contextlib.redirect_stdout(io.StringIO()) as out, contextlib.redirect_stderr(io.StringIO()):
            status = cg.main(argv)
        return status, out.getvalue()

    def test_matching_oracle_and_runner_pass(self):
        self.add("a")
        self.add("sub_b")
        status, text = self.run_check("--oracle-cmd", self.echo, "--runner-cmd", self.echo)
        self.assertEqual(status, 0, text)
        self.assertIn("4 comparisons, 0 failures", text)

    def test_nested_directories_are_covered(self):
        (self.golden / "regressions").mkdir()
        (self.golden / "regressions" / "r1.lnet").write_text("lnet 1\n", encoding="utf-8")
        (self.golden / "regressions" / "r1.lscn").write_text("lscn 1\n", encoding="utf-8")
        (self.golden / "regressions" / "r1.ltrace").write_text("ltrace 1\n", encoding="utf-8")
        status, text = self.run_check("--oracle-cmd", self.echo)
        self.assertEqual(status, 0, text)
        self.assertIn("regressions/r1", text)

    def test_one_changed_byte_fails_and_shows_the_diff(self):
        self.add("a", trace="ltrace 1\nticks 3\n")
        status, text = self.run_check("--oracle-cmd", self.echo)
        self.assertEqual(status, 1)
        self.assertIn("FAIL  a [oracle]", text)
        self.assertIn("-ticks 3", text)
        self.assertIn("+ticks 2", text)

    def test_only_the_runner_can_disagree(self):
        self.add("a")
        bad_runner = f"{shlex.quote(sys.executable)} -c \"print('nope')\" {{lnet}} {{lscn}}"
        status, text = self.run_check("--oracle-cmd", self.echo, "--runner-cmd", bad_runner)
        self.assertEqual(status, 1)
        self.assertIn("ok    a [oracle]", text)
        self.assertIn("FAIL  a [c++]", text)

    def test_trailing_newline_difference_is_a_failure(self):
        self.add("a", trace="ltrace 1\nticks 2")
        status, _ = self.run_check("--oracle-cmd", self.echo)
        self.assertEqual(status, 1)

    def test_out_placeholder(self):
        self.add("a")
        status, text = self.run_check("--oracle-cmd", self.write_out)
        self.assertEqual(status, 0, text)

    def test_command_that_cannot_start_fails(self):
        self.add("a")
        status, text = self.run_check("--oracle-cmd", "/nonexistent/engine {lnet} {lscn}")
        self.assertEqual(status, 1)
        self.assertIn("cannot start", text)

    def test_failing_command_fails(self):
        self.add("a")
        failing = f"{shlex.quote(sys.executable)} -c \"import sys; sys.exit(3)\" {{lnet}} {{lscn}}"
        status, text = self.run_check("--oracle-cmd", failing)
        self.assertEqual(status, 1)
        self.assertIn("exit status 3", text)

    def test_missing_input_fails(self):
        self.add("a")
        (self.golden / "a.lscn").unlink()
        status, text = self.run_check("--oracle-cmd", self.echo)
        self.assertEqual(status, 1)
        self.assertIn("missing input a.lscn", text)

    def test_no_traces_is_not_a_pass(self):
        status, _ = self.run_check("--oracle-cmd", self.echo)
        self.assertEqual(status, 2)

    def test_missing_directory_is_not_a_pass(self):
        with contextlib.redirect_stderr(io.StringIO()), contextlib.redirect_stdout(io.StringIO()):
            status = cg.main(["--golden-dir", str(self.dir / "absent"), "--oracle-cmd", self.echo])
        self.assertEqual(status, 2)


if __name__ == "__main__":
    unittest.main()
