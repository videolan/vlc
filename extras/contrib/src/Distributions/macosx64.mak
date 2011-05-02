# Darwin rules
TOOLS = .autoconf .automake .libtool .intl .pkgcfg .yasm

all: .freetype \
    .fribidi .a52 .mpeg2 .mad .ogg .vorbis .vorbisenc .fluid .theora \
    .flac .speex .shout .faad .lame .twolame .ebml .matroska .ffmpeg \
    .dvdcss .libdvdread .dvdnav .dvbpsi .live .caca .mod .fontconfig \
    .png .jpeg .tiff .gpg-error .gcrypt .gnutls .cddb .cdio .vcdimager \
    .gecko .mpcdec \
    .dca .tag .x264 .lua .zvbi .fontconfig .ncurses \
    .schroedinger .libass .libupnp .kate .sqlite3 .BGHUDAppKit .Growl .Sparkle

# .expat don't work with SDK yet
# .glib .IDL .gecko are required to build the mozilla plugin
# .mozilla-macosx will build an entire mozilla. it can be used if we need to create a new .gecko package

