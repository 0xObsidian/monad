import os
import re
from glob import glob
from os import path
from subprocess import check_output

__this_file = path.realpath(__file__)
__this_dir = path.dirname(__this_file)
__test_dir = path.join(__this_dir, "disas")

TARGET_REGEX = "((?:call|ja|jae|jb|jbe|je|jmp|jne|js)[ ]+)0x[0-9a-f]+[ ]+"
TARGET_PATTERN = re.compile(TARGET_REGEX)


def _get_test_ids(test_dir):
    test_ids = []
    files = glob(path.join(test_dir, "*.py"))
    for file in files:
        file = path.basename(file)
        if file.startswith("_"):
            continue
        test_id = file[0:-3]
        test_ids.append(test_id)
    return test_ids


def _create_test_class(test_dir):
    class TestDisas:
        @staticmethod
        def _load_test(test_id):
            test_file = path.join(TestDisas._test_dir, "%s.py" % (test_id,))
            ns = {}
            with open(test_file, "r") as f:
                code = compile(f.read(), test_file, "exec")
                exec(code, ns, ns)
            obj = ns.get("obj")
            syms = ns.get("syms")
            assert isinstance(obj, (str,))
            assert isinstance(syms, (list,))
            for sym in syms:
                assert isinstance(sym, (str,))
            return obj, syms

        @staticmethod
        def _load_result(test_id):
            result_file = path.join(TestDisas._test_dir, "%s.dis" % (test_id,))
            with open(result_file, "r") as f:
                result = f.read()
            return result

        @staticmethod
        def _gen_result(test_id):
            obj, syms = TestDisas._load_test(test_id)
            obj = path.join("**", obj)
            objs = glob(obj, recursive=True)
            assert len(objs) == 1
            obj = objs[0]
            cmd = [
                "gdb",
                "-batch",
                "-ex",
                "file %s" % (obj,),
            ]
            for sym in syms:
                cmd += [
                    "-ex",
                    "disas '%s'" % (sym,),
                ]
            result = check_output(cmd)
            result = result.decode("ascii")
            result = result.splitlines()

            def remove_address(line: str):
                line = line.expandtabs()
                if not line.startswith("   0x"):
                    return line
                line = line[22:]
                assert line[0] == "<"
                return line

            result = [remove_address(line) for line in result]

            def remove_target(line: str):
                return TestDisas._target_pattern.sub("\\1", line, count=1)

            result = [remove_target(line) for line in result]

            def gdb_10_to_12(line: str):  # TODO
                return line.replace("nopw   %cs:0", "cs nopw 0")

            result = [gdb_10_to_12(line) for line in result]
            result = "\n".join(result)
            return result

        @staticmethod
        def _gen_result_objdump(test_id):
            obj, syms = TestDisas._load_test(test_id)
            obj = path.join("**", obj)
            objs = glob(obj, recursive=True)
            assert len(objs) == 1
            obj = objs[0]
            cmd = [
                "objdump",
                "--demangle",
                "--disassemble",
                "--reloc",
                "--insn-width=8",
                obj,
            ]
            result = check_output(cmd)
            result = result.decode("ascii")
            result = result.splitlines()

            result2 = []
            match = False
            for line in result:
                if not match:
                    for sym in syms:
                        if "<%s>:" % (sym,) in line:
                            match = True
                            break
                if match:
                    result2.append(line)
                    if not line.strip():
                        match = False

            result = "\n".join(result2)
            return result

        @staticmethod
        def _save_result(test_id):
            result = TestDisas._gen_result_objdump(test_id)
            result_file = path.join(TestDisas._test_dir, "%s.dis" % (test_id,))
            with open(result_file, "w") as f:
                f.write(result)

        @staticmethod
        def _run_test(test_id):
            expected_result = TestDisas._load_result(test_id)
            current_result = TestDisas._gen_result_objdump(test_id)
            assert expected_result == current_result

    setattr(TestDisas, "_test_dir", test_dir)
    setattr(TestDisas, "_target_pattern", TARGET_PATTERN)

    test_ids = _get_test_ids(test_dir)
    for test_id in test_ids:

        def test_func(self, test_id=test_id):
            TestDisas._run_test(test_id)

        setattr(TestDisas, "test_%s" % (test_id,), test_func)

    return TestDisas


TestDisas = _create_test_class(__test_dir)


def main():
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument("cmd", choices=("generate",))
    args = parser.parse_args()
    if args.cmd == "generate":
        test_ids = _get_test_ids(__test_dir)
        for test_id in test_ids:
            TestDisas._save_result(test_id)


if __name__ == "__main__":
    main()
