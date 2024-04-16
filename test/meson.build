subdir('src')
subdir('modules')
subdir('libvlc')

foreach vlc_test: vlc_tests
    if not vlc_test.has_key('name')
        error('Got invalid vlc_tests entry without \'name\' key')
    endif
    if not vlc_test.has_key('sources')
        error('Got invalid vlc_tests entry without \'sources\' key')
    endif

    # This list MUST be kept in sync with the keys used below!
    valid_dict_keys = [
        'name',
        'sources',
        'moc_headers',
        'suite',
        'link_with',
        'module_depends',
        'dependencies',
        'c_args',
        'cpp_args',
        'objc_args',
        'include_directories',
    ]

    foreach key : vlc_test.keys()
        if key not in valid_dict_keys
            error('Invalid key \'@0@\' found in vlc_tests entry for \'@1@\''
                .format(key, vlc_test['name']))
        endif
    endforeach

    common_args = [
        '-DSRCDIR="@0@"'.format(vlc_src_root + '/test/'),
        '-DTOP_BUILDDIR="@0@"'.format(vlc_build_root),
        '-DTOP_SRCDIR="@0@"'.format(vlc_src_root),
    ]

    disabled_dependencies = []
    test_modules_deps = []
    foreach module_name : vlc_test.get('module_depends', [])
        if module_name not in vlc_plugins_targets.keys()
            error('Test entry \'@0@\' cannot depend on non-existant VLC plugin \'@1@\''
                  .format(vlc_test['name'], module_name))
        endif
        test_modules_deps += vlc_plugins_targets[module_name]
        if not vlc_plugins_manifest[module_name].get('enabled', true)
            disabled_dependencies += module_name
        endif
    endforeach

    if disabled_dependencies != []
        # TODO: mark as skipped
        continue
    endif

    moc_sources = []
    if vlc_test.has_key('moc_headers') and qt6_dep.found()
        moc_sources += qt6.preprocess(moc_headers: vlc_test['moc_headers'],
        include_directories: qt_include_dir,
        dependencies: qt6_dep)
    endif

    test(vlc_test['name'],
        executable(vlc_test['name'], vlc_test['sources'], moc_sources,
            build_by_default: false,
            link_with: [vlc_test.get('link_with', []),
                vlc_libcompat],
            include_directories: [vlc_test.get('include_directories', []),
                vlc_include_dirs],
            dependencies: [vlc_test.get('dependencies', []),
                libvlccore_deps],
            c_args: [vlc_test.get('c_args', []), common_args],
            cpp_args: [vlc_test.get('cpp_args', []), common_args],
            objc_args: [vlc_test.get('objc_args', []), common_args]),
        suite: [vlc_test.get('suite', []), 'test'],
        depends: [test_modules_deps])
endforeach
