#!/usr/bin/env python3
import argparse

# Argument parsing
parser = argparse.ArgumentParser(description="Generate Qt configure options from the compilation variables")
parser.add_argument('-D', action='append', help='compiler definition')
parser.add_argument('-I', action='append', help='include directory')
parser.add_argument('-L', action='append', help='linker directory')
# parser.add_argument('-F', action='append', help='framework flags')
args, remaining = parser.parse_known_args()

all_params = []
if args.D:
    all_params += ['-D ' + sub for sub in args.D]
if args.I:
    all_params += ['-I ' + sub for sub in args.I]
if args.L:
    all_params += ['-L ' + sub for sub in args.L]

if all_params:
    print(' '.join(all_params))
else:
    print('')
