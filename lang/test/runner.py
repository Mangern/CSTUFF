import json
import os
import subprocess

def test_file(filename: str, expected_stdout: str, expected_stderr: str):

    def compare_output(stdout: str, stderr: str):
        if stdout != expected_stdout:
            print(f"Expected: '{expected_stdout}', got '{stdout}'")
            return False

        if stderr != expected_stderr:
            print(f"Expected: '{expected_stderr}', got '{stderr}'")
            return False

        return True


    full_path = f"./test/files/{filename}"
    result = subprocess.run(
        ["./langc", full_path],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True
    )

    if result.returncode != 0:
        return compare_output(result.stdout, result.stderr)

    result = subprocess.run(
        "./a.out",
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True
    )

    return compare_output(result.stdout, result.stderr)

tests = json.load(open("./test/tests.json"))

for test in tests:
    fn = test["file"]
    success = test_file(
        fn, 
        test.get("expect-stdout", ""),
        test.get("expect-stderr", "")
    )

    if success:
        print(f"OK  : {fn}")
    else:
        print(f"FAIL: {fn}")
