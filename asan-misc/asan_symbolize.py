#!/usr/bin/env python3
"""asan_symbolize.py - Read bare-metal ASan logs, extract PC addresses, and format pretty stack traces using llvm-symbolizer."""

import argparse
import os
import re
import subprocess
import sys

def find_symbolizer(custom_path=None):
    if custom_path and os.path.isfile(custom_path):
        return custom_path
    script_dir = os.path.dirname(os.path.abspath(__file__))
    local_build = os.path.join(script_dir, "../build/bin/llvm-symbolizer")
    if os.path.isfile(local_build):
        return local_build
    return "llvm-symbolizer"

def symbolize_address(proc, addr):
    """Send an address to llvm-symbolizer and read back frames until empty line."""
    proc.stdin.write(addr + "\n")
    proc.stdin.flush()
    frames = []
    while True:
        func = proc.stdout.readline()
        if not func or func == "\n":
            break
        func = func.rstrip()
        loc = proc.stdout.readline().rstrip()
        frames.append((func, loc))
    return frames

def main():
    parser = argparse.ArgumentParser(description="Symbolize ASan logs from QEMU baremetal runs.")
    parser.add_argument("-e", "--elf", default="firmware.elf", help="Path to ELF binary (default: firmware.elf)")
    parser.add_argument("-s", "--symbolizer", help="Path to llvm-symbolizer binary")
    parser.add_argument("log_files", nargs="*", help="Log files to read (if none, reads from stdin)")
    args = parser.parse_args()

    symbolizer_bin = find_symbolizer(args.symbolizer)
    if not os.path.isfile(args.elf):
        sys.stderr.write(f"Warning: ELF binary '{args.elf}' not found. Printing raw output without symbolization.\n")
        proc = None
    else:
        cmd = [
            symbolizer_bin,
            f"--obj={args.elf}",
            "--inlining",
            "--functions=linkage",
            "--demangle"
        ]
        try:
            proc = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True)
        except Exception as e:
            sys.stderr.write(f"Failed to launch symbolizer '{symbolizer_bin}': {e}\n")
            proc = None

    pc_sp_re = re.compile(r"^\s*(?:#\d+\s+)?pc\s+(0x[0-9a-fA-F]+)\s+sp\s+(0x[0-9a-fA-F]+)")
    summary_re = re.compile(r"^(.*?)(PC=(0x[0-9a-fA-F]+))(.*)$")

    def process_line(line, frame_counter=[0]):
        line_str = line.rstrip("\n\r")
        if not proc:
            print(line_str)
            return

        if "Stack trace:" in line_str:
            print(line_str)
            frame_counter[0] = 0
            return

        m = pc_sp_re.match(line_str)
        if m:
            pc_hex, sp_hex = m.group(1), m.group(2)
            frames = symbolize_address(proc, pc_hex)
            if not frames or frames[0][0] == "??":
                print(f"  #{frame_counter[0]:02d} pc {pc_hex} sp {sp_hex} (??)")
                frame_counter[0] += 1
                return

            top_func, top_loc = frames[0]
            top_loc_short = top_loc
            if "/" in top_loc:
                if "llvm-asan/" in top_loc:
                    top_loc_short = top_loc.split("llvm-asan/")[1]
                else:
                    top_loc_short = "/".join(top_loc.split("/")[-2:])

            print(f"  #{frame_counter[0]:02d} {pc_hex} in {top_func} at {top_loc_short} (sp {sp_hex})")
            frame_counter[0] += 1

            for func, loc in frames[1:]:
                loc_short = loc
                if "llvm-asan/" in loc:
                    loc_short = loc.split("llvm-asan/")[1]
                elif "/" in loc:
                    loc_short = "/".join(loc.split("/")[-2:])
                print(f"      [inlined] {func} at {loc_short}")
            return

        sm = summary_re.match(line_str)
        if sm:
            prefix, pc_part, pc_hex, suffix = sm.group(1), sm.group(2), sm.group(3), sm.group(4)
            frames = symbolize_address(proc, pc_hex)
            if frames and frames[0][0] != "??":
                top_func, top_loc = frames[0]
                loc_short = top_loc.split("llvm-asan/")[1] if "llvm-asan/" in top_loc else top_loc
                print(f"{prefix}PC={pc_hex} ({top_func} at {loc_short}){suffix}")
            else:
                print(line_str)
            return

        print(line_str)

    def process_stream(stream):
        frame_counter = [0]
        for line in stream:
            process_line(line, frame_counter)

    try:
        if args.log_files:
            for log_file in args.log_files:
                if len(args.log_files) > 1:
                    print(f"\n--- Symbolizing {log_file} ---")
                with open(log_file, "r") as f:
                    process_stream(f)
        else:
            process_stream(sys.stdin)
    finally:
        if proc:
            proc.stdin.close()
            proc.wait()

if __name__ == "__main__":
    main()
