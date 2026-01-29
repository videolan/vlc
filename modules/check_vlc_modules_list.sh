#!/bin/sh
# Copyright (C) Alexandre Janniaux <ajanni@videolabs.io>
#
# License: see COPYING
#
## Check that vlc_modules_list points to existing plugins
set -e

# When run from make check, we're in the modules build directory
MODULE_LIST="vlc_modules_list"

if [ ! -f "$MODULE_LIST" ]; then
    echo "ERROR: $MODULE_LIST not found" >&2
    exit 1
fi

errors=0

# Parse vlc_modules_list with format: plugin_name: relative/path/to/plugin.ext
while IFS= read -r line; do
    [ -z "$line" ] && continue

    plugin="${line%%: *}"
    plugin_path="${line#*: }"

    # Path is relative to top build directory, we're in modules/
    full_path="../$plugin_path"

    if [ ! -f "$full_path" ]; then
        echo "MISSING: $plugin -> $full_path" >&2
        errors=$((errors + 1))
    fi
done < "$MODULE_LIST"

if [ $errors -gt 0 ]; then
    echo "ERROR: $errors plugin(s) listed in vlc_modules_list not found" >&2
    exit 1
fi

echo "OK: All plugins in vlc_modules_list exist"
