#!/usr/bin/env python3

# SPDX-License-Identifier: ISC
# Copyright © 2026 VideoLabs, VLC authors and VideoLAN
#
# Authors: Steve Lhomme <robux4@videolabs.io>
#
# Generate a C file with the CONFIGURE_LINE defined via the first argument

import argparse

parser = argparse.ArgumentParser()
parser.add_argument("build_config", type=str)
args = parser.parse_args()

output_str = '/* Automatically generated file - DO NOT EDIT */\n\n'
output_str += 'const char CONFIGURE_LINE[] =\n"'
output_str += args.build_config.replace('/\'','\'').replace('"', '\\"')
output_str += '";\n\n'

print(output_str)
