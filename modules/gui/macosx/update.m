/*****************************************************************************
 * update.m: MacOS X Check-For-Update window
 *****************************************************************************
 * Copyright (C) 2005 the VideoLAN team
 * $Id$
 *
 * Authors: Felix KŸhne <fkuehne@users.sf.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/


/*****************************************************************************
 * Note: 
 * the code used to bind with VLC's core and the download of files is heavily 
 * based upon ../wxwidgets/updatevlc.cpp, written by Antoine Cellerier. 
 * (he is a member of the VideoLAN team) 
 *****************************************************************************/


/*****************************************************************************
 * Preamble
 *****************************************************************************/
#import "update.h"
#import "intf.h"

#import <vlc/vlc.h>
#import <vlc/intf.h>

#import "vlc_block.h"
#import "vlc_stream.h"
#import "vlc_xml.h"

#define UPDATE_VLC_OS "macosx"

#ifdef i386
#define UPDATE_VLC_ARCH "i386"
#else
#define UPDATE_VLC_ARCH "ppc"
#endif

#define UPDATE_VLC_STATUS_URL "http://update.videolan.org/vlc/status"
#define UPDATE_VLC_MIRRORS_URL "http://update.videolan.org/mirrors"

#define UPDATE_VLC_DOWNLOAD_BUFFER_SIZE 2048

/*****************************************************************************
 * VLCExtended implementation
 *****************************************************************************/

@implementation VLCUpdate

static VLCUpdate *_o_sharedInstance = nil;

+ (VLCUpdate *)sharedInstance
{
    return _o_sharedInstance ? _o_sharedInstance : [[self alloc] init];
}

- (id)init
{
    if (_o_sharedInstance) {
        [self dealloc];
    } else {
        _o_sharedInstance = [super init];
    }

    return _o_sharedInstance;
}

- (void)awakeFromNib
{
    /* clean the interface */
    [o_fld_userVersion setStringValue: [[[NSBundle mainBundle] infoDictionary] \
        objectForKey:@"CFBundleVersion"]];
    [o_fld_releaseNote setString: @""];
    [o_fld_size setStringValue: @""];
    [o_fld_currentVersion setStringValue: @""];
    
    [self initStrings];
}

- (void)initStrings
{
    /* translate strings to the user's language */
    [o_update_window setTitle: _NS("Check for update")];
    [o_btn_cancel setTitle: _NS("Cancel")];
    [o_btn_DownloadNow setTitle: _NS("Download now")];
    [o_btn_okay setTitle: _NS("OK")];
    [o_lbl_currentVersion setStringValue: [_NS("Current version") \
        stringByAppendingString: @":"]];
    [o_lbl_size setStringValue: [_NS("Size") \
        stringByAppendingString: @":"]];
    [o_lbl_userVersion setStringValue: [_NS("Your version") \
        stringByAppendingString: @":"]];
    [o_lbl_mirror setStringValue: [_NS("Mirror") \
        stringByAppendingString: @":"]];
    [o_lbl_checkForUpdate setStringValue: _NS("Checking for update...")];
}

- (void)showUpdateWindow
{
    /* show the window and check for a potential update */
    [o_update_window center];
    [o_update_window displayIfNeeded];
    [o_update_window makeKeyAndOrderFront:nil];
    
    /* alloc some dictionaries first */
    o_mirrors = [[NSMutableArray alloc] init];
    o_files = [[NSMutableDictionary alloc] init];
    
    [o_bar_checking startAnimation:nil];
    [self getData];
    [o_bar_checking stopAnimation:nil];
}

- (IBAction)cancel:(id)sender
{
    /* cancel the download and close the sheet */
}

- (IBAction)download:(id)sender
{
    /* open the sheet and start the download */
}

- (IBAction)okay:(id)sender
{
    /* just close the window */
    [o_update_window close];
}



- (void)getData
{
    /* This function gets all the info from the xml files hosted on
    http://update.videolan.org/ and stores it in appropriate lists.
    It was taken from the WX-interface and ported from C++ to Obj-C. */

    stream_t *p_stream = NULL;
    char *psz_eltname = NULL;
    char *psz_name = NULL;
    char *psz_value = NULL;
    char *psz_eltvalue = NULL;
    xml_t *p_xml = NULL;
    xml_reader_t *p_xml_reader = NULL;
    bool b_os = false;
    bool b_arch = false;
    
    intf_thread_t * p_intf = VLCIntf;

    if( UPDATE_VLC_ARCH == "i386" )
    {
        /* since we don't provide any binaries for MacTel atm, doing the
         * update-check is not necessary (this would fail in fact). That's why 
         * we just provide a sheet telling the user about that and skip 
         * the rest. */
        /* FIXME: remove me, if a i386-binary is available */
        NSBeginInformationalAlertSheet(_NS("Unsupported architecture"), \
                    _NS("OK"), @"", @"", o_update_window, nil, nil, nil, nil, \
                    _NS("Binary builds are only available for Mac OS X on " \
                    "the PowerPC-platform. Official builds for Intel-Macs " \
                    "are not available at this time. \n\n If you want to " \
                    "help us here, feel free to contact us."));
        return;
    }
    
    NSMutableDictionary * temp_version;
    temp_version = [[NSMutableDictionary alloc] init];
    NSMutableDictionary * temp_file;
    temp_file = [[NSMutableDictionary alloc] init];
    NSMutableDictionary * temp_mirror;
    temp_mirror = [[NSMutableDictionary alloc] init];

    //struct update_mirror_t tmp_mirror;

    p_xml = xml_Create( p_intf );
    if( !p_xml )
    {
        msg_Err( p_intf, "Failed to open XML parser" );
        // FIXME: display error message in dialog
        return;
    }

    p_stream = stream_UrlNew( p_intf, UPDATE_VLC_STATUS_URL );
    if( !p_stream )
    {
        msg_Err( p_intf, "Failed to open %s for reading",
                 UPDATE_VLC_STATUS_URL );
        // FIXME: display error message in dialog
        return;
    }

    p_xml_reader = xml_ReaderCreate( p_xml, p_stream );

    if( !p_xml_reader )
    {
        msg_Err( p_intf, "Failed to open %s for parsing",
                 UPDATE_VLC_STATUS_URL );
        // FIXME: display error message in dialog
        return;
    }

    /* build tree */
    while( xml_ReaderRead( p_xml_reader ) == 1 )
    {
        switch( xml_ReaderNodeType( p_xml_reader ) )
        {
            // Error
            case -1:
                // TODO: print message
                return;

            case XML_READER_STARTELEM:
                psz_eltname = xml_ReaderName( p_xml_reader );
                if( !psz_eltname )
                {
                    // TODO: print message
                    return;
                }
                msg_Dbg( p_intf, "element name: %s", psz_eltname );
                while( xml_ReaderNextAttr( p_xml_reader ) == VLC_SUCCESS )
                {
                    psz_name = xml_ReaderName( p_xml_reader );
                    psz_value = xml_ReaderValue( p_xml_reader );
                    if( !psz_name || !psz_value )
                    {
                        // TODO: print message
                        free( psz_eltname );
                        return;
                    }
                    msg_Dbg( p_intf, "  attribute %s = %s",
                             psz_name, psz_value );
                    
                    if( !strcmp( psz_name, "name" )
                        && ( !strcmp( psz_value, "macosx" ) 
                            || !strcmp( psz_value, "*" ) )
                        && !strcmp( psz_eltname, "os" ) )
                    {
                        b_os = true;
                    }
                    if( b_os && !strcmp( psz_name, "name" )
                        && ( !strcmp( psz_value, UPDATE_VLC_ARCH ) 
                            || !strcmp( psz_value, "*" ) )
                        && !strcmp( psz_eltname, "arch" ) )
                    {
                        b_arch = true;
                    }
                    
                    if( b_os && b_arch )
                    {
                        if( strcmp( psz_eltname, "version" ) == 0 )
                        {
                            [temp_version setObject: [NSString \
                                stringWithUTF8String: psz_value] forKey: \
                                [NSString stringWithUTF8String: psz_name]];
                        }
                        if( !strcmp( psz_eltname, "file" ) )
                        {
                            [temp_file setObject: [NSString \
                                stringWithUTF8String: psz_value] forKey: \
                                [NSString stringWithUTF8String: psz_name]];
                        }
                    }
                    free( psz_name );
                    free( psz_value );
                }
                if( ( b_os && b_arch && strcmp( psz_eltname, "arch" ) ) )
                {
                    /*if( !strcmp( psz_eltname, "version" ) )
                    {
                        it = m_versions.begin();
                        while( it != m_versions.end() )
                        {
                            if( it->type == tmp_version.type
                                && it->major == tmp_version.major
                                && it->minor == tmp_version.minor
                                && it->revision == tmp_version.revision
                                && it->extra == tmp_version.extra )
                            {
                                break;
                            }
                            it++;
                        }
                        if( it == m_versions.end() )
                        {
                            m_versions.push_back( tmp_version );
                            it = m_versions.begin();
                            while( it != m_versions.end() )
                            {
                                if( it->type == tmp_version.type
                                    && it->major == tmp_version.major
                                    && it->minor == tmp_version.minor
                                    && it->revision == tmp_version.revision
                                    && it->extra == tmp_version.extra )
                                {
                                    break;
                                }
                                it++;
                            }
                        }
                        tmp_version.type = wxT( "" );
                        tmp_version.major = wxT( "" );
                        tmp_version.minor = wxT( "" );
                        tmp_version.revision = wxT( "" );
                        tmp_version.extra = wxT( "" );
                    }
                    if( !strcmp( psz_eltname, "file" ) )
                    {
                        it->m_files.push_back( tmp_file );
                        tmp_file.type = wxT( "" );
                        tmp_file.md5 = wxT( "" );
                        tmp_file.size = wxT( "" );
                        tmp_file.url = wxT( "" );
                        tmp_file.description = wxT( "" );
                    }*/
                     
                    if(! [temp_version objectForKey: @"extra"] == @"0")
                    {
                        [o_fld_currentVersion setStringValue: [NSString \
                            stringWithFormat: @"%@.%@.%@-%@ (%@)", \
                            [temp_version objectForKey: @"major"], \
                            [temp_version objectForKey: @"minor"], \
                            [temp_version objectForKey: @"revision"], \
                            [temp_version objectForKey: @"extra"], \
                            [temp_version objectForKey: @"type"]]];
                    }
                    else
                    {
                        [o_fld_currentVersion setStringValue: [NSString \
                            stringWithFormat: @"%@.%@.%@ (%@)", \
                            [temp_version objectForKey: @"major"], \
                            [temp_version objectForKey: @"minor"], \
                            [temp_version objectForKey: @"revision"], \
                            [temp_version objectForKey: @"type"]]];
                    }
                }
                free( psz_eltname );
                break;

            case XML_READER_ENDELEM:
                psz_eltname = xml_ReaderName( p_xml_reader );
                if( !psz_eltname )
                {
                    // TODO: print message
                    return;
                }
                msg_Dbg( p_intf, "element end: %s", psz_eltname );
                if( !strcmp( psz_eltname, "os" ) )
                    b_os = false;
                if( !strcmp( psz_eltname, "arch" ) )
                    b_arch = false;
                    
                if( !strcmp( psz_eltname, "file") )
                {
                    if( [temp_file objectForKey: @"type"] == @"info" )
                    {
                        /* this is the announce file, store it correctly */
                        [o_files setObject: temp_file forKey: @"announce"];
                    }
                    else if( [temp_file objectForKey: @"type"] == @"binary" )
                    {
                        /* that's our binary */
                        [o_files setObject: temp_file forKey: @"binary"];
                    }
                    else if( [temp_file objectForKey: @"type"] == @"source" )
                    {
                        /* that's the source. not needed atm, but store it 
                         * anyway to make possible enhancement of this dialogue
                         * a bit easier */
                        [o_files setObject: temp_file forKey: @"source"];
                    }
                    
                    /* clean the temp-dict */
                    [temp_file removeAllObjects];
                }
                free( psz_eltname );
                break;

            case XML_READER_TEXT:
                /* you can check the content of a file here (e.g. \
                 * "Installer-less binaries", "Disk-Image", etc.). That's not
                 * needed on OSX atm, but print debug-info anyway. */
                psz_eltvalue = xml_ReaderValue( p_xml_reader );
                msg_Dbg( p_intf, "  text: %s", psz_eltvalue );
                free( psz_eltvalue );
                break;
        }
    }

    if( p_xml_reader && p_xml ) xml_ReaderDelete( p_xml, p_xml_reader );
    if( p_stream ) stream_Delete( p_stream );

    p_stream = stream_UrlNew( p_intf, UPDATE_VLC_MIRRORS_URL );
    if( !p_stream )
    {
        msg_Err( p_intf, "Failed to open %s for reading",
                 UPDATE_VLC_MIRRORS_URL );
        // FIXME: display error message in dialog
        return;
    }

    p_xml_reader = xml_ReaderCreate( p_xml, p_stream );

    if( !p_xml_reader )
    {
        msg_Err( p_intf, "Failed to open %s for parsing",
                 UPDATE_VLC_MIRRORS_URL );
        // FIXME: display error message in dialog
        return;
    }
    
    /* build list */
    while( xml_ReaderRead( p_xml_reader ) == 1 )
    {
        switch( xml_ReaderNodeType( p_xml_reader ) )
        {
            // Error
            case -1:
                // TODO: print message
                return;

            case XML_READER_STARTELEM:
                psz_eltname = xml_ReaderName( p_xml_reader );
                if( !psz_eltname )
                {
                    // TODO: print message
                    return;
                }
                msg_Dbg( p_intf, "element name: %s", psz_eltname );
                while( xml_ReaderNextAttr( p_xml_reader ) == VLC_SUCCESS )
                {
                    psz_name = xml_ReaderName( p_xml_reader );
                    psz_value = xml_ReaderValue( p_xml_reader );
                    if( !psz_name || !psz_value )
                    {
                        // TODO: print message
                        free( psz_eltname );
                        return;
                    }
                    msg_Dbg( p_intf, "  attribute %s = %s",
                             psz_name, psz_value );
                    
                    if( !strcmp( psz_eltname, "mirror" ) )
                    {
                        [temp_mirror setObject: [NSString stringWithUTF8String: psz_name] forKey: [NSString stringWithUTF8String: psz_value]];
                    
                        /*if( !strcmp( psz_name, "name" ) )
                            tmp_mirror.name = wxU( psz_value );
                        if( !strcmp( psz_name, "location" ) )
                            tmp_mirror.location = wxU( psz_value );*/
                    }
                    if( !strcmp( psz_eltname, "url" ) )
                    {
                        [temp_mirror setObject: [NSString stringWithUTF8String: psz_name] forKey: [NSString stringWithUTF8String: psz_value]];
                        
                        /*
                        if( !strcmp( psz_name, "type" ) )
                            tmp_mirror.type = wxU( psz_value );
                        if( !strcmp( psz_name, "base" ) )
                            tmp_mirror.base_url = wxU( psz_value );*/
                    }
                    free( psz_name );
                    free( psz_value );
                }
                /*if( !strcmp( psz_eltname, "url" ) )
                {
                    m_mirrors.push_back( tmp_mirror );
                    tmp_mirror.type = wxT( "" );
                    tmp_mirror.base_url = wxT( "" );
                }*/
                free( psz_eltname );
                break;

            case XML_READER_ENDELEM:
                psz_eltname = xml_ReaderName( p_xml_reader );
                if( !psz_eltname )
                {
                    // TODO: print message
                    return;
                }
                msg_Dbg( p_intf, "element end: %s", psz_eltname );
                /*if( !strcmp( psz_eltname, "mirror" ) )
                {
                    tmp_mirror.name = wxT( "" );
                    tmp_mirror.location = wxT( "" );
                }*/
                
                /* store our mirror correctly */
                [o_mirrors addObject: temp_mirror];
                [temp_mirror removeAllObjects];
                
                free( psz_eltname );
                break;

            case XML_READER_TEXT:
                psz_eltvalue = xml_ReaderValue( p_xml_reader );
                msg_Dbg( p_intf, "  text: %s", psz_eltvalue );
                free( psz_eltvalue );
                break;
        }
    }


    if( p_xml_reader && p_xml ) xml_ReaderDelete( p_xml, p_xml_reader );
    if( p_stream ) stream_Delete( p_stream );
    if( p_xml ) xml_Delete( p_xml );
}

@end
