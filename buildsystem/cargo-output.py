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


# This is a wrapper/interceptor script for Cargo (the Rust package manager) that adds
# the ability to specify an output directory for the final compiler artifact
# (archive lib, depfile).
#
# Usage: ./buildsystem/cargo-output.py --output=out/ --depfile NORMAL_CARGO_INVOCATION
# Usage: ./buildsystem/cargo-output.py --output=out/ --depfile /usr/bin/cargo
#          --target=x86_64-unknown-linux-gnu rustc --crate-type=staticlib
#
# The resulting static library will be put in the output directory as well as its depfile
# if asked (--depfile) and available

import os, sys, json, pathlib, shutil, subprocess, argparse

def dir_path(string):
    if os.path.isdir(string):
        return string
    else:
        raise NotADirectoryError(string)

parser = argparse.ArgumentParser()

parser.add_argument("--depfile", action="store_true")
parser.add_argument("-o", "--output", type=dir_path)
parser.add_argument("cargo_cmds", nargs=argparse.REMAINDER)

args = parser.parse_args()

cargo_argv = args.cargo_cmds

# Insert Cargo argument `--message-format=json-render-diagnostics`
# before a `--` (if there are any) to still be in Cargo arguments
# and not in the inner thing (rustc, ...)
cargo_argv.insert(
    cargo_argv.index('--') if '--' in cargo_argv else len(cargo_argv),
    "--message-format=json-render-diagnostics"
)

# Execute the cargo build and redirect stdout (and not stderr)
cargo_r = subprocess.run(cargo_argv, stdout=subprocess.PIPE)

# We don't use `check=True` in the above `run` method call because it
# raise an execption and outputing a python traceback isn't useful at all
if cargo_r.returncode != 0:
    sys.exit(cargo_r.returncode)

# Get the jsons output
cargo_stdout = cargo_r.stdout.decode('utf-8')
cargo_jsons = [json.loads(line) for line in cargo_stdout.splitlines()]

# We are only interrested in the final artifact (ie the output, not it's deps)
last_compiler_artifact = next(
        j for j in reversed(cargo_jsons) if j["reason"] == "compiler-artifact")

# We only take the first one, because the other one are just aliases and thus
# are not relevant for us
first_compiler_artifact_filename = last_compiler_artifact["filenames"][0]

for filename in [first_compiler_artifact_filename]:
    shutil.copy2(filename, args.output)
    if args.depfile:
        shutil.copy2(pathlib.Path(filename).with_suffix(".d"), args.output)
