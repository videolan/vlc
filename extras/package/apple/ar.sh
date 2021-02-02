#!/usr/bin/env sh

set -eu
OPTIONS=$1; shift
TARGET=$1; shift

LINKER_OPTIONS=""

if echo "${TARGET}" | grep "_plugin.a\$"; then
    ${CC} ${CFLAGS} ${LDFLAGS} -Wl,-export_dynamic -fvisibility=default -r -o ${TARGET}.partial.o $@
    ${AR} ${OPTIONS} ${TARGET} ${TARGET}.partial.o
else
    ${AR} ${OPTIONS} ${TARGET} $@
fi
