# Chromecast module
protobuf_dep = dependency('protobuf', required: get_option('chromecast'))
protoc = disabler()
chromecast_proto = []

if protobuf_dep.found()
    # Protoc can output either a 2-number version or a 3-number version...
    # Protobuf switched from version naming 3.20.12.z to version 21.7.z, and
    # protoc provide compatibility with version x.y now. But version 21.y.z
    # still outputs the previous kind of version naming up until 3.21.12.z
    # included, so the first version with the x.y format is 22.0.
    protobuf_version_list = protobuf_dep.version().split('.')
    if protobuf_dep.version().version_compare('< 3.22.0.0')
        protoc_version = '@0@.@1@.@2@'.format(
            protobuf_version_list[0],
            protobuf_version_list[1],
            protobuf_version_list[2])
    else
        # Protobuf version is x.y.z and protoc version is x.y
        protoc_version = '@0@.@1@'.format(protobuf_version_list[0], protobuf_version_list[1])
    endif
    protoc = find_program('protoc',
        version: protoc_version,
        required: get_option('chromecast'))
endif

if protoc.found()
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
