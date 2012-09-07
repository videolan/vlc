#!/usr/bin/python
#####################################################################
#             DO WHAT THE FUCK YOU WANT TO PUBLIC LICENSE
#                    Version 2, December 2004
#
# Copyright (C) 2011-2012 Ludovic Fauvet <etix@videolan.org>
#                         Jean-Baptiste Kempf <jb@videolan.org>
#
# Everyone is permitted to copy and distribute verbatim or modified
# copies of this license document, and changing it is allowed as long
# as the name is changed.
#
#            DO WHAT THE FUCK YOU WANT TO PUBLIC LICENSE
#   TERMS AND CONDITIONS FOR COPYING, DISTRIBUTION AND MODIFICATION
#
#  0. You just DO WHAT THE FUCK YOU WANT TO.
#####################################################################
#
# This script can be started in two ways:
# - Without any arguments:
#   The script will search for stacktrace in the WORKDIR, process
#   them and dispatch them in their respective subdirectories.
# - With a stacktrace as only argument:
#   The script will write the output on stdout and exit immediately
#   after the stacktrace has been processed.
#   The input file will stay in place, untouched.
#
# NOTE: Due to a bug in the mingw32-binutils > 2.19 the section
#       .gnu_debuglink in the binary file is trimmed thus preventing
#       gdb to find the associated symbols. This script will
#       work around this issue and rerun gdb for each dbg file.
#
#####################################################################

VLC_VERSION         = "2.0.3"
VLC_BIN             = "/home/videolan/vlc/" + VLC_VERSION + "/vlc-" VLC_VERSION + "/vlc.exe"
VLC_BASE_DIR        = "/home/videolan/vlc/" + VLC_VERSION + "/vlc-" + VLC_VERSION + "/"
VLC_SYMBOLS_DIR     = "/home/videolan/vlc/" + VLC_VERSION + "/symbols-" + VLC_VERSION + "/"
WORKDIR             = "/srv/ftp/crashes-win32"
FILE_MATCH          = r"^\d{14}$"
FILE_MAX_SIZE       = 10000
GDB_CMD             = "gdb --exec=%(VLC_BIN)s --symbols=%(VLC_SYMBOLS_DIR)s%(DBG_FILE)s.dbg --batch -x %(BATCH_FILE)s"

EMAIL_TO            = "bugreporter -- videolan.org"
EMAIL_FROM          = "crashes@crash.videolan.org"
EMAIL_SUBJECT       = "[CRASH] New Win32 crash report"
EMAIL_BODY          = \
"""
Dear Bug Squasher,

This crash has been reported automatically and might be incomplete and/or broken.
Windows version: %(WIN32_VERSION)s

%(STACKTRACE)s

Truly yours,
a python script.
"""

import os, sys, re, tempfile
import string, shlex, subprocess
import smtplib, datetime, shutil
import traceback
from email.mime.text import MIMEText


def processFile(filename):
    print "Processing " + filename
    global win32_version

    f = open(filename, 'r')
    # Read (and repair) the input file
    content = "".join(filter(lambda x: x in string.printable, f.read()))
    f.close()

    if os.path.getsize(filename) < 10:
        print("File empty")
        os.remove(filename)
        return

    # Check if VLC version match
    if not isValidVersion(content):
        print("Invalid VLC version")
        moveFile(filename, outdated = True)
        return

    # Get Windows version
    win32_version = getWinVersion(content) or 'unknown'

    # Map eip <--> library
    mapping = mapLibraries(content)
    if not mapping:
        print("Stacktrace not found")
        os.remove(filename)
        return

    # Associate all eip to their respective lib
    # lib1
    #     `- 0x6904f020
    #      - 0x6927d37c
    # lib2
    #     `- 0x7e418734
    #      - 0x7e418816
    #      - 0x7e42bf15
    sortedEIP,delta_libs = sortEIP(content,mapping)
    # Compute the stacktrace using GDB
    eipmap = findSymbols(sortedEIP)
    # Generate the body of the email
    body = genEmailBody(mapping, eipmap, delta_libs)
    # Send the email
    sendEmail(body)
    # Print the output
    print(body)

    # Finally archive the stacktrace
    moveFile(filename, outdated = False)

def isValidVersion(content):
    pattern = re.compile(r"^VLC=%s " % VLC_VERSION, re.MULTILINE)
    res = pattern.search(content)
    return True if res else False

def getWinVersion(content):
    pattern = re.compile(r"^OS=(.*)$", re.MULTILINE)
    res = pattern.search(content)
    if res is not None:
        return res.group(1)
    return None

def getDiffAddress(content, name):
    plugin_name_section = content.find(name)
    if plugin_name_section < 0:
        return None

    begin_index = content.rfind("\n", 0, plugin_name_section) + 1
    end_index = content.find("|", begin_index)

    tmp_index = name.rfind('plugins\\')
    libname = name[tmp_index :].replace("\\", "/")
    full_path = VLC_BASE_DIR + libname

    if not os.path.isfile(full_path):
        return None

    cmd = "objdump -p " + full_path + " |grep ImageBase -|cut -f2-"
    p = subprocess.Popen(cmd, stdout=subprocess.PIPE, shell=True).stdout.read().strip()

    diff = int(content[begin_index:end_index], 16) - int(p, 16)
    return diff

def mapLibraries(content):
    stacktrace_section = content.find("[stacktrace]")
    if stacktrace_section < 0:
        return None

    stacklines = content[stacktrace_section:]
    stacklines = stacklines.splitlines()
    pattern = re.compile(r"^([0-9a-fA-F]+)\|(.+)$")

    mapping = []
    for line in stacklines:
        m = pattern.match(line)
        print(line)
        if m is not None:
            mapping.append(m.group(1, 2))

    if len(mapping) == 0:
        return None
    return mapping


def sortEIP(content, mapping):
    # Merge all EIP mapping to the same library
    libs = {}
    libs_address = {}
    for item in mapping:
        # Extract the library name (without the full path)
        index = item[1].rfind('\\')
        libname = item[1][index + 1:]

        # Append the eip to its respective lib
        if libname not in libs:
            libs[libname] = []
            diff = getDiffAddress(content, item[1])
            if diff is not None:
                libs_address[libname] = diff
            else:
                libs_address[libname] = 0

        libs[libname].append(int(item[0],16) - libs_address[libname])

    return libs,libs_address


def findSymbols(sortedEIP):
    eipmap = {}

    for k, v in sortedEIP.items():
        # Create the gdb batchfile
        batchfile = tempfile.NamedTemporaryFile(mode="w")
        batchfile.write("set print symbol-filename on\n")

        # Append all eip for this lib
        for eip in v:
            batchfile.write('p/a %s\n' % hex(eip))
        batchfile.flush()

        # Generate the command line
        cmd = GDB_CMD % {"VLC_BIN": VLC_BIN, "VLC_SYMBOLS_DIR": VLC_SYMBOLS_DIR, "DBG_FILE": k, "BATCH_FILE": batchfile.name}
        args = shlex.split(cmd)

        # Start GDB and get result
        p = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

        # Parse result
        gdb_pattern = re.compile(r"^\$\d+ = (.+)$")
        cnt = 0
        while p.poll() == None:
            o = p.stdout.readline()
            if o != b'':
                o = bytes.decode(o)
                m = gdb_pattern.match(o)
                if m is not None:
                    #print("LINE: [%s]" % m.group(1))
                    eipmap[v[cnt]] = m.group(1)
                    cnt += 1
        batchfile.close()
    return eipmap


def genEmailBody(mapping, eipmap, delta_libs):
    stacktrace = ""
    cnt = 0
    for item in mapping:
        index = item[1].rfind('\\')
        libname = item[1][index + 1:]
        print(int(item[0],16), delta_libs[libname])
        #print(eipmap)
        #print(mapping)
        stacktrace += "%d. %s [in %s]\n" % (cnt, eipmap[int(item[0],16)-delta_libs[libname]], item[1])
        cnt += 1
    stacktrace = stacktrace.rstrip('\n')
    return EMAIL_BODY % {"STACKTRACE": stacktrace, "WIN32_VERSION": win32_version}


def sendEmail(body):
    msg = MIMEText(body)
    msg['Subject'] = EMAIL_SUBJECT
    msg['From'] = EMAIL_FROM
    msg['To'] = EMAIL_TO

    # Send the email
    s = smtplib.SMTP()
    s.connect("127.0.0.1")
    s.sendmail(EMAIL_FROM, [EMAIL_TO], msg.as_string())
    s.quit()

def moveFile(filename, outdated = False):
    today = datetime.datetime.now().strftime("%Y%m%d")
    today_path = "%s/%s" % (WORKDIR, today)
    if not os.path.isdir(today_path):
        os.mkdir(today_path)
    if not outdated:
        shutil.move(filename, "%s/%s" % (today_path, os.path.basename(filename)))
    else:
        outdated_path = "%s/outdated/" % today_path
        if not os.path.isdir(outdated_path):
            os.mkdir(outdated_path)
        shutil.move(filename, "%s/%s" % (outdated_path, os.path.basename(filename)))


### ENTRY POINT ###

batch = len(sys.argv) != 2
if batch:
    print("Running in batch mode")

input_files = []
if not batch:
    if not os.path.isfile(sys.argv[1]):
        exit("file does not exists")
    input_files.append(sys.argv[1])
else:
    file_pattern = re.compile(FILE_MATCH)
    entries = os.listdir(WORKDIR)
    for entry in entries:
        path_entry = WORKDIR + "/" + entry
        if not os.path.isfile(path_entry):
            continue
        if not file_pattern.match(entry):
            print(entry)
            os.remove(path_entry)
            continue
        if os.path.getsize(path_entry) > FILE_MAX_SIZE:
            print("%s is too big" % entry)
            os.remove(path_entry)
            continue
        input_files.append(path_entry)

if not len(input_files):
    exit("Nothing to process")

# Start processing each file
for input_file in input_files:
    try:
        processFile(input_file)
    except Exception as ex:
        print(traceback.format_exc())
