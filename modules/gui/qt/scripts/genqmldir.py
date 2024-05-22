import os
from pathlib import Path
import sys
import argparse

def genQmldir(out, files, singletons, module, prefix, version):
    out.write(f"module {module}\n")
    out.write(f"prefer :{prefix}/\n")
    for f in files:
        basename = Path(f).stem
        filename = os.path.basename(f)
        out.write(f"{basename} {version} {filename}\n")

    for f in singletons:
        basename = Path(f).stem
        filename = os.path.basename(f)
        out.write(f"singleton {basename} {version} {filename}\n")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description='This program generates qmldir files to build Qml modules'
    )
    parser.add_argument("-o", "--output", required=True, help="genereated file")
    parser.add_argument("-s", "--srcdir", required=False, default="", help="path to the source directory")
    parser.add_argument("--prefix", default="", help="module prefix path")
    parser.add_argument("--version", default="1.0", help="version of the module")
    parser.add_argument("--module", required=True, help="module name")
    parser.add_argument("--singletons", action='append', default=[], nargs='*', metavar=("CLASS"), help="mark CLASS as a singleton")
    parser.add_argument("--sources", action='append', default=[], nargs='*', metavar=("CLASS"), help="mark CLASS as a singleton")

    args = parser.parse_args()
    prefix = os.path.join("/", args.prefix, *args.module.split("."))
    singletons = [os.path.join(args.srcdir, s) for s in args.singletons[0]]
    files = [os.path.join(args.srcdir, relpath) for relpath in args.sources[0]]

    #ensure outdir exists
    os.makedirs(os.path.dirname(args.output), exist_ok=True)

    with open(args.output, "w+") as fd:
        genQmldir(fd, files, singletons, args.module, prefix,  args.version)
