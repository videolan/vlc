# Contrib configuration for TI Davinci based SoC

all: \
	.a52 .mad .ogg \
	.flac .faad .ffmpeg \
	.live .png .dvbpsi .tremor

FFMPEGCONF += --arch=arm --cpu=arm9tdmi --disable-armvfp
