#!/usr/bin/env sh

set -eu
OPTIONS=$1; shift
TARGET=$1; shift

if echo "${TARGET}" | grep "_plugin.a\$"; then
    ${CC} ${CFLAGS} ${LDFLAGS} -r -o ${TARGET}.partial.o $@
    ${AR} ${OPTIONS} ${TARGET} ${TARGET}.partial.o
else
    ${AR} ${OPTIONS} ${TARGET} $@
fi
