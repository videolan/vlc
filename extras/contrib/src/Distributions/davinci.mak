# Contrib configuration for TI Davinci based SoC

all: \
	.a52 .id3tag .mad .ogg .theora \
	.flac .faad \
	.live .png .dvbpsi .tremor
# .ffmpeg # ARM assembly code can't be built by the old montavista toolchain
#FFMPEGCONF += --arch=arm
