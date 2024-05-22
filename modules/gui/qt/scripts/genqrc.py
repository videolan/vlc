import os
from pathlib import Path
import sys
import argparse

def genQrc(out, files, prefix):
    out.write(f'''<RCC>
  <qresource prefix="{prefix}">
''')
    for f in files:
        alias = os.path.basename(f)
        abspath = os.path.abspath(f)
        out.write(f'    <file alias="{alias}">{abspath}</file>\n')
    out.write('''  </qresource>
</RCC>
''')

if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description='This program generates qrc from a list of resources'
    )
    parser.add_argument("-o", "--output", required=True, help="genereated file")
    parser.add_argument("-s", "--srcdir", required=False, default="", help="path to the source directory")
    parser.add_argument("-p", "--prefix", required=False, default="", help="resource prefix")
    parser.add_argument("--sources", action='append', default=[], nargs='*', metavar=("FILE"), help="file to embed")

    args = parser.parse_args()
    files = [os.path.join(args.srcdir, relpath) for relpath in args.sources[0]]
    prefix = os.path.join("/", args.prefix)

    #ensure outdir exists
    os.makedirs(os.path.dirname(args.output), exist_ok=True)

    with open(args.output, "w+") as fd:
        genQrc(fd, files, prefix)
