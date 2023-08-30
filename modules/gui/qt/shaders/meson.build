# Shader compilation and resource generation

qt6 = import('qt6')

env = environment()
# For qsb to locate spirv-opt:
env.append('PATH', contrib_dir + '/bin')

shader_sources = [
    'FadingEdge.frag',
    'FadingEdgeX.vert',
    'FadingEdgeY.vert',
    'FrostedGlass.frag',
    'RectFilter.frag',
    'SubTexture.vert',
    'PlayerBlurredBackground.frag'
]

shader_files = files(shader_sources)

qt_bin_directory = qt6_dep.get_variable(pkgconfig: 'bindir')
qsb = find_program(qt_bin_directory + '/qsb', required: true)

qsb_params = ['--glsl=100 es,120,150', '--batchable', '-O']
if host_system == 'windows'
    qsb_params += ['--hlsl=50', '-c']
elif host_system == 'darwin'
    qsb_params += '--msl=12'
endif

shader_targets = []
foreach shader: shader_sources
    shader_target_name = shader + '.qsb'

    target = custom_target(shader_target_name,
      input: shader,
      output: shader_target_name,
      command: [qsb, qsb_params, '--output', '@OUTPUT@', '@INPUT@']
    )
    shader_targets += [target]
endforeach

build_shaders = configure_file(input: 'shaders.qrc', output: 'shaders.qrc', copy: true)

shaders_processed = qt6.preprocess(
        qresources: build_shaders,
        dependencies: qt6_dep)
