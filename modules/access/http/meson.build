#
# HTTP(S) lib
#
vlc_http_lib = static_library('vlc_http',
    files(
        'message.c',
        'resource.c',
        'file.c',
        'live.c',
        'hpack.c',
        'hpackenc.c',
        'h2frame.c',
        'h2output.c',
        'h2conn.c',
        'h1conn.c',
        'chunked.c',
        'tunnel.c',
        'connmgr.c',
        'ports.c',
	'outfile.c',
    ),
    dependencies: [threads_dep, socket_libs, libvlccore_dep],
    link_with: [vlc_libcompat],
    install: false,
    include_directories: [vlc_include_dirs],
)

#
# Tests
#
hpack_test = executable('hpack_test',
    files('hpack.c'),
    c_args: ['-DDEC_TEST'],
    include_directories: [vlc_include_dirs])
hpackenc_test = executable('hpackenc_test',
    files('hpack.c', 'hpackenc.c'),
    c_args: ['-DENC_TEST'],
    link_with: vlc_libcompat,
    include_directories: [vlc_include_dirs])
h2frame_test = executable('h2frame_test',
    files(
        'h2frame_test.c',
        'hpack.c',
        'hpackenc.c',
        'h2frame.c',
    ),
    include_directories: [vlc_include_dirs])
h2output_test = executable('h2output_test',
    files('h2output_test.c'),
    link_with: vlc_http_lib,
    include_directories: [vlc_include_dirs])
h2conn_test = executable('h2conn_test',
    files('h2conn_test.c'),
    link_with: vlc_http_lib,
    include_directories: [vlc_include_dirs])
h1conn_test = executable('h1conn_test',
    files('h1conn_test.c'),
    link_with: vlc_http_lib,
    include_directories: [vlc_include_dirs])
h1chunked_test = executable('h1chunked_test',
    files('chunked_test.c'),
    link_with: vlc_http_lib,
    include_directories: [vlc_include_dirs])
http_msg_test = executable('http_msg_test',
    files('message_test.c'),
    link_with: vlc_http_lib,
    include_directories: [vlc_include_dirs])
http_file_test = executable('http_file_test',
    files('file_test.c'),
    link_with: vlc_http_lib,
    include_directories: [vlc_include_dirs])
http_tunnel_test = executable('http_tunnel_test',
    files('tunnel_test.c'),
    link_with: vlc_http_lib,
    include_directories: [vlc_include_dirs])

test('http_hpack', hpack_test, suite: 'http')
test('http_hpackenc', hpackenc_test, suite: 'http')
test('http_h2frame_test', h2frame_test, suite: 'http')
test('http_h2output_test', h2output_test, suite: 'http')
test('http_h2conn_test', h2conn_test, suite: 'http')
test('http_h1conn_test', h1conn_test, suite: 'http')
test('http_h1chunked_test', h1chunked_test, suite: 'http')
test('http_msg_test', http_msg_test, suite: 'http')
test('http_file_test', http_file_test, suite: 'http')
test('http_tunnel_test', http_tunnel_test, suite: 'http', timeout: 90)


#
# HTTP(S) module
#
vlc_modules += {
    'name' : 'https',
    'sources' : files('access.c'),
    'link_with' : [vlc_http_lib]
}
