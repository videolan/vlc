#!/usr/bin/env python3
"""
Embeds VLC QML source files as Qt resources so Qt6 can find them at
qrc:/qt/qml/VLC/<Module>/<file>.qml. This is required because the
*_qmlassets.cpp files only embed the qmldir, not the .qml sources.
"""

import argparse
import os
import re
import subprocess
import sys
import tempfile
import textwrap

_DEFAULT_TOOLCHAIN = os.path.expanduser(
    '~/vlc-webos-build/sdk/arm-webos-linux-gnueabi_sdk-buildroot')
_DEFAULT_TARGET = 'arm-webos-linux-gnueabi'

parser = argparse.ArgumentParser(
    description='Embed VLC QML source files as Qt resources inside libqt_plugin.so')
parser.add_argument('--build-dir',
    default=os.path.expanduser('~/vlc-webos-build/modules/gui/qt'),
    help='Path to the modules/gui/qt build directory')
parser.add_argument('--src-dir',
    default='/mnt/c/Repos/vlc',
    help='VLC source root (used to locate .qml source files)')
parser.add_argument('--toolchain',
    default=_DEFAULT_TOOLCHAIN,
    help='Path to the ARM cross-compilation SDK/toolchain')
parser.add_argument('--target',
    default=_DEFAULT_TARGET,
    help='GNU cross-compilation target triple (default: arm-webos-linux-gnueabi)')
args = parser.parse_args()

BUILD_DIR = args.build_dir
VLC_SRC = os.path.join(args.src_dir, 'modules', 'gui', 'qt')
RCC = os.path.join(args.toolchain, 'bin', 'rcc')
CXX = os.path.join(args.toolchain, 'bin', args.target + '-g++')
SYSROOT = os.path.join(args.toolchain, args.target, 'sysroot')

# Module name → VLC module subdirectory name mapping
MODULE_TO_SUBDIR = {
    "dialogs": "dialogs",
    "maininterface": "maininterface",
    "medialibrary": "medialibrary",
    "menus": "menus",
    "network": "network",
    "player": "player",
    "playercontrols": "player",  # playercontrols are under player/
    "playlist": "playlist",
    "style": "style",
    "util": "util",
    "widgets": "widgets",
}

# Qt resource prefix → module name mapping (from qmldir contents)
MODULE_TO_QT_NAME = {
    "dialogs": "Dialogs",
    "maininterface": "MainInterface",
    "medialibrary": "MediaLibrary",  # capitalize properly
    "menus": "Menus",
    "network": "Network",
    "player": "Player",
    "playercontrols": "PlayerControls",
    "playlist": "Playlist",
    "style": "Style",
    "util": "Util",
    "widgets": "Widgets",
}


def get_module_qt_name(module):
    """Get the Qt module name (used in qrc path) from cache loader."""
    cache_loader = os.path.join(BUILD_DIR, f"{module}_qmlcache_loader.cpp")
    if not os.path.exists(cache_loader):
        return MODULE_TO_QT_NAME.get(module, module.capitalize())
    with open(cache_loader) as f:
        content = f.read()
    # Extract from paths like "/qt/qml/VLC/MainInterface/BannerSources.qml"
    m = re.search(r'"/qt/qml/VLC/([^/]+)/', content)
    if m:
        return m.group(1)
    return MODULE_TO_QT_NAME.get(module, module.capitalize())


def get_module_qml_files(module):
    """Get list of .qml filenames for a module from the cache loader."""
    cache_loader = os.path.join(BUILD_DIR, f"{module}_qmlcache_loader.cpp")
    if not os.path.exists(cache_loader):
        return []
    with open(cache_loader) as f:
        content = f.read()
    # Extract filenames from paths like "/qt/qml/VLC/MainInterface/BannerSources.qml"
    qt_name = get_module_qt_name(module)
    pattern = rf'"/qt/qml/VLC/{re.escape(qt_name)}/([^"]+\.qml)"'
    return re.findall(pattern, content)


def find_qml_file(qml_name, module):
    """Find a .qml source file in the VLC source tree."""
    subdir = MODULE_TO_SUBDIR.get(module, module)
    search_root = os.path.join(VLC_SRC, subdir)
    for dirpath, _, files in os.walk(search_root):
        if qml_name in files:
            return os.path.join(dirpath, qml_name)
    # Fallback: search entire qt source
    for dirpath, _, files in os.walk(VLC_SRC):
        if qml_name in files:
            return os.path.join(dirpath, qml_name)
    return None


def generate_qrc(module, qt_name, qml_files_map):
    """Generate a .qrc file content for the module."""
    lines = ['<!DOCTYPE RCC>', '<RCC version="1.0">',
             f'  <qresource prefix="/qt/qml/VLC/{qt_name}">']
    for qml_name, src_path in qml_files_map.items():
        lines.append(f'    <file alias="{qml_name}">{src_path}</file>')
    lines += ['  </qresource>', '</RCC>']
    return '\n'.join(lines)


def run(cmd, **kwargs):
    print(f"  $ {' '.join(cmd)}")
    result = subprocess.run(cmd, capture_output=True, text=True, **kwargs)
    if result.returncode != 0:
        print(f"  STDERR: {result.stderr[:500]}")
        raise RuntimeError(f"Command failed: {' '.join(cmd)}")
    return result


def main():
    modules = []
    for f in sorted(os.listdir(BUILD_DIR)):
        m = re.match(r'^(.+)_qmlcache_loader\.cpp$', f)
        if m:
            modules.append(m.group(1))

    print(f"Found {len(modules)} QML modules: {', '.join(modules)}")

    all_qrc_entries = {}  # path -> src_file for one combined resource

    for module in modules:
        qt_name = get_module_qt_name(module)
        qml_files = get_module_qml_files(module)
        print(f"\nModule: {module} (Qt name: {qt_name}), {len(qml_files)} files")

        found_map = {}
        for qml_name in qml_files:
            src = find_qml_file(qml_name, module)
            if src:
                print(f"  FOUND: {qml_name} -> {src}")
                found_map[qml_name] = src
                all_qrc_entries[f"/qt/qml/VLC/{qt_name}/{qml_name}"] = src
            else:
                print(f"  MISSING: {qml_name}")

    print(f"\nTotal QML files found: {len(all_qrc_entries)}")

    if not all_qrc_entries:
        print("ERROR: No QML files found!")
        sys.exit(1)

    # Generate one combined .qrc file organized by module prefix
    # Group by prefix
    prefixes = {}
    for qrc_path, src in all_qrc_entries.items():
        parts = qrc_path.rsplit('/', 1)
        prefix = parts[0]  # e.g. "/qt/qml/VLC/MainInterface"
        fname = parts[1]
        if prefix not in prefixes:
            prefixes[prefix] = []
        prefixes[prefix].append((fname, src))

    qrc_lines = ['<!DOCTYPE RCC>', '<RCC version="1.0">']
    for prefix in sorted(prefixes.keys()):
        qrc_lines.append(f'  <qresource prefix="{prefix}">')
        for fname, src in sorted(prefixes[prefix]):
            qrc_lines.append(f'    <file alias="{fname}">{src}</file>')
        qrc_lines.append('  </qresource>')
    qrc_lines.append('</RCC>')
    qrc_content = '\n'.join(qrc_lines)

    qrc_file = os.path.join(BUILD_DIR, 'vlc_qml_sources.qrc')
    cpp_file = os.path.join(BUILD_DIR, 'vlc_qml_sources.cpp')
    obj_file = os.path.join(BUILD_DIR, '.libs/libqt_plugin_la-vlc_qml_sources.o')

    with open(qrc_file, 'w') as f:
        f.write(qrc_content)
    print(f"\nWrote QRC: {qrc_file}")

    # Run rcc to generate C++ resource file
    print(f"Running rcc...")
    run([RCC, '-name', 'vlc_qml_sources', '-o', cpp_file, qrc_file])
    print(f"Generated: {cpp_file}")

    # Locate Qt include directory: prefer WEBOS_QT6_TARGET_PREFIX from env,
    # then sysroot/usr, then the legacy ~/qt6-webos/target/usr location.
    _qt6_prefix = os.environ.get('WEBOS_QT6_TARGET_PREFIX', '')
    _candidates = [
        _qt6_prefix,
        os.path.join(SYSROOT, 'usr'),
        os.path.expanduser('~/qt6-webos/target/usr'),
    ]
    _qt_include = None
    for _c in _candidates:
        if _c and os.path.isdir(os.path.join(_c, 'include', 'QtCore')):
            _qt_include = os.path.join(_c, 'include')
            break
    if _qt_include is None:
        print("ERROR: Cannot find Qt6 include directory. "
              "Set WEBOS_QT6_TARGET_PREFIX or pass --toolchain.", file=sys.stderr)
        sys.exit(1)

    # Cross-compile for ARM
    print(f"Cross-compiling for ARM (Qt includes: {_qt_include})...")
    run([
        CXX,
        f'--sysroot={SYSROOT}',
        '-march=armv7-a', '-mfloat-abi=softfp', '-mfpu=neon',
        '-fPIC', '-DQT_NO_DEBUG',
        '-I', _qt_include,
        '-I', os.path.join(_qt_include, 'QtCore'),
        '-O2', '-fvisibility=hidden',
        '-c', cpp_file,
        '-o', obj_file
    ])
    print(f"Compiled: {obj_file}")

    # Now we need to add this .o to the plugin link
    # The simplest way: add it to the plugin's LIBADD in the Makefile,
    # but since we can't modify the Makefile easily at link time,
    # let's directly relink the plugin with this extra object.

    # Find the existing link command from the .la file
    la_file = os.path.join(BUILD_DIR, 'libqt_plugin.la')
    print(f"\nExtra object ready: {obj_file}")
    print(f"Now re-linking the plugin with the extra QML resources object...")

    # Trigger relink by modifying a dependency
    os.utime(obj_file)

    # We need to manually relink. Let's add the .o to the existing .libs/.o list
    # The cleanest approach: add it to the Qt plugin's LIBADD directly
    print(f"\nDone! To relink, run:")
    print(f"  cd {BUILD_DIR}")
    print(f"  PATH=/usr/bin:/home/gabor/vlc-webos-build/sdk/arm-webos-linux-gnueabi_sdk-buildroot/bin make libqt_plugin.la EXTRA_LDFLAGS='{obj_file}'")
    print(f"\nOr use the manual relink script.")

    return obj_file, cpp_file


if __name__ == '__main__':
    obj_file, cpp_file = main()
    print(f"\nSuccess! QML resources compiled to: {obj_file}")
