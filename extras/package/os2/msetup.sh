#! /bin/sh

if test -z "$MESON_DIR"; then
    echo "Please set MESON_DIR to the directory of meson.py."
    exit 1
fi

OPTIONS="
      --prefer-static
      -Drun_as_root=true
      -Dlua=enabled
      -Dlive555=enabled
      -Ddvdread=enabled
      -Ddvdnav=enabled
      -Dsftp=enabled
      -Dvcd_module=true
      -Dlibcddb=enabled
      -Dlibdvbpsi=enabled
      -Dogg=enabled
      -Dmad=enabled
      -Dmerge-ffmpeg=true
      -Davcodec=enabled
      -Davformat=enabled
      -Dswscale=enabled
      -Dpostproc=enabled
      -Dflac=enabled
      -Dvorbis=enabled
      -Dpng=enabled
      -Dx264=enabled
      -Dlibass=enabled
      -Dxcb=disabled
      -Dfreetype=enabled
      -Dfribidi=enabled
      -Dfontconfig=enabled
      -Dkva=enabled
      -Dkai=enabled
      -Dqt=enabled
      -Dskins2=enabled
      -Dlibxml2=enabled
      -Dlibgcrypt=enabled
      -Dgnutls=enabled
      -Dvlc=true
"

python "$MESON_DIR"/meson.py setup $OPTIONS "$@"
