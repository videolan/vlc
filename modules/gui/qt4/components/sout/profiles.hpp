#define NB_PROFILE \
    (sizeof(video_profile_value_list)/sizeof(video_profile_value_list[0]))

static const char video_profile_name_list[][37] = {
    "Video - H.264 + MP3 (MP4)",
    "Video - VP80 + Vorbis (Webm)",
    "Video - H.264 + MP3 (TS)",
    "Video - Dirac + MP3 (TS)",
    "Video - Theora + Vorbis (OGG)",
    "Video - Theora + Flac (OGG)",
    "Video - MPEG-2 + MPGA (TS)",
    "Video - WMV + WMA (ASF)",
    "Video - DIV3 + MP3 (ASF)",
    "Audio - Vorbis (OGG)",
    "Audio - MP3",
    "Audio - MP3 (MP4)",
    "Audio - FLAC",
    "Audio - CD",
    "Video for MPEG4 720p TV/device",
    "Video for MPEG4 1080p TV/device",
    "Video for DivX compatible player",
    "Video for iPod SD",
    "Video for iPod HD/iPhone/PSP",
    "Video for Android SD Low",
    "Video for Android SD High",
    "Video for Android HD",
    "Video for Youtube SD",
    "Video for Youtube HD",
};

static const char video_profile_value_list[][58] = {
    /* Container(string), transcode video(bool), transcode audio(bool), */
    /* use subtitles(bool), video codec(string), video bitrate(integer), */
    /* scale(float), fps(float), width(integer, height(integer), */
    /* audio codec(string), audio bitrate(integer), channels(integer), */
    /* samplerate(integer), subtitle codec(string), subtitle overlay(bool) */
    "mp4;1;1;0;h264;0;0;0;0;0;mpga;128;2;44100;0;1",
    "webm;1;1;0;VP80;2000;0;0;0;0;vorb;128;2;44100;0;1",
    "ts;1;1;0;h264;800;1;0;0;0;mpga;128;2;44100;0;0",
    "ts;1;1;0;drac;800;1;0;0;0;mpga;128;2;44100;0;0",
    "ogg;1;1;0;theo;800;1;0;0;0;vorb;128;2;44100;0;0",
    "ogg;1;1;0;theo;800;1;0;0;0;flac;128;2;44100;0;0",
    "ts;1;1;0;mp2v;800;1;0;0;0;mpga;128;2;44100;0;0",
    "asf;1;1;0;WMV2;800;1;0;0;0;wma2;128;2;44100;0;0",
    "asf;1;1;0;DIV3;800;1;0;0;0;mp3;128;2;44100;0;0",
    "ogg;1;1;0;none;800;1;0;0;0;vorb;128;2;44100;none;0",
    "raw;1;1;0;none;800;1;0;0;0;mp3;128;2;44100;none;0",
    "mp4;1;1;0;none;800;1;0;0;0;mpga;128;2;44100;none;0",
    "raw;1;1;0;none;800;1;0;0;0;flac;128;2;44100;none;0",
    "wav;1;1;0;none;800;1;0;0;0;s16l;128;2;44100;none;0",
    "mp4;1;1;0;h264;1500;1;0;1280;720;mp3;192;2;44100;none;0",
    "mp4;1;1;0;h264;3500;1;0;1920;1080;mp3;192;2;44100;none;0",
    "avi;1;1;0;DIV3;900;1;0;720;568;mp3;128;2;44100;0;0",
    "mp4;1;1;0;h264;600;1;0;320;180;mp3;128;2;44100;none;0",
    "mp4;1;1;0;h264;700;1;0;480;272;mp3;128;2;44100;none;0",
    "mp4;1;1;0;h264;56;1;12;176;144;mp3;24;1;44100;none;0",
    "mp4;1;1;0;h264;500;1;0;480;360;mp3;128;2;44100;none;0",
    "mp4;1;1;0;h264;2000;1;0;1280;720;mp3;192;2;44100;none;0",
    "mp4;1;1;0;h264;800;1;0;640;480;mp3;128;2;44100;none;0",
    "mp4;1;1;0;h264;1500;1;0;1280;720;mp3;128;2;44100;none;0",
};


