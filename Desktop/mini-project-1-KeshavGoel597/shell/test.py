import os

from time import sleep
from codecs import decode
from psutil import Process
from shutil import rmtree
from socket import gethostname
from getpass import getuser
from pathlib import Path
from pexpect import spawn

KILL_SIGNAL = 9
TIMEOUT = 5.0
CRLF = "\r\n"

class ShellTester:
    """Basic tests for shell functionality and I/O format compliance"""
    
    def setup_method(self, test_method):
        if not os.path.exists('./shell.out'):
            raise FileNotFoundError(
                "shell.out not found. Please compile your shell using 'make all'"
            )
        
        self.test_dir = Path(".shell_test").resolve()
        os.mkdir(self.test_dir)
        self.original_dir = os.getcwd()
        shell_path = os.path.join(self.original_dir, 'shell.out')

        self.shell = spawn(shell_path, cwd=self.test_dir)
        self.proc = Process(pid=self.shell.pid)

        os.chdir(self.test_dir)
        
    def teardown_method(self, test_method):
        self.shell.kill(KILL_SIGNAL)
        
        os.chdir(self.original_dir)
        rmtree(self.test_dir, ignore_errors=True)
    
    def get_cwd(self):
        return self.proc.cwd()

    def assert_cwd(self, path: str | Path):
        if isinstance(path, Path):
            path = str(path.resolve())
        assert self.get_cwd() == path
    
    def _assert_prompt(self):
        sleep(0.001) # needed due to weird race condition in TestPartB.test_part1
        prompt = f"<{getuser()}@{gethostname()}:{self.get_cwd().replace(str(self.test_dir),'~')}> "
        self.shell.expect_exact(prompt, timeout=TIMEOUT)
    
    def _sort_listing(self, listing: list[str]):
        return sorted(listing, key=lambda x: x.lower())

    def assert_startup(self):
        self._assert_prompt()
    
    def run_cmd(self, cmd: str) -> str:
        self.shell.sendline(cmd)
        self._assert_prompt()
        return decode(self.shell.before, "utf-8").replace(cmd+CRLF, "") # type: ignore

class TestPartA(ShellTester):
    def test_part1(self):
        self.assert_startup()

    def test_part2(self):
        self.assert_startup()

        self.run_cmd("hi!")
        self.run_cmd("bye?")
    
    def test_part3(self):
        self.assert_startup()

        self.run_cmd("Hi there guys!")
        output = self.run_cmd("cat meow.txt | ; meow")
        expected = "Invalid Syntax!" + CRLF
        assert output == expected

class TestPartB(ShellTester):
    def test_part1(self):
        self.assert_startup()

        home = self.get_cwd()
        self.run_cmd("hop ~")
        self.assert_cwd(home)

        parent = Path(home).parent
        self.run_cmd("hop ..")
        self.assert_cwd(parent)

        self.run_cmd("hop -")
        self.assert_cwd(home)

        self.run_cmd("hop .. .. .. .. ~ /")
        self.assert_cwd("/")
    
    def test_part2(self):
        self.assert_startup()

        os.mkdir("meow/")
        os.chdir("meow/")

        os.mkdir(".git/")
        os.mkdir("src/")
        os.mkdir("llm_completions/")

        Path(".gitignore").touch()
        Path("shell.out").touch()
        Path("Makefile").touch()
        Path("README.md").touch()

        expected_items = self._sort_listing([
            "meow",
            "..",
            ".",
        ])

        out = self.run_cmd("reveal -la")
        assert expected_items == out.split()

        expected_items = self._sort_listing([
            ".git",
            "src",
            "llm_completions",
            ".",
            "..",
            ".gitignore",
            "shell.out",
            "Makefile",
            "README.md",
        ])

        out = self.run_cmd("reveal -lalalalaaaalal -lalala -al meow")
        assert expected_items == out.split()

class TestPartC(ShellTester):
    def test_part1(self):
        self.assert_startup()

        file = Path("test.txt")
        file.touch()
        file.write_text("Hi there!")

        out = self.run_cmd("cat test.txt")
        assert out == file.read_text()
    
    def test_part2(self):
        self.assert_startup()

        file = Path("test.txt")
        file.touch()
        file.write_text("Hi there!")

        out = self.run_cmd("cat < test.txt")
        assert out == file.read_text()
    
    def test_part3(self):
        self.assert_startup()

        expected = "Hi There!"
        self.run_cmd(f"echo {expected} > test.txt")

        file = Path("test.txt")
        assert expected+"\n" == file.read_text()

        self.run_cmd(f"echo {expected} >> test.txt")
        expected += "\n" + expected + "\n"
        assert expected == file.read_text()
    
    def test_part4(self):
        self.assert_startup()

        expected = "Hi There!"
        out = self.run_cmd(f"echo {expected} | cat | wc > test.txt")
        assert out == ""

        file = Path("test.txt")
        assert list(map(int, file.read_text().split())) == [1, 2, 10]

class TestPartD(ShellTester):
    def test_part1(self):
        self.assert_startup()

        expected = "Hi There!"
        out = self.run_cmd(f"echo {expected} > test.txt; cat test.txt; echo {expected} >> test.txt; cat test.txt")
        assert CRLF.join(expected for _ in range(3))+CRLF == out
    
    def test_part2(self):
        self.assert_startup()

        expected = "Hi There!"
        out = self.run_cmd(f"sleep 10 & echo {expected} > test.txt; cat test.txt")
        assert expected+CRLF == out