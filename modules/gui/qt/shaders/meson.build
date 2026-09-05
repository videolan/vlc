# Shader compilation and resource generation

env = environment()
# For qsb to locate spirv-opt:
env.append('PATH', contrib_dir + '/bin')

shader_sources = [
    'FadingEdge.frag',
    'FadingEdgeX.vert',
    'FadingEdgeY.vert',
    'Noise.frag',
    'RectFilter.frag',
    'SubTexture.vert',
    'SDFAARoundedTexture.frag',
    'SDFAARoundedTexture_cropsupport_bordersupport.frag',
    'DitheredTexture.frag',
    'FastBlend.frag',
    'FastBlend_additive.frag',
    'FastBlend_multiply.frag',
    'FastBlend_screen.frag',
    'RoundedRectangleShadow.frag',
    'RoundedRectangleShadow_hollow.frag',
    'VoronoiSnow.frag',
    'DualKawaseBlur_downsample.frag',
    'DualKawaseBlur_upsample.frag',
    'DualKawaseBlur_upsample_postprocess.frag',
]

shader_files = files(shader_sources)

qt_bin_directory = qt6_dep.get_variable(pkgconfig: 'bindir', configtool: 'QT_HOST_BINS')
qsb = find_program('qsb', dirs: qt_bin_directory, required: true)

qsb_params = ['--glsl=300 es,130,150', '--batchable', '-O']
if host_system == 'windows'
    qsb_params += ['--hlsl=50', '-c']
elif host_system == 'darwin'
    qsb_params += '--msl=12'
endif

shader_targets = []
shader_qsb = []
foreach shader: shader_sources
    shader_target_name = shader + '.qsb'
    shader_qsb += shader_target_name

    target = custom_target(shader_target_name,
      depend_files: ['Common.glsl'],
      input: shader,
      output: shader_target_name,
      command: [qsb, qsb_params, '--output', '@OUTPUT@', '@INPUT@']
    )
    shader_targets += [target]
endforeach

build_shaders = configure_file(
    output: 'shaders.qrc',
    command: [
        find_program('python3'),
        meson.current_source_dir() / '..' / 'scripts' / 'genqrc.py',
        '-o', '@OUTPUT@',
        '--prefix', 'shaders',
        '--sources', shader_qsb,
    ],
)

shaders_processed = qt6.preprocess(
        qresources: build_shaders,
        dependencies: qt6_dep)