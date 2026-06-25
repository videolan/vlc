import os
from pathlib import Path
import sys
import argparse
import glob
import subprocess

def collectJson(mocpath, jsondir, output):
    path = str(Path(jsondir).resolve())

    # https://stackoverflow.com/questions/18394147/how-to-do-a-recursive-sub-folder-search-and-return-files-in-a-list:
    files = glob.glob(path + '/**/*.moc*.json', recursive=True)

    outputbasename = os.path.basename(output)

    filtered = []
    for f in files:
        if os.path.basename(f) != outputbasename:
            filtered.append(f)
    files = filtered

    if not files:
        print(f"warning: no input files found under '{path}', output {output} will be an empty json file", file=sys.stderr)
        Path(output).write_text("[]\n")
        return 0

    moc = [mocpath, "--collect-json", "-o", output, *files]

    try:
        result = subprocess.run(moc)
    except FileNotFoundError:
        print(f"error: could not run the provided moc with path {mocpath}", file=sys.stderr)
        return 1

    if result.returncode != 0:
        print(f"error: moc --collect-json failed with exit code {result.returncode} for output: {output} and input directory: {jsondir}", file=sys.stderr)

    return result.returncode

if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description='This program collects json through moc. Input directory is recursively searched for *.moc*.json files, excluding the ones that match the output file name. If input directory is not specified, the current directory is used.'
    )
    parser.add_argument("-o", "--output", help="output collected json file, if empty 'metatypes.moc.json' is used.")
    parser.add_argument("--moc", help="path for moc, if empty it will be sourced from the PATH.")
    parser.add_argument("--metatypejsondir", help="directory that will be recursively searched for *.moc*.json files for collecting meta type info.")
    args = parser.parse_args()

    if not args.output:
        args.output = "metatypes.moc.json"

    if not args.metatypejsondir:
        args.metatypejsondir = "."

    if not args.moc:
        args.moc = "moc"

    #ensure outdir exists
    outdir = os.path.dirname(args.output)
    if outdir:
        os.makedirs(outdir, exist_ok=True)

    ret = collectJson(args.moc, args.metatypejsondir, args.output)
    sys.exit(ret)
