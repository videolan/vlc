# Secure RTP with libgcrypt
# Copyright (C) 2007  Rémi Denis-Courmont
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

AM_CPPFLAGS = @GCRYPT_CFLAGS@

noinst_HEADERS = srtp.h

EXTRA_PROGRAMS = srtp
noinst_LTLIBRARIES = libvlc_srtp.la
check_PROGRAMS = test-aes test-recv
TESTS = $(check_PROGRAMS)

libvlc_srtp_la_SOURCES = srtp.c
libvlc_srtp_la_LIBADD = @GCRYPT_LIBS@

srtp_SOURCES = recv.c
srtp_LDADD = libvlc_srtp.la
test_recv_LDADD = libvlc_srtp.la
test_aes_LDADD = @GCRYPT_LIBS@

lcov-run:
	rm -Rf *.gcda lcov
	$(MAKE) $(AM_MAKEFLAGS) check

lcov-pre.out:
	lcov -c -d . -o lcov.tmp

lcov.out: lcov-pre.out
	lcov -r lcov.tmp '*test*' > lcov.out

lcov: lcov.out
	genhtml lcov.out -o lcov

.PHONY: lcov-run
