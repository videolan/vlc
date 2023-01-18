#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later
# Copyright (C) 2022 Loïc Branstett <loic@videolabs.io>
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation; either version 2.1 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.


# This script is meant to find the required native libs of an empty Rust crate
# compiled as a static lib.
#
# Usage: ./buildsystem/cargo-rustc-static-libs.py /usr/bin/cargo
# Usage: ./buildsystem/cargo-rustc-static-libs.py /usr/bin/cargo --target=x86_64-unknown-linux-gnu
#
# The result is then printed to the standard output.

import os, sys, json, subprocess, tempfile, argparse

NATIVE_STATIC_LIBS="native-static-libs"

parser = argparse.ArgumentParser()
parser.add_argument("cargo_cmds", nargs=argparse.REMAINDER)

args = parser.parse_args()

cargo_argv = args.cargo_cmds
cargo_argv.append("rustc")
cargo_argv.append("--message-format=json")
cargo_argv.append("--crate-type=staticlib")
cargo_argv.append("--quiet")
cargo_argv.append("--")
cargo_argv.append("--print=native-static-libs")

with tempfile.TemporaryDirectory() as tmpdir:
    os.chdir(tmpdir)

    with open("Cargo.toml", "w") as cargo_toml:
        cargo_toml.write("""
[package]
name = "native-static-libs"
version = "0.0.0"

[lib]
path = "lib.rs"
""")
    
    with open("lib.rs", "w") as lib_rs:
        lib_rs.write("#![allow(dead_code)] fn main(){}")

    # Execute the cargo build and redirect stdout (and not stderr)
    cargo_r = subprocess.run(cargo_argv, stdout=subprocess.PIPE)

    # We don't use `check=True in run because it raise an execption
    # and outputing a python traceback isn't useful at all.
    #
    # We also exit here so that the output o tmp dir is not cleared when
    # there is an error.
    if cargo_r.returncode != 0:
        print("command: {cargo_argv}", file=sys.stderr)
        print("cwd: {tmpdir}", file=sys.stderr)
        sys.exit(cargo_r.returncode)

# Get the jsons output
cargo_stdout = cargo_r.stdout.decode('utf-8')
cargo_jsons = [json.loads(line) for line in cargo_stdout.splitlines()]

# Print the last message with a `NATIVE_STATIC_LIBS` message
for j in reversed(cargo_jsons):
    if j["reason"] == "compiler-message":
        msg = j["message"]["message"]
        if msg.startswith(NATIVE_STATIC_LIBS):
            print(msg[len(NATIVE_STATIC_LIBS + ": "):])
