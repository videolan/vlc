#!/usr/bin/env python3
import os, sys, argparse
from collections import OrderedDict

class PkgConfigFile():
    """Representation of a pkg-config file (.pc)"""

    pc_variables = OrderedDict()
    pc_variables_expanded = OrderedDict()

    pc_keywords = OrderedDict()

    def __init__(self, file):
        for line in file:
            self.parse_pc_line(line)

    def parse_pc_line(self, line):
        for i, c in enumerate(line):
            if c == '=':
                # This is a pkg-config variable line
                key = line[:i].strip()
                val = line[(i + 1):].strip()

                # Add unexpanded version of variable
                self.pc_variables.update({ key : val })

                # Add expanded version of variable
                self.pc_variables_expanded.update({ key : self.expand_pc_vars(val) })
                break
            elif c == ':':
                # This is a pkg-config keyword line
                key = line[:i].strip()
                val = line[(i + 1):].strip()

                self.pc_keywords.update({ key : val })
                break

    def expand_pc_vars(self, line):
        for key, val in self.pc_variables_expanded.items():
            line = line.replace('${' + key + '}', val)
        return line

    def get_variable(self, key, expand=True):
        if expand:
            return self.pc_variables_expanded.get(key, None)
        else:
            return self.pc_variables.get(key, None)

    def get_keyword(self, key, expand=True):
        keyword = self.pc_keywords.get(key, None)
        if expand and keyword != None:
            return self.expand_pc_vars(keyword)
        else:
            return keyword

    def set_keyword(self, key, value):
        self.pc_keywords.update({ key : value })

    def write(self, file):
        pc_contents = ''
        # Print variables
        for key, val in self.pc_variables.items():
            pc_contents += key + '=' + val + '\n'
        pc_contents += '\n'
        # Print keywords
        for key, val in self.pc_keywords.items():
            pc_contents += key + ': ' + val + '\n'

        file.write(pc_contents)

def remove_str_fix(text, prefix, suffix):
    start = len(prefix) if text.startswith(prefix) else 0
    end_offset = len(suffix) if text.endswith(suffix) else 0
    end = len(text) - end_offset
    return text[start:end]

def rewrite_abs_to_rel(pc_file):
    linker_args = pc_file.get_keyword('Libs', expand=False)
    if linker_args is None:
        raise KeyError('No "Libs" keyword in input .pc file found!')
    linker_args_list = linker_args.split()

    # Replace absolute library paths with relative ones
    # i.e. /foo/bar/baz.a to -L/foo/bar -lbaz
    lib_paths = []
    out_args = []
    for arg in linker_args_list:
        if arg.startswith('-L'):
            path = arg[2:]
            path = pc_file.expand_pc_vars(path)
            lib_paths.append(path)

        # Filter all absolute static library paths
        if arg.startswith('/') and arg.endswith('.a'):
            lib_path = os.path.dirname(arg)
            lib_filename = os.path.basename(arg)
            # Remove lib prefix and .a suffix
            lib_name = remove_str_fix(lib_filename, 'lib', '.a')
            if lib_path not in lib_paths:
                out_args.append('-L' + lib_path)
                lib_paths.append(lib_path)
            out_args.append('-l' + lib_name)
        else:
            out_args.append(arg)

    linker_args = ' '.join(out_args)
    pc_file.set_keyword('Libs', linker_args)

# Main function
# Do argument parsing and other stuff needed
# for CLI usage here.
def main():
    if not sys.version_info >= (3, 4):
        print("Python version 3.4 or higher required!", file=sys.stderr)
        exit(1)

    # Create main parser
    parser = argparse.ArgumentParser()
    parser.add_argument('-i', '--input', required=True)
    parser.add_argument('-o', '--output')

    args = parser.parse_args()

    # Default to input file (in-place editing) if no output file is given
    args.output = args.output or args.input

    # Read .pc from input file
    input_file = sys.stdin if args.input == '-' else open(args.input, 'r')
    pc_file = PkgConfigFile(input_file)
    if input_file is not sys.stdin:
        input_file.close()

    rewrite_abs_to_rel(pc_file)

    # Write output
    output_file = sys.stdout if args.output == '-' else open(args.output, 'w')
    pc_file.write(output_file)
    if output_file is not sys.stdout:
        output_file.close()

    return 0

if __name__ == "__main__":
    exit(main())
