#
# Fine tune ffmpeg's capabiities for this system
#
FFMPEGCONF += --enable-encoders --disable-muxers --disable-demuxers --disable-demuxer=dv1394 --enable-demuxer=avi --enable-demuxer=avisynth --enable-demuxer=gif --enable-demuxer=h261 --enable-demuxer=h263 --enable-demuxer=h264 --enable-demuxer=m4v --enable-demuxer=mjpg --enable-demuxer=mov --enable-demuxer=mpegps --enable-demuxer=mpegvideo --enable-demuxer=yuv4mpegpipe --disable-parsers --enable-parser=mpeg4video --enable-parser=vc1 --enable-parser=mjpeg --enable-parser=mpegaudio --enable-parser=mpegvideo --enable-parser=pnm --enable-parser=h264 --enable-parser=h263 --enable-parser=h264 --disable-decoders --enable-decoder=h261 --enable-decoder=h263 --enable-decoder=h263i --enable-decoder=h264 --enable-decoder=huffyuv --enable-decoder=idcin --enable-decoder=jpegls --enable-decoder=mjpeg --enable-decoder=mjpegb --enable-decoder=mmvideo --enable-decoder=mpeg1video --enable-decoder=mpeg2video --enable-decoder=mpeg4 --enable-decoder=mpeg4aac --enable-decoder=mpegvideo --enable-decoder=msmpeg4v1 --enable-decoder=msmpeg4v2 --enable-decoder=msmpeg4v3 --enable-decoder=msvideo1 --enable-decoder=png --enable-decoder=bmp --enable-decoder=rv10 --enable-decoder=rv20 --enable-decoder=vc1 --enable-decoder=wmav1 --enable-decoder=wmav2 --enable-decoder=wmv1 --enable-decoder=wmv2 --enable-decoder=wmv3 --enable-decoder=zlib --enable-decoder=gif --enable-decoder=tiff --disable-protocols --enable-encoder=bmp --enable-encoder=gif --enable-encoder=jpegls --enable-encoder=mjpeg --enable-encoder=png --enable-encoder=huffyuv --enable-muxer=mjpeg --enable-encoder=zlib

#
# NetBOX Linux rules
#
all: .iconv .intl .freetype .zlib \
	.ffmpeg .live .xml \
	.dvbpsi

