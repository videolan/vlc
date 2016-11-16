function assert_url(result, protocol, username, password, host, port, path)
    assert(result.protocol == protocol)
    assert(result.username == username)
    assert(result.password == password)
    assert(result.host     == host)
    assert(result.port     == port)
    assert(result.path     == path)
end

vlc.msg.info('---- Testing misc functions ----')
vlc.msg.info('version: ' .. vlc.misc.version())
vlc.msg.info('copyright: ' .. vlc.misc.copyright())
vlc.msg.info('license: ' .. vlc.misc.license())
vlc.msg.info('mdate: ' .. vlc.misc.mdate())

vlc.msg.info('---- Testing config functions ----')
vlc.msg.info('datadir: ' .. vlc.config.datadir())
vlc.msg.info('userdatadir: ' .. vlc.config.userdatadir())
vlc.msg.info('homedir: ' .. vlc.config.homedir())
vlc.msg.info('configdir: ' .. vlc.config.configdir())
vlc.msg.info('cachedir: ' .. vlc.config.cachedir())

vlc.msg.info('---- Testing net functions ----')
vlc.msg.info(' * testing vlc.strings.url_parse')
vlc.msg.info('    "filename.ext"')
assert_url(vlc.strings.url_parse('file:///filename.ext'), 'file', nil, nil,
           nil, 0, '/filename.ext')
vlc.msg.info('    "http://server.org/path/file.ext"')
assert_url(vlc.strings.url_parse('http://server.org/path/file.ext'),
           'http', nil, nil, 'server.org', 0, '/path/file.ext')
vlc.msg.info('    "rtmp://server.org:4212/bla.ext"')
assert_url(vlc.strings.url_parse('rtmp://server.org:4212/bla.ext'),
           'rtmp', nil, nil, 'server.org', 4212, '/bla.ext')
vlc.msg.info('    "ftp://userbla@server.org:4567/bla.ext"')
assert_url(vlc.strings.url_parse('rtmp://userbla@server.org:4567/bla.ext'),
           'rtmp', 'userbla', nil, 'server.org', 4567, '/bla.ext')
vlc.msg.info('    "sftp://userbla:Passw0rd@server.org/bla.ext"')
assert_url(vlc.strings.url_parse('sftp://userbla:Passw0rd@server.org/bla.ext'),
           'sftp', 'userbla', 'Passw0rd', 'server.org', 0, '/bla.ext')
vlc.msg.info("")

vlc.misc.quit()
