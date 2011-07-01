#! /bin/sh

HOST="$1"
if test -z "$HOST"; then
	echo "Usage: $0 <target machine>" >&2
	exit 1
fi

case "$HOST" in
	amd64-*)
		ARCH="x86_64"
		;;
	i[3456]86-*)
		ARCH="i386"
		;;
	powerpc-*|ppc-*)
		ARCH="ppc"
		;;
	powerpc64-*|ppc64-*)
		ARCH="ppc64"
		;;
	*-*)
		ARCH="${HOST%%-*}"
		;;
	*)
		echo "$HOST: invalid machine specification" >&2
		exit 1
esac
echo $ARCH
