# android rules
# Not compiling: .shout .live .mod .x264 .caca .mpcdec .dirac .schroedinger .libass
all: .zlib \
     .a52 .mpeg2 .mad .ogg .vorbis .vorbisenc .theora \
     .flac .speex .faad .lame .ffmpeg \
     .twolame \
     .png .dvbpsi \
     .dca .kate .ebml .matroska


ANDROID_INCLUDE=$(ANDROID_NDK)/platforms/android-9/arch-arm/usr/include
ANDROID_LIB=$(ANDROID_NDK)/platforms/android-9/arch-arm/usr/lib

EXTRA_CPPFLAGS+=-I$(ANDROID_INCLUDE)
EXTRA_LDFLAGS+=-Wl,-rpath-link=$(ANDROID_LIB),-Bdynamic,-dynamic-linker=/system/bin/linker -Wl,--no-undefined -L$(ANDROID_LIB)
