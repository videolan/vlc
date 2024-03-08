# goom visualization plugin
goom2_dep = dependency('libgoom2', required: get_option('goom2'))
vlc_modules += {
    'name' : 'goom',
    'sources' : files('goom.c'),
    'dependencies' : [goom2_dep, m_lib],
    'enabled' : goom2_dep.found(),
}

# projectM visualization plugin
projectm_dep = dependency('libprojectM', version: '>= 2.0.0', required: false)
projectm_args = []

if projectm_dep.found()
    projectm_args += '-DHAVE_PROJECTM2'
else
    projectm_dep = dependency('libprojectM', version: '< 2.0.0',
        required: false)
endif

vlc_modules += {
    'name' : 'projectm',
    'sources' : files('projectm.cpp'),
    'dependencies' : [projectm_dep],
    'cpp_args' : [projectm_args],
    'enabled' : projectm_dep.found() and host_system != 'windows'
}
