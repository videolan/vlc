sdp_test_SOURCES = \
	access/rtp/sdp.c \
	access/rtp/test/sdp.c
check_PROGRAMS += sdp_test
TESTS += sdp_test

srtp_aes_test_SOURCES = access/rtp/test/srtp-aes.c
srtp_aes_test_LDADD = $(GCRYPT_LIBS)
srtp_recv_test_SOURCES = access/rtp/test/srtp-recv.c
srtp_recv_test_LDADD = libvlc_srtp.la

if HAVE_GCRYPT
check_PROGRAMS += srtp_aes_test srtp_recv_test
TESTS += srtp_aes_test srtp_recv_test
endif
