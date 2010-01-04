# Contrib configuration for TI Davinci based SoC

all: \
	.a52 .mad .ogg .theora \
	.flac .faad .ffmpeg \
	.live .png .dvbpsi .tremor

FFMPEGCONF += --arch=arm --disable-neon --disable-armv6 --disable-armv6t2 --disable-armvfp --disable-iwmmxt
