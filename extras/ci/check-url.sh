#!/usr/bin/env bash
# Copyright (C) Marvin Scholz
#
# License: see COPYING
#
# Check if a given URL exists or not
set -e

# Print error message and terminate script with status 1
# Arguments:
#   Message to print
abort_err()
{
    echo "ERROR: $1" >&2
    exit 1
}

# Return the HTTP status code for a specific URL
# Arguments:
#    URL
# Globals:
#    HTTP_STATUS_CODE
get_http_status()
{
	HTTP_STATUS_CODE=$(curl -s -o /dev/null -L -I -w "%{http_code}" "$1")
}

command -v "curl" >/dev/null 2>&1 || abort_err "cURL was not found!"

if [ $# -eq 0 ]; then
	abort_err "No URL to check provided!"
fi

get_http_status "$1"

if [ "$HTTP_STATUS_CODE" -eq 200 ]; then
	true
else
	abort_err "'$1' returned HTTP Status Code '$HTTP_STATUS_CODE'"
fi