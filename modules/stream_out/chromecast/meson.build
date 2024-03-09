# Chromecast module
protobuf_dep = dependency('protobuf', required: get_option('chromecast'))
protoc = disabler()
chromecast_proto = []

if protobuf_dep.found()
    protobuf_version_list = protobuf_dep.version().split('.')
    protoc = find_program('protoc',
        version: '@0@.@1@'.format(protobuf_version_list[0], protobuf_version_list[1]),
        required: get_option('chromecast'))
    protobuf_gen = generator(protoc,
        output: [
            '@BASENAME@.pb.cc'.format(protobuf_dep.version()),
            '@BASENAME@.pb.h'.format(protobuf_dep.version())
        ],
        arguments: [
            '--proto_path=@CURRENT_SOURCE_DIR@',
            '--cpp_out=@BUILD_DIR@',
            '@INPUT@',
        ])
    chromecast_proto += [protobuf_gen.process('cast_channel.proto')]
endif

vlc_modules += {
    'name' : 'demux_chromecast',
    'sources' : files(
        'chromecast_demux.cpp',
        'chromecast.h',
        'chromecast_common.h') + chromecast_proto,
    'dependencies' : [protobuf_dep],
    'enabled': get_option('stream_outputs') and protoc.found() and protobuf_dep.found(),
}

vlc_modules += {
    'name' : 'chromecast',
    'sources' : files(
        'cast.cpp',
        'chromecast.h',
        'chromecast_common.h',
        'chromecast_ctrl.cpp',
        'chromecast_communication.cpp',
        '../../misc/webservices/json.c',
        '../../misc/webservices/json.h',
        '../renderer_common.cpp',
        '../renderer_common.hpp') +
        chromecast_proto,
    'dependencies' : [socket_libs, protobuf_dep],
    'enabled': get_option('stream_outputs') and protobuf_dep.found() and protoc.found(),
}
