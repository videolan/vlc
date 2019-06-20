#!/usr/bin/env python3
import os, re, argparse

parser = argparse.ArgumentParser()
# Input files
parser.add_argument("copying", type=argparse.FileType('r', encoding='UTF-8'))
parser.add_argument("thanks", type=argparse.FileType('r', encoding='UTF-8'))
parser.add_argument("authors", type=argparse.FileType('r', encoding='UTF-8'))
# Output files
parser.add_argument("output", type=argparse.FileType('w', encoding='UTF-8'))
args = parser.parse_args()

# Regex to remove emails in thanks and authors files
email_regex = re.compile(r'<.*.>')

output_str = '/* Automatically generated file - DO NOT EDIT */\n\n'

with args.copying:
    output_str += 'static const char psz_license[] =\n"'
    output_str += args.copying.read().replace('"', '\\"').replace('\r', '').replace('\n', '\\n"\n"')
    output_str += '";\n\n'

with args.thanks:
    output_str += 'static const char psz_thanks[] =\n"'
    output_str += email_regex.sub('', args.thanks.read().replace('"', '\\"').replace('\r', '').replace('\n', '\\n"\n"'))
    output_str += '";\n\n'

with args.authors:
    output_str += 'static const char psz_authors[] =\n"'
    output_str += email_regex.sub('', args.authors.read().replace('"', '\\"').replace('\r', '').replace('\n', '\\n"\n"'))
    output_str += '";\n\n'

with args.output:
    args.output.write(output_str)
