# Access output plugins

# Dummy
vlc_modules += {
    'name' : 'access_output_dummy',
    'sources': files('dummy.c')
}

# File
vlc_modules += {
    'name' : 'access_output_file',
    'sources': files('file.c')
}

# HTTP
vlc_modules += {
    'name' : 'access_output_http',
    'sources': files('http.c')
}

# TODO: Add HAVE_GCRYPT + livehttp

# Shout (Icecast and old Shoutcast)
shout_dep = dependency('shout', version: '>= 2.1', required: get_option('shout'))
if shout_dep.found()
    vlc_modules += {
        'name' : 'access_output_shout',
        'sources' : files('shout.c'),
        'dependencies' : [shout_dep, socket_libs]
    }
endif

# HTTP PUT
vlc_modules += {
    'name' : 'access_output_http_put',
    'sources' : files('http-put.c'),
    'include_directories' : include_directories('../access/http'),
    'link_with' : [vlc_http_lib]
}

# SRT
if srt_dep.found()
    vlc_modules += {
	'name' : 'access_output_srt',
	'sources' : files('srt.c', '../access/srt_common.c'),
	'include_directories' : include_directories('../access/'),
	'dependencies' : [srt_dep],
    }
endif

# RIST
if librist_dep.found()
    vlc_modules += {
	'name' : 'access_output_rist',
	'sources' : files('rist.c'),
	'include_directories' : include_directories('../access/'),
	'dependencies' : [librist_dep]
    }
endif
