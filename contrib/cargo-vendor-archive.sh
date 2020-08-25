#!/bin/sh

set -e

usage()
{
cat << EOF
usage: $0 <archive>

Fetch and archive all dependencies from a Rust archive using 'cargo vendor'.
EOF
}

if [ "x$1" = "x" ]; then
    usage
    exit 1
fi

# Setup cargo path
CARGO=
if [ -d "$(dirname $0)/bin/.cargo" ];then
    CARGO_HOME=$(cd $(dirname $0)/bin/.cargo && pwd)
    CARGO="CARGO_HOME=\"${CARGO_HOME}\" \"${CARGO_HOME}/bin/cargo\""
else
    CARGO=cargo
fi

# Extract archive into a tmp dir
TMP_DIR=.tmp-$(basename $1)
rm -rf ${TMP_DIR}
mkdir ${TMP_DIR}

tar xf "$1" -C ${TMP_DIR}
cd ${TMP_DIR}/*

# Fetch all dependencies
eval ${CARGO} vendor --locked

# Archive all dependencies
name=$(basename `pwd`)-vendor
tar -jcf "../../${name}.tar.bz2" vendor --transform "s,vendor,${name},"
cd ../..
rm -rf ${TMP_DIR}

echo ""
echo "Please upload this package '${name}.tar.bz2' to our VideoLAN FTP,"
echo ""
echo "and write the following checksum into the contrib/src/<project>/cargo-vendor-SHA512SUMS:"
echo ""
sha512sum ${name}.tar.bz2
