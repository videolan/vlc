/*****************************************************************************
 * eyetv.m : Access module to connect to our plugin running within EyeTV
 *****************************************************************************
 * Copyright (C) 2006-2007 VLC authors and VideoLAN
 * $Id$
 *
 * Author: Felix Kühne <fkuehne at videolan dot org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_access.h>

#include <vlc_network.h>
#include <vlc_interrupt.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#import <Foundation/Foundation.h>

#define MTU 65535

/* TODO:
 * watch for PluginQuit or DeviceRemoved to stop output to VLC's core then */

/*****************************************************************************
 * Module descriptior
 *****************************************************************************/
static int  Open (vlc_object_t *);
static void Close(vlc_object_t *);

#define CHANNEL_TEXT N_("Channel number")
#define CHANNEL_LONGTEXT N_(\
    "EyeTV program number, or use 0 for last channel, " \
    "-1 for S-Video input, -2 for Composite input")

vlc_module_begin ()
    set_shortname("EyeTV")
    set_description(N_("EyeTV input"))
    set_category(CAT_INPUT)
    set_subcategory(SUBCAT_INPUT_ACCESS)

    add_integer("eyetv-channel", 0,
                 CHANNEL_TEXT, CHANNEL_LONGTEXT, false)

    set_capability("access", 0)
    add_shortcut("eyetv")
    set_callbacks(Open, Close)
vlc_module_end ()

/*****************************************************************************
 * Access: local prototypes
 *****************************************************************************/
struct access_sys_t
{
    int eyetvSock;
};

static block_t *BlockRead(access_t *, bool *);
static int Control(access_t *, int, va_list);

static void selectChannel(vlc_object_t *p_this, int theChannelNum)
{
    @autoreleasepool {
        NSAppleScript *script;
        switch(theChannelNum) {
            case -2: // Composite
                script = [[NSAppleScript alloc] initWithSource:
                          @"tell application \"EyeTV\"\n"
                          "  input_change input source composite video input\n"
                          "  volume_change level 0\n"
                          "  show player_window\n"
                          "  tell application \"System Events\" to set visible of process \"EyeTV\" to false\n"
                          "end tell"];
                break;
            case -1: // S-Video
                script = [[NSAppleScript alloc] initWithSource:
                          @"tell application \"EyeTV\"\n"
                          "  input_change input source S video input\n"
                          "  volume_change level 0\n"
                          "  show player_window\n"
                          "  tell application \"System Events\" to set visible of process \"EyeTV\" to false\n"
                          "end tell"];
                break;
            case 0: // Last
                script = [[NSAppleScript alloc] initWithSource:
                          @"tell application \"EyeTV\"\n"
                          "  volume_change level 0\n"
                          "  show player_window\n"
                          "  tell application \"System Events\" to set visible of process \"EyeTV\" to false\n"
                          "end tell"];
                break;
            default:
                if (theChannelNum > 0) {
                    NSString *channel_change = [NSString stringWithFormat:
                                                @"tell application \"EyeTV\"\n"
                                                "  channel_change channel number %d\n"
                                                "  volume_change level 0\n"
                                                "  show player_window\n"
                                                "  tell application \"System Events\" to set visible of process \"EyeTV\" to false\n"
                                                "end tell", theChannelNum];
                    script = [[NSAppleScript alloc] initWithSource:channel_change];
                }
                else
                    return;
        }
        NSDictionary *errorDict;
        NSAppleEventDescriptor *descriptor = [script executeAndReturnError:&errorDict];
        if (nil == descriptor) {
            NSString *errorString = [errorDict objectForKey:NSAppleScriptErrorMessage];
            msg_Err(p_this, "EyeTV source change failed with error status '%s'", [errorString UTF8String]);
        }
        [script release];
    }
}

/*****************************************************************************
 * Open: sets up the module and its threads
 *****************************************************************************/
static int Open(vlc_object_t *p_this)
{
    access_t        *p_access = (access_t *)p_this;
    access_sys_t    *p_sys;

    struct sockaddr_un publicAddr, peerAddr;
    int publicSock;

    /* Init p_access */
    ACCESS_SET_CALLBACKS(NULL, BlockRead, Control, NULL);
    p_sys = p_access->p_sys = calloc(1, sizeof(access_sys_t));
    if (!p_sys)
        return VLC_ENOMEM;

    msg_Dbg(p_access, "coming up");
    selectChannel(p_this, var_InheritInteger(p_access, "eyetv-channel"));

    /* socket */
    memset(&publicAddr, 0, sizeof(publicAddr));
    publicAddr.sun_family = AF_UNIX;
    strncpy(publicAddr.sun_path, "/tmp/.vlc-eyetv-bridge", sizeof(publicAddr.sun_path)-1);
    /* remove previous public path if it wasn't cleanly removed */
    if ((0 != unlink(publicAddr.sun_path)) && (ENOENT != errno)) {
        msg_Err(p_access, "local socket path is not usable (errno=%d)", errno);
        free(p_sys);
        return VLC_EGENERIC;
    }

    publicSock = vlc_socket(PF_UNIX, SOCK_STREAM, 0, false);
    if (publicSock == -1) {
        msg_Err(p_access, "create local socket failed (errno=%d)", errno);
        free(p_sys);
        return VLC_EGENERIC;
    }

    if (bind(publicSock, (struct sockaddr *)&publicAddr, sizeof(struct sockaddr_un)) == -1) {
        msg_Err(p_access, "bind local socket failed (errno=%d)", errno);
        vlc_close(publicSock);
        free(p_sys);
        return VLC_EGENERIC;
    }

    /* we are not expecting more than one connection */
    if (listen(publicSock, 1) == -1) {
        msg_Err(p_access, "cannot accept connection (errno=%d)", errno);
        vlc_close(publicSock);
        free(p_sys);
        return VLC_EGENERIC;
    } else {
        socklen_t peerSockLen = sizeof(struct sockaddr_un);
        int peerSock;

        /* tell the EyeTV plugin to open start sending */
        CFNotificationCenterPostNotification(CFNotificationCenterGetDistributedCenter (),
                                              CFSTR("VLCAccessStartDataSending"),
                                              CFSTR("VLCEyeTVSupport"),
                                              /*userInfo*/ NULL,
                                              TRUE);

        msg_Dbg(p_access, "plugin notified");

        peerSock = accept(publicSock, (struct sockaddr *)&peerAddr, &peerSockLen);
        if (peerSock == -1) {
            msg_Err(p_access, "cannot wait for connection (errno=%d)", errno);
            vlc_close(publicSock);
            free(p_sys);
            return VLC_EGENERIC;
        }

        msg_Dbg(p_access, "plugin connected");

        p_sys->eyetvSock = peerSock;

        /* remove public access */
        vlc_close(publicSock);
        unlink(publicAddr.sun_path);
    }
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: closes msg-port, free resources
 *****************************************************************************/
static void Close(vlc_object_t *p_this)
{
    access_t     *p_access = (access_t *)p_this;
    access_sys_t *p_sys = p_access->p_sys;

    msg_Dbg(p_access, "closing");

    /* tell the EyeTV plugin to close its msg port and stop sending */
    CFNotificationCenterPostNotification(CFNotificationCenterGetDistributedCenter (),
                                          CFSTR("VLCAccessStopDataSending"),
                                          CFSTR("VLCEyeTVSupport"),
                                          /*userInfo*/ NULL,
                                          TRUE);
    msg_Dbg(p_access, "plugin notified");

    vlc_close(p_sys->eyetvSock);
    msg_Dbg(p_access, "msg port closed and freed");

    free(p_sys);
}

/*****************************************************************************
* BlockRead: forwarding data from EyeTV plugin which was received above
*****************************************************************************/
static block_t *BlockRead(access_t *p_access, bool *restrict eof)
{
    access_sys_t *p_sys = p_access->p_sys;
    block_t      *p_block;
    ssize_t len;

    (void) eof;

    /* Read data */
    p_block = block_Alloc(MTU);
    len = vlc_read_i11e(p_sys->eyetvSock, p_block->p_buffer, MTU);

    if (len < 0) {
        block_Release(p_block);
        return NULL;
    }

    return block_Realloc(p_block, 0, p_block->i_buffer = len);
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control(access_t *p_access, int i_query, va_list args)
{
    bool   *pb_bool;
    int          *pi_int;
    int64_t      *pi_64;
    access_sys_t  *p_sys = (access_sys_t *) p_access->p_sys;

    switch(i_query) {
        case STREAM_CAN_SEEK:
        case STREAM_CAN_FASTSEEK:
            pb_bool = (bool*)va_arg(args, bool*);
            *pb_bool = false;
            break;
        case STREAM_CAN_PAUSE:
            pb_bool = (bool*)va_arg(args, bool*);
            *pb_bool = false;
            break;
        case STREAM_CAN_CONTROL_PACE:
            pb_bool = (bool*)va_arg(args, bool*);
            *pb_bool = false;
            break;
        case STREAM_GET_PTS_DELAY:
            pi_64 = (int64_t*)va_arg(args, int64_t *);
            *pi_64 =
                INT64_C(1000) * var_InheritInteger(p_access, "live-caching");
            break;
        default:
            return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}
