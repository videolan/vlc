#!/usr/bin/env python3
import argparse
import glob
import os
import re
import sys

parser = argparse.ArgumentParser(
    description='Patch the Qt plugin Makefile to wrap QML module archives in --whole-archive')
parser.add_argument('--build-dir',
    default=os.path.expanduser('~/vlc-webos-build/modules/gui/qt'),
    help='Path to the modules/gui/qt build directory (default: ~/vlc-webos-build/modules/gui/qt)')
parser.add_argument('--src-dir',
    help='VLC source root (unused by this script, accepted for uniformity)')
args = parser.parse_args()

qt_dir = args.build_dir
makefile_path = os.path.join(qt_dir, 'Makefile')

# Step 1: Create .la stubs (shouldnotlink=yes triggers libtool convenience codepath)
archives = sorted(glob.glob(os.path.join(qt_dir, 'libqml_module_*.a')))
print(f'Found {len(archives)} QML archives')
la_stubs = []
for ap in archives:
    a = os.path.basename(ap)
    la = a[:-2] + '.la'
    la_path = os.path.join(qt_dir, la)
    with open(la_path, 'w') as f:
        f.write('dlname=\'\'\n')
        f.write('library_names=\'\'\n')
        f.write(f'old_library=\'{a}\'\n')
        f.write('dependency_libs=\'\'\n')
        f.write('installed=no\n')
        f.write('shouldnotlink=yes\n')
        f.write('libdir=\'\'\n')
    print(f'  Created {la}')
    la_stubs.append(la)

# Step 2: Patch Makefile
with open(makefile_path) as f:
    content = f.read()

changed = False

# Revert LDFLAGS - remove broken --whole-archive from LDFLAGS
old_ldf = 'libqt_plugin_la_LDFLAGS = $(AM_LDFLAGS) $(QT_LDFLAGS) $(am__append_7) -Wl,--whole-archive $(am__append_35) -Wl,--no-whole-archive\n'
new_ldf = 'libqt_plugin_la_LDFLAGS = $(AM_LDFLAGS) $(QT_LDFLAGS) $(am__append_7)\n'
if old_ldf in content:
    content = content.replace(old_ldf, new_ldf, 1)
    print('Reverted LDFLAGS')
    changed = True
elif new_ldf in content:
    print('LDFLAGS already clean')
else:
    print('WARNING: LDFLAGS line not found!', file=sys.stderr)

# Add .la stubs to LIBADD
if 'libqml_module_dialogs.la' not in content:
    m = re.search(r'(libqt_plugin_la_LIBADD\s*=(?:.*\\\n)*.*[^\\\n])\n', content)
    if m:
        addition = ' \\\n\t' + ' \\\n\t'.join(la_stubs)
        content = content.replace(m.group(1), m.group(1) + addition, 1)
        print(f'Added {len(la_stubs)} .la stubs to LIBADD')
        changed = True
    else:
        print('ERROR: LIBADD not found!', file=sys.stderr)
        sys.exit(1)
else:
    print('Already patched')

if changed:
    with open(makefile_path, 'w') as f:
        f.write(content)
    print('Makefile written')

# Verify
print('\nVerification:')
for line in content.split('\n'):
    if 'libqt_plugin_la_LIBADD' in line or 'libqt_plugin_la_LDFLAGS' in line or 'libqml_module' in line:
        print(f'  {line}')
