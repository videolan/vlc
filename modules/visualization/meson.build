# goom visualization plugin
goom2_dep = dependency('libgoom2', required: get_option('goom2'))
if goom2_dep.found()
    vlc_modules += {
        'name' : 'goom',
        'sources' : files('goom.c'),
        'dependencies' : [goom2_dep, m_lib]
    }
endif

# projectM visualization plugin
projectm_dep = dependency('libprojectM', version: '>= 2.0.0', required: false)
projectm_args = []

if projectm_dep.found()
    projectm_args += '-DHAVE_PROJECTM2'
else
    projectm_dep = dependency('libprojectM', version: '< 2.0.0',
        required: false)
endif

if projectm_dep.found()
  if host_system != 'windows'
    vlc_modules += {
        'name' : 'projectm',
        'sources' : files('projectm.cpp'),
        'dependencies' : [projectm_dep],
        'cpp_args' : [projectm_args]
    }
  endif
endif
