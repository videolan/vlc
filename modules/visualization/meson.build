# glspectrum plugin
vlc_modules += {
    'name' : 'glspectrum',
    'sources' : files('glspectrum.c',
      'visual/fft.c', 'visual/fft.h',
      'visual/window.c', 'visual/window.h',
      'visual/window_presets.h'),
    'dependencies' : [opengl_dep, m_lib],
    'c_args' : [ contrib_inc_args ],
    'enabled' : opengl_dep.found(),
}

# goom visualization plugin
goom2_dep = dependency('libgoom2', required: get_option('goom2'))
vlc_modules += {
    'name' : 'goom',
    'sources' : files('goom.c'),
    'dependencies' : [goom2_dep, m_lib],
    'enabled' : goom2_dep.found(),
}

# projectM visualization plugin
projectm_dep = dependency('libprojectM', required: get_option('projectm'))
if projectm_dep.found() and projectm_dep.version().version_compare('>= 2.0.0')
    cdata.set('HAVE_PROJECTM2',1)
endif

vlc_modules += {
    'name' : 'projectm',
    'sources' : files('projectm.cpp'),
    'dependencies' : [projectm_dep],
    'cpp_args' : [ '-std=gnu++14' ],
    'enabled' : projectm_dep.found()
}

# Visual visualization plugin
vlc_modules += {
    'name' : 'visual',
    'sources' : files(
        'visual/visual.c', 'visual/visual.h',
        'visual/fft.c', 'visual/fft.h',
        'visual/window.c', 'visual/window.h',
        'visual/effects.c', 'visual/window_presets.h'),
    'dependencies' : [m_lib],
}

# Vsxu plugin
vsxu_dep = dependency('libvsxu', required: get_option('vsxu'))
vlc_modules += {
    'name' : 'vsxu',
    'sources' : files('vsxu.cpp', 'cyclic_buffer.h'),
    'dependencies' : [m_lib, vsxu_dep],
    'enabled' : vsxu_dep.found()
}
