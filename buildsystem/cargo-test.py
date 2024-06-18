#!/usr/bin/python3
# SPDX-License-Identifier: LGPL-2.1-or-later
# Copyright (C) 2024 Alexandre Janniaux <ajanni@videolabs.io>
#
# Helper script to implement a cargo-based automake test driver.
# See modules/Makefile.am and included Makefile for usage.

import os, sys
import argparse

parser = argparse.ArgumentParser(
    prog='cargo-test',
    description='Automake test driver for cargo-based tests')

parser.add_argument('--test-name')
parser.add_argument('--log-file')
parser.add_argument('--trs-file')
parser.add_argument('--color-tests')
parser.add_argument('--expect-failure')
parser.add_argument('--enable-hard-errors')
parser.add_argument('--working-directory', default=os.getcwd())

args = []
other_args = None
for arg in sys.argv[1:]:
    if other_args is not None:
        other_args.append(arg)
        continue
    if arg == "--":
        other_args = []
        continue
    args.append(arg)
args = parser.parse_args(args)

test_name = args.test_name
if test_name.endswith('.cargo'):
    test_name = test_name[:-len('.cargo')]

# TODO: handle also additional parameter to select sub-tests
# test_name = "::".join(args.test_name.split('.')[1:-1])

import subprocess, os
cmd = ['cargo', 'test', 
       '--offline', '--locked',
       '--color', 'always' if args.color_tests == 'yes' else 'never',
       '--package', test_name,
       '--',
       '-Z', 'unstable-options',
       '--format=json', '--show-output']

out = subprocess.run(
    cmd,
    cwd=args.working_directory,
    capture_output=True,
    text=True,
    close_fds=False, # Necessary for jobserver
    env=os.environ | {
        'DYLD_LIBRARY_PATH': os.environ['top_builddir'] + '/src/.libs',
        'LD_LIBRARY_PATH': os.environ['top_builddir'] + '/src/.libs',
        }
    )

sys.stderr.write(str(out.stderr))

import json
log_file = open(args.log_file, 'wt')
trs_file = open(args.trs_file, 'wt')
for line in out.stdout.splitlines():
    meta = json.loads(line)
    if meta['type'] == 'test' and \
       meta['event'] in ['ok', 'failed']:
        result = 'PASS' if meta['event'] == 'ok' else 'FAIL'

        PASS = '\033[92m'
        FAIL = '\033[91m'
        ENDC = '\033[0m'
        color = PASS if meta['event'] == 'ok' else FAIL
        if args.color_tests == 'yes':
            print('{}{}{}: {}'.format(color, result, ENDC, meta['name']))
        else:
            print('{}: {}'.format(result, meta['name']))
        trs_file.write(':test-result: {} {}\n'.format(result, meta['name']))
        log_file.write('test: {}\n{}\n\n'.format(meta['name'], meta.get('stdout', '')))
log_file.close()

# TODO: define :global-test-result: correctly
trs_file.write(':global-test-result: {}\n'.format('PASS'))

# TODO: define :recheck: correctly
trs_file.write(':recheck: no\n')

trs_file.write(':copy-in-global-log: no\n')
trs_file.close()
