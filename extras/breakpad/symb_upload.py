#! /usr/bin/env python3
import os
import sys
import argparse
import subprocess
import logging
import requests
import io
import shutil
import typing

class Dumper:
    def __init__(self, strip_path: str = None):
        self.strip_path = strip_path

    def can_process(self, fpath: str):
        return False

    def dump(self, fpath: str):
        assert(False)

    def _preparse_dump(self, source: str):
        meta = {}
        dest = io.StringIO()
        if  not source.startswith("MODULE"):
            logging.error("file doesn't starst with MODULE")
            return None, None
        for line in source.split("\n"):
            if line.startswith("MODULE"):
                #MODULE <os> <arch> <buildid> <filename>
                line_split = line.split(" ")
                if len(line_split) != 5:
                    logging.error("malformed MODULE entry")
                    return None, None
                _, _os, cpu, buildid, filename  =  line_split
                if filename.endswith(".dbg"):
                    filename = filename[:-4]
                meta["os"] = _os
                meta["cpu"] = cpu
                meta["debug_file"] = filename
                meta["code_file"] = filename
                #see CompactIdentifier in symbol_upload.cc
                meta["debug_identifier"] = buildid.replace("-", "")
                dest.write("MODULE {} {} {} {}".format(_os, cpu, buildid, filename))
                dest.write("\n")
            elif line.startswith("FILE"):
                #FILE <LINE> <PATH>
                _, line, *path_split =  line.split(" ")
                path = " ".join(path_split)
                path = os.path.normpath(path)

                if self.strip_path and path.startswith(self.strip_path):
                    path = os.path.relpath(path, self.strip_path)

                dest.write("FILE {} {}\n".format(line, path))
            else:
                dest.write(line)
                dest.write("\n")
        dest.seek(0)
        return meta, dest

class WindowDumper(Dumper):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

    def can_process(self, fpath: str):
        return any(fpath.endswith(ext) for ext in ["dbg", "dll", "exe"])

    def dump(self, fpath: str):
        proc = subprocess.run(
            ["dump_syms_win", "-r", fpath],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        if  proc.returncode != 0:
            logging.error("unable to extract symbols from {}".format(fpath))
            logging.error(proc.stderr)
            return None, None
        return self._preparse_dump(proc.stdout.decode("utf8"))

class MacDumper(Dumper):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

    # Helper to check Mach-O header
    def is_mach_o(self, fpath: str):
        file = open(fpath, "rb")
        header = file.read(4)
        file.close()
        # MH_MAGIC
        if b'\xFE\xED\xFA\xCE' == header:
            return True
        # MH_CIGAM
        elif b'\xCE\xFA\xED\xFE' == header:
            return True
        # MH_MAGIC_64
        elif b'\xFE\xED\xFA\xCF' == header:
            return True
        # MH_CIGAM_64
        elif b'\xCF\xFA\xED\xFE' == header:
            return True
        return False

    def can_process(self, fpath: str):
        if fpath.endswith(".dylib") or os.access(fpath, os.X_OK):
            return self.is_mach_o(fpath) and not os.path.islink(fpath)
        return False

    def dump(self, fpath: str):
        dsymbundle = fpath + ".dSYM"
        if os.path.exists(dsymbundle):
            shutil.rmtree(dsymbundle)

        #generate symbols file
        proc = subprocess.run(
            ["dsymutil", fpath],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.PIPE,
            check=True
        )
        if  proc.returncode != 0:
            logging.error("unable to run dsymutil on {}:".format(fpath))
            logging.error(proc.stderr)
            return None, None
        if not os.path.exists(dsymbundle):
            logging.error("No symbols in {}".format(fpath))
            return None, None

        proc = subprocess.run(
            ["dump_syms", "-r", "-g", dsymbundle, fpath],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )

        # Cleanup dsymbundle file
        shutil.rmtree(dsymbundle)

        if  proc.returncode != 0:
            logging.error("unable to extract symbols from {}:".format(fpath))
            logging.error(proc.stderr)
            return None, None

        return self._preparse_dump(proc.stdout.decode("utf8"))


class OutputStore:
    def store(self, dump: typing.io.TextIO, meta):
        assert(False)

class HTTPOutputStore(OutputStore):
    def __init__(self, url : str, version = None, prod = None):
        super().__init__()
        self.url = url
        self.extra_args = {}
        if version:
            self.extra_args["ver"] = version
        if prod:
            self.extra_args["prod"] = prod

    def store(self, dump: typing.io.TextIO, meta):
        post_args = {**meta, **self.extra_args}
        r = requests.post(self.url, post_args, files={"symfile": dump})
        if not r.ok:
            logging.error("Unable to perform request, ret {}".format(r.status_code))
            r.raise_for_status()

class LocalDirOutputStore(OutputStore):
    def __init__(self, rootdir: str):
        super().__init__()
        self.rootdir = rootdir

    def store(self, dump: typing.io, meta):
        basepath = os.path.join(self.rootdir, meta["debug_file"], meta["debug_identifier"])
        if not os.path.exists(basepath):
            os.makedirs(basepath)
        with open(os.path.join(basepath, meta["debug_file"] + ".sym"), "w+") as fd:
            shutil.copyfileobj(dump, fd)

def process_dir(sourcedir, dumper, store):
    for root, dirnames, filenames, in os.walk(sourcedir):
        for fname in filenames:
            if not dumper.can_process(os.path.join(root, fname)):
                continue
            logging.info("processing {}".format(fname))
            meta, dump = dumper.dump(os.path.join(root, fname))
            if meta is None or dump is None:
                logging.warning("unable to dump {}".format(fname))
                continue
            store.store(dump, meta)


def main():
    parser = argparse.ArgumentParser(description='extract symbols for breakpad and upload or store them')
    parser.add_argument("sourcedir", help="source directory")
    parser.add_argument("--upload-url", metavar="URL", dest="uploadurl", type=str, help="upload url")
    parser.add_argument("--strip-path", metavar="PATH", dest="strippath", type=str, help="strip path prefix")
    parser.add_argument("-p","--platform",metavar="OS", dest="platform",
                        choices=["mac", "linux", "win"], required=True, help="symbol platform (mac, linux, win)")
    parser.add_argument("--output-dir", metavar="DIRECTORY", dest="outdir", type=str, help="output directory")
    parser.add_argument("--version", metavar="VERSION", dest="version", type=str, help="specify symbol version for uploading")
    parser.add_argument("--prod", metavar="PRODUCT", dest="prod", type=str, help="specify product name for uploading")
    parser.add_argument("--log", metavar="LOGLEVEL", dest="log", type=str, help="log level (INFO, WARNING, ERROR)")
    args = parser.parse_args()

    if args.log:
        numeric_level = getattr(logging, args.log.upper(), None)
        if not isinstance(numeric_level, int):
            raise ValueError("Invalid log level: {}".format(loglevel))
        logging.basicConfig(format='%(levelname)s: %(message)s', level=numeric_level)


    if args.platform == "win":
        dumper = WindowDumper(strip_path=args.strippath)
    elif args.platform == "mac":
        dumper = MacDumper(strip_path=args.strippath)
    else:
        logging.error("Dumper {} is not implemented yet".format(args.platform))
        exit(1)

    if args.uploadurl:
        store=HTTPOutputStore(args.uploadurl, version=args.version, prod=args.prod)
    elif args.outdir:
        store=LocalDirOutputStore(args.outdir)
    else:
        logging.error("You must chose either --output-dir or --upload-url")
        exit(1)

    process_dir(args.sourcedir, dumper, store)


if __name__ == "__main__":
    assert(sys.version_info >= (3,5))
    main()
