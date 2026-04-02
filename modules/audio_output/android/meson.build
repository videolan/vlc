android_env_c = files('../../video_output/android/env.c')

android_audio_common = files(
    'audioformat_jni.c',
    'dynamicsprocessing_jni.c',
)

vlc_modules += {
    'name' : 'android_audiodev',
    'sources' : [android_audio_common, android_env_c, files('device.c')],
    'enabled' : host_system == 'android',
}

vlc_modules += {
    'name' : 'audiotrack',
    'sources' : [android_audio_common, android_env_c, files('audiotrack.c')],
    'enabled' : host_system == 'android',
}

opensles_lib = cc.find_library('OpenSLES', required: false)
vlc_modules += {
    'name' : 'opensles',
    'sources' : files('opensles.c'),
    'dependencies' : [opensles_lib],
    'enabled' : host_system == 'android' and opensles_lib.found(),
}

aaudio_lib = cc.find_library('aaudio', required: false)
vlc_modules += {
    'name' : 'aaudio',
    'sources' : [android_audio_common, android_env_c, files('aaudio.c')],
    'dependencies' : [aaudio_lib],
    'enabled' : host_system == 'android' and aaudio_lib.found(),
}
