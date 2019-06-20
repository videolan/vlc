# Libplacebo-based video outputs

vlc_modules += {
    'name' : 'libplacebo',
    'sources' : files('instance.c', 'display.c'),
    'link_with' : [libplacebo_utils],
    'dependencies' : [libplacebo_dep],
}

## Vulkan
vulkan_dep = dependency('vulkan',
                        version: '>= 1.0.26',
                        required: get_option('vulkan'))

if vulkan_dep.found()
    vlc_modules += {
        'name' : 'libplacebo_vk',
        'sources' : files('../vulkan/platform.c', 'instance_vulkan.c'),
        'link_with' : [libplacebo_utils],
        'dependencies' : [libplacebo_dep],
    }
endif

## OpenGL
if opengl_dep.found()
    vlc_modules += {
        'name' : 'libplacebo_gl',
        'sources' : files('instance_opengl.c'),
        'link_with' : [libplacebo_utils],
        'dependencies' : [libplacebo_dep],
    }
endif

if opengles2_dep.found()
    vlc_modules += {
        'name' : 'libplacebo_gles2',
        'sources' : files('instance_opengl.c'),
        'link_with' : [libplacebo_utils],
        'c_args' : '-DUSE_OPENGL_ES2',
        'dependencies' : [libplacebo_dep],
    }
endif
