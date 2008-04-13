# Darwin rules
download-all: autoconf automake libtool cmake gettext pkgconfig freetype2 \
    fribidi a52dec mpeg2dec libid3tag libmad libogg libvorbis libtheora flac \
    speex libshout faad2 faac lame twolame libebml libmatroska ffmpeg libdca \
    libdvdcss libdvdnav libdvbpsi live libcaca libmodplug xml asa jpeg tiff \
    SDL zlib libpng libgpg-error libgcrypt opencdk gnutls libopendaap libcddb \
    libcdio vcdimager SDL_image glib gecko-sdk mpcdec dirac expat taglib nasm \
    yasm x264 goom lua zvbi fontconfig ncurses all
all: .autoconf .automake .libtool .cmake .intl .pkgcfg .freetype \
    .fribidi .a52 .mpeg2 .id3tag .mad .ogg .vorbis .vorbisenc .theora \
    .flac .speex .shout .faad .faac .lame .twolame .ebml .matroska .ffmpeg \
    .dvdcss .dvdnav .dvdread .dvbpsi .live .caca .mod .asa \
    .png .gpg-error .gcrypt .opencdk .gnutls .opendaap .cddb .cdio .vcdimager \
    .SDL_image .glib .gecko .mpcdec .dirac_encoder .dirac_decoder \
    .dca .tag .nasm .yasm .x264 .goom2k4 .lua .zvbi .fontconfig .ncurses .aclocal
# .expat .clinkcc don't work with SDK yet
# .glib .IDL .gecko are required to build the mozilla plugin
# .mozilla-macosx will build an entire mozilla. it can be used if we need to create a new .gecko package

