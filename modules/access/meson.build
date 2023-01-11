#
# File access modules
#

# Data URL-scheme access
vlc_modules += {
    'name' : 'data',
    'sources' : files('data.c')
}

# Filesystem access module
vlc_modules += {
    'name' : 'filesystem',
    'sources' : files('file.c', 'directory.c', 'fs.c'),
}

# Dummy access module
vlc_modules += {
    'name' : 'idummy',
    'sources' : files('idummy.c')
}

# In-memory bitstream input module
vlc_modules += {
    'name' : 'imem',
    'sources' : files('imem-access.c'),
    'dependencies' : [m_lib]
}

# Memory input module
vlc_modules += {
    'name' : 'access_imem',
    'sources' : files('imem.c')
}

# Fake sdp:// scheme input module
vlc_modules += {
    'name' : 'sdp',
    'sources' : files('sdp.c')
}

# Timecode sub-picture generator module
vlc_modules += {
    'name' : 'timecode',
    'sources' : files('timecode.c')
}

# VDR recordings access module
vlc_modules += {
    'name' : 'vdr',
    'sources' : files('vdr.c')
}

## Audio capture

# Alsa capture access
if alsa_dep.found()
    vlc_modules += {
        'name' : 'access_alsa',
        'sources' : files('alsa.c'),
        'dependencies' : [alsa_dep]
    }
endif

# OSS access
if get_option('oss').disable_auto_if(
    host_system not in ['freebsd', 'netbsd', 'dragonfly']).allowed()

    # TODO: Actually add the OSS module
endif

# JACK input module
if jack_dep.found()
    vlc_modules += {
        'name' : 'access_jack',
        'sources' : files('jack.c'),
        'dependencies' : [jack_dep]
    }
endif

# Pulseaudio module
if pulse_dep.found()
    vlc_modules += {
        'name' : 'pulsesrc',
        'sources' : files('pulse.c'),
        'dependencies' : [pulse_dep],
        'link_with' : [libvlc_pulse],
    }
endif

# AVFoundation audio capture access
if have_osx
    vlc_modules += {
        'name' : 'avaudiocapture',
        'sources' : files('avaudiocapture.m'),
        'objc_args' : ['-fobjc-arc'],
        'dependencies' : [avfoundation_dep, coremedia_dep, foundation_dep]
    }
endif

# WASAPI access
if (get_option('wasapi')
    .require(host_system == 'windows', error_message: 'WASAPI requires Windows')
    .allowed() and ksuser_lib.found())
    vlc_modules += {
        'name' : 'access_wasapi',
        'sources' : files('wasapi.c'),
        'dependencies' : [ksuser_lib]
    }
endif


## Video capture

# macOS AVFoundation-based video capture
if (get_option('macosx_avfoundation')
    .require(have_osx and host_system == 'darwin', error_message: 'AVCapture requires macOS')
    .allowed())
    vlc_modules += {
        'name' : 'avcapture',
        'sources' : files('avcapture.m'),
        'objc_args' : ['-fobjc-arc'],
        'dependencies' : [avfoundation_dep, coremedia_dep, foundation_dep, corevideo_dep],
    }
endif

# IIDC FireWire input
libdc1394_dep = dependency('libdc1394-2', version: '>= 2.1.0', required: get_option('dc1394'))
if libdc1394_dep.found()
    vlc_modules += {
        'name' : 'dc1394',
        'sources' : files('dc1394.c'),
        'dependencies' : [libdc1394_dep]
    }
endif

# DV FireWire input
libraw1394_dep = dependency('libraw1394', version: '>= 2.0.1', required: get_option('dv1394'))
libavc1394_dep = dependency('libavc1394', version: '>= 0.5.3', required: get_option('dv1394'))
if libraw1394_dep.found() and libavc1394_dep.found()
    vlc_modules += {
        'name' : 'dv1394',
        'sources' : files('dv.c'),
        'dependencies' : [libraw1394_dep, libavc1394_dep, threads_dep],
    }
endif

# Linsys
if (get_option('linsys')
    .require(host_system == 'linux', error_message: 'Linsys requires Linux')
    .allowed())
    vlc_modules += {
        'name' : 'linsys_hdsdi',
        'sources' : files('linsys/linsys_hdsdi.c'),
        'dependencies' : [threads_dep]
    }

    if zvbi_dep.found()
        vlc_modules += {
            'name' : 'linsys_sdi',
            'sources' : files('linsys/linsys_sdi.c'),
            'dependencies' : [zvbi_dep]
        }
    endif
endif

# Shared memory frame buffer capture
vlc_modules += {
    'name' : 'shm',
    'sources' : files('shm.c'),
    'dependencies' : [m_lib]
}


## Screen grab plugins

subdir('screen')

# RDP/Remote Client Desktop capture
freerdp_dep = dependency('freerdp', required: get_option('freerdp'))
if freerdp_dep.found()
    vlc_modules += {
    'name' : 'rdp',
    'sources' : files('rdp.c'),
    'dependencies' : [freerdp_dep]
    }
endif

# VNC capture
vncclient_dep = dependency('libvncclient', required: get_option('vnc'))
if vncclient_dep.found()
    vlc_modules += {
    'name' : 'vnc',
    'sources' : files('vnc.c'),
    'dependencies' : [vncclient_dep]
    }
endif


## Optical media

# VCD and CD-DA access module
if get_option('vcd_module')
    vcd_cdda_flags = []

    # TODO: Add gcrypt dependency if it's present
    libcddb_dep = dependency('libcddb', version: '>= 0.9.5', required: get_option('libcddb'))
    if libcddb_dep.found()
        vcd_cdda_flags += '-DHAVE_LIBCDDB'
    endif

    if cc.has_header_symbol('linux/cdrom.h', 'struct cdrom_msf0')
        needed_vcd_unix_headers_present = true
    elif cc.has_header_symbol('sys/scsiio.h', 'struct scsireq')
        needed_vcd_unix_headers_present = true
        vcd_cdda_flags += '-DHAVE_SCSIREQ_IN_SYS_SCSIIO_H'
    elif cc.has_header_symbol('sys/cdio.h', 'struct ioc_toc_header')
        needed_vcd_unix_headers_present = true
        vcd_cdda_flags += '-DHAVE_IOC_TOC_HEADER_IN_SYS_CDIO_H'
    else
        needed_vcd_unix_headers_present = false
    endif

    if (needed_vcd_unix_headers_present or 
        host_system in ['windows'] or have_osx)

        vcd_cdda_darwin_deps = []
        if host_system == 'darwin'
            vcd_cdda_darwin_deps = [iokit_dep, corefoundation_dep]
        endif

        vlc_modules += {
            'name' : 'cdda',
            'sources' : files(
                'cdda.c',
                'vcd/cdrom.c',
                '../misc/webservices/json.c',
                '../misc/webservices/musicbrainz.c',
            ),
            'c_args' : vcd_cdda_flags,
            'dependencies' : [libcddb_dep, vcd_cdda_darwin_deps]
        }

        vlc_modules += {
            'name' : 'vcd',
            'sources' : files(
                'vcd/vcd.c',
                'vcd/cdrom.c',
            ),
            'c_args' : vcd_cdda_flags,
            'dependencies' : [libcddb_dep, vcd_cdda_darwin_deps]
        }

    endif
endif

# DVD with navigation (dvdnav) access
dvdnav_dep = dependency('dvdnav', version: '>= 6.0.0', required: get_option('dvdnav'))
if dvdnav_dep.found()
    vlc_modules += {
        'name' : 'dvdnav',
        'sources' : files('dvdnav.c'),
        'dependencies' : [
            dvdnav_dep,
            (host_system == 'darwin') ? [corefoundation_dep, iokit_dep] : []
        ]
    }
endif

# DVD access
dvdread_dep = dependency('dvdread', version: '>= 6.0.0', required: get_option('dvdread'))
if dvdread_dep.found()
    vlc_modules += {
        'name' : 'dvdread',
        'sources' : files('dvdread.c'),
        'dependencies' : [
            dvdread_dep,
            (host_system == 'darwin') ? [corefoundation_dep, iokit_dep] : []
        ]
    }
endif

# Blu-ray access
libbluray_dep = dependency('libbluray', version: '>= 0.6.2', required: get_option('bluray'))
if libbluray_dep.found()
    vlc_modules += {
        'name' : 'bluray',
        'sources' : files('bluray.c'),
        'dependencies' : [libbluray_dep]
    }
endif


## Digital TV

# DTV access
dtv_common_sources = files('dtv/access.c')

if host_system == 'linux'
    has_linux_dvb_5_1 = cc.compiles(
        '''
        #include <linux/dvb/version.h>
        #if (DVB_API_VERSION < 5)
        # error Linux DVB API v3.2 and older are not supported.
        #endif
        #if (DVB_API_VERSION == 5 && DVB_API_VERSION_MINOR < 1)
        # error Linux DVB API v5.0 is unsupported. Please update.
        #endif
        '''
    )
    if (has_linux_dvb_5_1)
        message('Has Linux DVB 5.1 or higher: YES')
        vlc_modules += {
            'name' : 'dtv',
            'sources' : [
                dtv_common_sources,
                files('dtv/linux.c', 'dtv/en50221.c')
            ],
            'c_args' : ['-DHAVE_LINUX_DVB']
        }
    else
        message('Has Linux DVB 5.1 or higher: NO')
    endif
endif

if host_system == 'windows'
    strmiids_lib = cc.find_library('strmiids', required: true)
    vlc_modules += {
        'name' : 'dtv',
        'sources' : [
            dtv_common_sources,
            files('dtv/bdagraph.cpp')
        ],
        'dependencies' : [strmiids_lib],
        'cpp_args' : libcom_cppflags
    }
endif


## Network stream access modules

# FTP
vlc_modules += {
    'name' : 'ftp',
    'sources' : files('ftp.c'),
    'dependencies' : [socket_libs]
}

# Gopher
vlc_modules += {
    'name' : 'gopher',
    'sources' : files('gopher.c'),
    'dependencies' : [socket_libs]
}

# Old HTTP
vlc_modules += {
    'name' : 'http',
    'sources' : files('http.c'),
    'dependencies' : [socket_libs]
}

# New HTTP(S)
subdir('http')

# TCP
vlc_modules += {
    'name' : 'tcp',
    'sources' : files('tcp.c'),
    'dependencies' : [socket_libs]
}

# UDP
vlc_modules += {
    'name' : 'udp',
    'sources' : files('udp.c'),
    'dependencies' : [socket_libs]
}

# AMT
vlc_modules += {
    'name' : 'amt',
    'sources' : files('amt.c'),
    'dependencies' : [socket_libs]
}

# Windows UNC
if host_system == 'windows'
    mpr_lib = cc.find_library('mpr', required: true)
    netapi32_lib = cc.find_library('netapi32', required: true)

    vlc_modules += {
        'name' : 'unc',
        'sources' : files('unc.c'),
        'dependencies' : [mpr_lib, netapi32_lib]
    }
endif

# AVIO access module
if avformat_dep.found()
    vlc_modules += {
        'name' : 'avio',
        'sources' : files('avio.c'),
        'dependencies' : [avutil_dep, avformat_dep, m_lib],
        'link_args' : symbolic_linkargs
    }
endif

# SAT>IP
vlc_modules += {
    'name' : 'satip',
    'sources' : files('satip.c'),
    'dependencies' : [socket_libs, threads_dep]
}


## Misc

vlc_modules += {
    'name' : 'access_concat',
    'sources' : files('concat.c')
}

# Media Transfer Protocol (MTP)
if mtp_dep.found()
    vlc_modules += {
        'name' : 'access_mtp',
        'sources' : files('mtp.c'),
        'dependencies' : [mtp_dep]
    }
endif

# SRT
if srt_dep.found()
    vlc_modules += {
        'name' : 'access_srt',
        'sources' : files('srt.c', 'srt_common.c'),
        'dependencies' : [srt_dep]
    }
endif

# RIST
if librist_dep.found()
    vlc_modules += {
        'name' : 'rist',
        'sources' : files('rist.c'),
        'dependencies' : [socket_libs, librist_dep]
    }
endif
