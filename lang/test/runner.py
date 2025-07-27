import json
import os
import subprocess

def test_file(filename: str, expected_output: str):
    full_path = f"./test/files/{filename}"
    result = subprocess.run(
        ["./langc", full_path],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True
    )

    if result.returncode != 0:
        print("COMPILATION FAILED")
        print(f"Stdout: {result.stdout}")
        print(f"Stderr: {result.stderr}")
        return False

    result = subprocess.run(
        "./a.out",
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True
    )

    if result.returncode != 0:
        print("PROGRAM FAILED")
        print(f"Stdout: {result.stdout}")
        print(f"Stderr: {result.stderr}")
        return False

    if result.stdout != expected_output:
        print(f"Expected: '{expected_output}', got '{result.stdout}'")
        return False

    return True

tests = json.load(open("./test/tests.json"))

for test in tests:
    fn = test["file"]
    success = test_file(fn, test["expected"])

    if success:
        print(f"OK  : {fn}")
    else:
        print(f"FAIL: {fn}")
