# Maemo 5
all: \
	.mad .a52 .dca .mpcdec .lame .mpeg2 \
	.id3tag \
	.ogg .tremor .vorbisenc .flac .speex .theora .kate .tiger \
	.twolame .x264 \
	.ebml .matroska .ffmpeg .mod \
	.live .dvbpsi .zvbi \
	.lua .xcb .xcb-util
#.faad -> way slower than libavcodec
#.gpg-error .gcrypt .gnutls -> OpenSSL
#.dvdcss .dvdnav -> no DVD
#.dirac -> broken
