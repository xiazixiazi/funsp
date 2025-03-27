#!/usr/bin/env python3

import os
import sys
import subprocess

# Define your LLVM Pass path
PASS_PATH = "/home/test/my_lib/my_paper/func_split/demo/func_split_pass/func_split_pass.so"
PASS_NAME = "func_split"
CC = "clang"

def run_command(command: str):
    """Execute command and check return value"""
    cmd_list = command.split()
    result = subprocess.run(cmd_list)
    # result = subprocess.run(command, shell=True)
    if result.returncode != 0:
        print(f"Error: Command failed with exit code {result.returncode}")
        sys.exit(result.returncode)

def main():
    # breakpoint()
    # print("the cwd :"+os.getcwd())
    cmd = " ".join(sys.argv)
    argv_list = sys.argv
    # cmd="/home/test/my_lib/my_paper/func_split/demo/custom_compiler/custom_compiler.py  -I. -I./lib  -Ilib -I./lib -Isrc -I./src    -O0 -c -o lib/acl-internal.o lib/acl-internal.c"
    # argv_list=cmd.split()

    # Check if it's a compilation operation (-c parameter)
    if " -c " not in cmd:
        # If not a compilation operation, directly call gcc and pass all arguments
        run_command(CC + " " + " ".join(argv_list[1:]))
        return

    # Extract input file and output file
    input_file = ""
    output_file = ""
    other_args = []  # Save other arguments (like -I, -D, -O2, etc.)

    i = 1
    while i < len(argv_list):
        if argv_list[i].startswith('-o'):
            if len(argv_list[i]) > 2:
                output_file = argv_list[i][2:]
                i += 1
            else:
                output_file = argv_list[i + 1]
                i += 2

        # if argv_list[i] == "-o":
        #     output_file = argv_list[i + 1]
        #     i += 2
        elif argv_list[i].endswith(".c") or argv_list[i].endswith(".cc") or argv_list[i].endswith(".cpp") or argv_list[i].endswith(".cxx"):
            input_file = argv_list[i]
            i += 1
        else:
            other_args.append(argv_list[i])  # Save other arguments
            i += 1

    # note: here need to process the .s .S .hpp file 
    # If no input file found, directly call gcc
    if not input_file:
        run_command(CC + " " + " ".join(argv_list[1:]))
        return

    # If no output file found, default to using .o file with input filename
    if not output_file:
        output_file = os.path.splitext(input_file)[0] + ".o"

    # Generate temporary files
    temp_bc = os.path.splitext(input_file)[0] + ".bc"
    temp_opt_bc = os.path.splitext(input_file)[0] + ".optimized.bc"

    # # Step 1: Compile source file to LLVM IR (.bc file)
    # run_command(f"clang -S -c -emit-llvm -o {temp_bc} {input_file}")

    # Step 1: Compile source file to LLVM IR (.bc file)
    # Preserve all original arguments (like -I, -D, -O2, etc.)
    run_command(f"clang -c -emit-llvm -o {temp_bc} {' '.join(other_args)} {input_file}")

    # Step 2: Run LLVM Pass using opt tool
    run_command(f"opt -load {PASS_PATH} -{PASS_NAME} {temp_bc} -o {temp_opt_bc}")

    # Step 3: Compile optimized LLVM IR to object file
    if "-fPIC" in cmd or "-fpic" in cmd:
        run_command(f"clang -c -fPIC {temp_opt_bc} -o {output_file}")
    else:
        run_command(f"clang -c {temp_opt_bc} -o {output_file}")

    # Clean up temporary files
    # os.remove(temp_bc)
    # os.remove(temp_opt_bc)

if __name__ == "__main__":
    main()
