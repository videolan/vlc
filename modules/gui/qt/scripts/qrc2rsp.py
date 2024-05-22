import xml.etree.ElementTree as ET
import os
import sys
import argparse

def parseQrc(fdin, fdout):
    tree = ET.parse(fdin)
    root = tree.getroot()
    for qresources in root:
        if qresources.tag != "qresource":
            raise RuntimeError("unexpected qrc format")
        prefix = qresources.get("prefix")
        for file in qresources:
            if file.tag != "file":
                raise RuntimeError("unexpected qrc format")
            alias = file.get("alias")
            fdout.write(os.path.join(prefix, alias))
            fdout.write("\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="This program generates a list of files from Qt qrc file, this file may be used as an input for QmlCachegen"
    )
    parser.add_argument("-o", "--output", type=argparse.FileType('w'),
                        default=sys.stdout)
    parser.add_argument("FILES", nargs="*")

    args = parser.parse_args()
    for f in args.FILES:
        with open(f) as fdin:
            parseQrc(fdin, args.output)
