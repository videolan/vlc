#!/bin/sh
# SPDX-License-Identifier: LGPL-2.1-or-later
# Copyright (C) 2021-2026 Alexandre Janniaux <ajanni@videolabs.io>
#
# Extract static or forwarded dependencies from a libtool .la archive.
#
# Usage: dependencies.sh <mode> <file.la>
#
# Modes:
#   static    print static archive paths (.a) needed for partial linking
#   forward   print flags (-l, .la deps) to defer to the final link stage
set -eu

MODE=$1
LTFILE=$2

# Parse a field value from a libtool .la archive, stripping quotes.
# Usage: parse_la_field <file.la> <field_name>
parse_la_field()
{
    _value=$(grep "$2=" "$1" | cut -d= -f2-)
    _value="${_value%\'}"; _value="${_value#\'}"
    echo "$_value"
}

# Collect -L directory paths from a dependency string.
# Usage: collect_library_path <deps>
collect_library_path()
{
    _lp=""
    for _dep in $1; do
        case "$_dep" in -L*)
            _lp="$_lp ${_dep#-L}" ;;
        esac
    done
    echo "$_lp"
}

# Resolve a -l flag to a static archive using LIBRARY_PATH search.
# Prints resolved path(s) to stdout. Returns 1 if unresolvable.
# Usage: resolve_lflag <-lfoo> <library_path>
resolve_lflag()
{
    _libname="${1#-l}"
    for _rpath in $2; do
        _base="$_rpath/lib${_libname}"
        if [ -f "${_base}.a" ]; then
            echo "${_base}.a"
            return 0
        elif [ -f "${_base}.la" ]; then
            if grep -q installed=yes "${_base}.la"; then
                extract_dependencies "${_base}.la"
                return 0
            fi
        fi
    done
    return 1
}

# Process a .la dependency: resolve its static archive path and recurse.
# Sets _STATIC and _FORWARD for the caller to append.
# Usage: process_la_dep <file.la>
process_la_dep()
{
    _STATIC="" ; _FORWARD=""
    _ladep="$1"

    # Only installed static archives (contribs) should be
    # merged. Build-tree libraries (installed=no with a
    # libdir) like libvlccore.la, libvlc_pulse.la will be
    # available at final link so that the lib global state
    # is preserved for in-tree and out-of-tree plugins.
    _installed=$(parse_la_field "$_ladep" installed)
    _libdir_field=$(parse_la_field "$_ladep" libdir)
    if [ "$_installed" = "no" ] && [ -n "$_libdir_field" ]; then
        _FORWARD="$_ladep"
        return
    fi

    _old_lib=$(parse_la_field "$_ladep" old_library)

    if [ -n "$_old_lib" ]; then
        _libdir=$(dirname "$_ladep")
        # Check if the archive is in .libs, which could signal
        # a non-installed library
        if [ -f "$_libdir/.libs/$_old_lib" ]; then
            _libdir="$_libdir/.libs"
        fi
        case "$MODE" in
            static)  _STATIC="$_libdir/$_old_lib $(extract_dependencies "$_ladep")" ;;
            forward) _FORWARD="$(extract_dependencies "$_ladep")" ;;
        esac
    else
        # Shared-only library (no static archive): forward -l flag
        _ln=$(basename "$_ladep" .la)
        _ln="${_ln#lib}"
        _FORWARD="-l$_ln"
    fi
}

# Recursively extract dependencies from a .la file.
# Prints the resolved dependency list for the current MODE.
extract_dependencies()
{
    _deps=$(parse_la_field "$1" dependency_libs)
    _lib_path=$(collect_library_path "$_deps")

    STATIC_DEPENDENCIES=""
    FORWARD_DEPENDENCIES=""

    for _dep in $_deps; do
        case "$_dep" in
            -L*) ;; # already handled by collect_library_path
            -l*)
                if _resolved=$(resolve_lflag "$_dep" "$_lib_path"); then
                    STATIC_DEPENDENCIES="$STATIC_DEPENDENCIES $_resolved"
                else
                    FORWARD_DEPENDENCIES="$FORWARD_DEPENDENCIES $_dep"
                fi
                ;;
            *.a)
                STATIC_DEPENDENCIES="$STATIC_DEPENDENCIES $_dep"
                ;;
            *.la)
                process_la_dep "$_dep"
                STATIC_DEPENDENCIES="$STATIC_DEPENDENCIES $_STATIC"
                FORWARD_DEPENDENCIES="$FORWARD_DEPENDENCIES $_FORWARD"
                ;;
        esac
    done

    case "$MODE" in
        forward) echo $FORWARD_DEPENDENCIES ;;
        static)  echo $STATIC_DEPENDENCIES ;;
    esac
}

extract_dependencies "$LTFILE"
