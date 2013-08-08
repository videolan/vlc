/*****************************************************************************
 * sgimb.c: a meta demux to parse sgimb referrer files
 *****************************************************************************
 * Copyright (C) 2004 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Derk-Jan Hartman <hartman at videolan dot org>
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
 * This is a metademux for the Kasenna MediaBase metafile format.
 * Kasenna MediaBase first returns this file when you are trying to access
 * their MPEG streams (MIME: application/x-sgimb). Very few applications
 * understand this format and the format is not really documented on the net.
 * Following a typical MediaBase file. Notice the sgi prefix of all the elements.
 * This stems from the fact that the MediaBase servers were first introduced by SGI?????.
 *
 * sgiNameServerHost=host.name.tld
 *     Obvious: the host hosting this stream
 * Stream="xdma://host.name.tld/demo/a_very_cool.mpg"
 *     Not always present. xdma can be read as RTSP.
 * sgiMovieName=/demo/a_very_cool.mpg
 *     The path to the asset
 * sgiAuxState=1|2
 *     AuxState=2 is always Video On Demand (so not Scheduled)
 *     Not present with Live streams
 * sgiLiveFeed=True|False
 *     Denounces if the stream is live or from assets (Canned?)
 *     Live appears as a little sattelite dish in the web interface of Kasenna
 * sgiFormatName=PARTNER_41_MPEG-4
 *     The type of stream. One of:
 *       PARTNER_41_MPEG-4 (RTSP MPEG-4 fully compliant)
 *       MPEG1-Audio       (MP3 Audio streams in MPEG TS)
 *       MPEG-1            (MPEG 1 A/V in MPEG TS)
 *       MPEG-2            (MPEG 2 A/V in MPEG TS)
 * sgiWidth=720
 *     The width of the to be received stream. Only present if stream is not Live.
 * sgiHeight=576
 *     The height of the to be received stream. Only present if stream is not Live.
 * sgiBitrate=1630208
 *     The bitrate of the to be received stream. Only present if stream is not Live.
 * sgiDuration=378345000
 *     The duration of the to be received stream. Only present if stream is not Live.
 * sgiQTFileBegin
 * rtsptext
 * rtsp://host.name.tld/demo/a_very_cool.mpg
 * sgiQTFileEnd
 *     Sometimes present. QT will recognize this as a RTSP reference file, if present.
 * sgiApplicationName=MediaBaseURL
 *     Beats me !! :)
 * sgiElapsedTime=0
 *     Time passed since the asset was started (resets for repeating non live assets?)
 * sgiMulticastAddress=233.81.233.15
 *     The multicast IP used for the Multicast feed.
 *     Also defines if a stream is multicast or not. (blue dot in kasenna web interface)
 * sgiMulticastPort=1234
 *     The multicast port for the same Multicast feed.
 * sgiPacketSize=16384
 *     The packetsize of the UDP frames that Kasenna sends. They should have used a default
 *     that is a multiple of 188 (TS frame size). Most networks don't support more than 1500 anyways.
 *     Also, when you lose a frame of this size, imagecorruption is more likely then with smaller
 *     frames.
 * sgiServerVersion=6.1.2
 *     Version of the server
 * sgiRtspPort=554
 *     TCP port used for RTSP communication
 * AutoStart=True
 *     Start playing automatically
 * DeliveryService=cds
 *     Simulcasted (scheduled unicast) content. (Green dot in Kasenna web interface)
 * sgiShowingName=A nice name that everyone likes
 *     A human readible descriptive title for this stream.
 * sgiSid=2311
 *     Looks like this is the ID of the scheduled asset?
 * sgiUserAccount=pid=1724&time=1078527309&displayText=You%20are%20logged%20as%20guest&
 *     User Authentication. Above is a default guest entry. Not required for RTSP communication.
 * sgiUserPassword=
 *     Password :)
 *
 *****************************************************************************/


/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_demux.h>
#include "playlist.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
#define MAX_LINE 1024

struct demux_sys_t
{
    char        *psz_uri;       /* Stream= or sgiQTFileBegin rtsp link */
    char        *psz_server;    /* sgiNameServerHost= */
    char        *psz_location;  /* sgiMovieName= */
    char        *psz_name;      /* sgiShowingName= */
    char        *psz_user;      /* sgiUserAccount= */
    char        *psz_password;  /* sgiUserPassword= */
    char        *psz_mcast_ip;  /* sgiMulticastAddress= */
    int         i_mcast_port;   /* sgiMulticastPort= */
    int         i_packet_size;  /* sgiPacketSize= */
    mtime_t     i_duration;     /* sgiDuration= */
    int         i_port;         /* sgiRtspPort= */
    int         i_sid;          /* sgiSid= */
    bool  b_concert;      /* DeliveryService=cds */
    bool  b_rtsp_kasenna; /* kasenna style RTSP */
};

static int Demux ( demux_t *p_demux );

/*****************************************************************************
 * Activate: initializes m3u demux structures
 *****************************************************************************/
int Import_SGIMB( vlc_object_t * p_this )
{
    demux_t *p_demux = (demux_t *)p_this;
    const uint8_t *p_peek;
    int i_size;

    /* Lets check the content to see if this is a sgi mediabase file */
    i_size = stream_Peek( p_demux->s, &p_peek, MAX_LINE );
    i_size -= sizeof("sgiNameServerHost=") - 1;
    if ( i_size > 0 )
    {
        unsigned int i_len = sizeof("sgiNameServerHost=") - 1;
        while ( i_size && strncasecmp( (char *)p_peek, "sgiNameServerHost=", i_len ) )
        {
            p_peek++;
            i_size--;
        }
        if ( !strncasecmp( (char *)p_peek, "sgiNameServerHost=", i_len ) )
        {
            STANDARD_DEMUX_INIT_MSG( "using SGIMB playlist reader" );
            p_demux->p_sys->psz_uri = NULL;
            p_demux->p_sys->psz_server = NULL;
            p_demux->p_sys->psz_location = NULL;
            p_demux->p_sys->psz_name = NULL;
            p_demux->p_sys->psz_user = NULL;
            p_demux->p_sys->psz_password = NULL;
            p_demux->p_sys->psz_mcast_ip = NULL;
            p_demux->p_sys->i_mcast_port = 0;
            p_demux->p_sys->i_packet_size = 0;
            p_demux->p_sys->i_duration = 0;
            p_demux->p_sys->i_port = 0;
            p_demux->p_sys->i_sid = 0;
            p_demux->p_sys->b_rtsp_kasenna = false;
            p_demux->p_sys->b_concert = false;

            return VLC_SUCCESS;
        }
    }
    return VLC_EGENERIC;
}

/*****************************************************************************
 * Deactivate: frees unused data
 *****************************************************************************/
void Close_SGIMB( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys = p_demux->p_sys;
    free( p_sys->psz_uri );
    free( p_sys->psz_server );
    free( p_sys->psz_location );
    free( p_sys->psz_name );
    free( p_sys->psz_user );
    free( p_sys->psz_password );
    free( p_sys->psz_mcast_ip );
    free( p_demux->p_sys );
    return;
}

static int ParseLine ( demux_t *p_demux, char *psz_line )
{
    char        *psz_bol;
    demux_sys_t *p_sys = p_demux->p_sys;

    psz_bol = psz_line;

    /* Remove unnecessary tabs or spaces at the beginning of line */
    while( *psz_bol == ' ' || *psz_bol == '\t' ||
           *psz_bol == '\n' || *psz_bol == '\r' )
    {
        psz_bol++;
    }

    if( !strncasecmp( psz_bol, "rtsp://", sizeof("rtsp://") - 1 ) )
    {
        /* We found the link, it was inside a sgiQTFileBegin */
        free( p_sys->psz_uri );
        p_sys->psz_uri = strdup( psz_bol );
    }
    else if( !strncasecmp( psz_bol, "Stream=\"", sizeof("Stream=\"") - 1 ) )
    {
        psz_bol += sizeof("Stream=\"") - 1;
        char* psz_tmp = strrchr( psz_bol, '"' );
        if( !psz_tmp )
            return 0;
        psz_tmp[0] = '\0';
        /* We cheat around xdma. for some reason xdma links work different then rtsp */
        if( !strncasecmp( psz_bol, "xdma://", sizeof("xdma://") - 1 ) )
        {
            psz_bol[0] = 'r';
            psz_bol[1] = 't';
            psz_bol[2] = 's';
            psz_bol[3] = 'p';
        }
        free( p_sys->psz_uri );
        p_sys->psz_uri = strdup( psz_bol );
    }
    else if( !strncasecmp( psz_bol, "sgiNameServerHost=", sizeof("sgiNameServerHost=") - 1 ) )
    {
        psz_bol += sizeof("sgiNameServerHost=") - 1;
        free( p_sys->psz_server );
        p_sys->psz_server = strdup( psz_bol );
    }
    else if( !strncasecmp( psz_bol, "sgiMovieName=", sizeof("sgiMovieName=") - 1 ) )
    {
        psz_bol += sizeof("sgiMovieName=") - 1;
        free( p_sys->psz_location );
        p_sys->psz_location = strdup( psz_bol );
    }
    else if( !strncasecmp( psz_bol, "sgiUserAccount=", sizeof("sgiUserAccount=") - 1 ) )
    {
        psz_bol += sizeof("sgiUserAccount=") - 1;
        free( p_sys->psz_user );
        p_sys->psz_user = strdup( psz_bol );
    }
    else if( !strncasecmp( psz_bol, "sgiUserPassword=", sizeof("sgiUserPassword=") - 1 ) )
    {
        psz_bol += sizeof("sgiUserPassword=") - 1;
        free( p_sys->psz_password );
        p_sys->psz_password = strdup( psz_bol );
    }
    else if( !strncasecmp( psz_bol, "sgiShowingName=", sizeof("sgiShowingName=") - 1 ) )
    {
        psz_bol += sizeof("sgiShowingName=") - 1;
        free( p_sys->psz_name );
        p_sys->psz_name = strdup( psz_bol );
    }
    else if( !strncasecmp( psz_bol, "sgiFormatName=", sizeof("sgiFormatName=") - 1 ) )
    {
        psz_bol += sizeof("sgiFormatName=") - 1;
        if( strcasestr( psz_bol, "MPEG-4") == NULL ) /*not mpeg4 found in string */
            p_sys->b_rtsp_kasenna = true;
    }
    else if( !strncasecmp( psz_bol, "sgiMulticastAddress=", sizeof("sgiMulticastAddress=") - 1 ) )
    {
        psz_bol += sizeof("sgiMulticastAddress=") - 1;
        free( p_sys->psz_mcast_ip );
        p_sys->psz_mcast_ip = strdup( psz_bol );
    }
    else if( !strncasecmp( psz_bol, "sgiMulticastPort=", sizeof("sgiMulticastPort=") - 1 ) )
    {
        psz_bol += sizeof("sgiMulticastPort=") - 1;
        p_sys->i_mcast_port = (int) strtol( psz_bol, NULL, 0 );
    }
    else if( !strncasecmp( psz_bol, "sgiPacketSize=", sizeof("sgiPacketSize=") - 1 ) )
    {
        psz_bol += sizeof("sgiPacketSize=") - 1;
        p_sys->i_packet_size = (int) strtol( psz_bol, NULL, 0 );
    }
    else if( !strncasecmp( psz_bol, "sgiDuration=", sizeof("sgiDuration=") - 1 ) )
    {
        psz_bol += sizeof("sgiDuration=") - 1;
        p_sys->i_duration = (mtime_t) strtol( psz_bol, NULL, 0 );
    }
    else if( !strncasecmp( psz_bol, "sgiRtspPort=", sizeof("sgiRtspPort=") - 1 ) )
    {
        psz_bol += sizeof("sgiRtspPort=") - 1;
        p_sys->i_port = (int) strtol( psz_bol, NULL, 0 );
    }
    else if( !strncasecmp( psz_bol, "sgiSid=", sizeof("sgiSid=") - 1 ) )
    {
        psz_bol += sizeof("sgiSid=") - 1;
        p_sys->i_sid = (int) strtol( psz_bol, NULL, 0 );
    }
    else if( !strncasecmp( psz_bol, "DeliveryService=cds", sizeof("DeliveryService=cds") - 1 ) )
    {
        p_sys->b_concert = true;
    }
    else
    {
        /* This line isn't really important */
        return 0;
    }
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Demux: reads and demuxes data packets
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, 1 otherwise
 *****************************************************************************/
static int Demux ( demux_t *p_demux )
{
    demux_sys_t     *p_sys = p_demux->p_sys;
    input_item_t    *p_child = NULL;
    char            *psz_line;

    input_item_t *p_current_input = GetCurrentItem(p_demux);

    while( ( psz_line = stream_ReadLine( p_demux->s ) ) )
    {
        ParseLine( p_demux, psz_line );
        free( psz_line );
    }

    if( p_sys->psz_mcast_ip )
    {
        /* Definetly schedules multicast session */
        /* We don't care if it's live or not */
        free( p_sys->psz_uri );
        if( asprintf( &p_sys->psz_uri, "udp://@" "%s:%i", p_sys->psz_mcast_ip, p_sys->i_mcast_port ) == -1 )
        {
            p_sys->psz_uri = NULL;
            return -1;
        }
    }

    if( p_sys->psz_uri == NULL )
    {
        if( p_sys->psz_server && p_sys->psz_location )
        {
            if( asprintf( &p_sys->psz_uri, "rtsp://" "%s:%i%s",
                     p_sys->psz_server, p_sys->i_port > 0 ? p_sys->i_port : 554, p_sys->psz_location ) == -1 )
            {
                p_sys->psz_uri = NULL;
                return -1;
            }
        }
    }

    if( p_sys->b_concert )
    {
        /* It's definetly a simulcasted scheduled stream */
        /* We don't care if it's live or not */
        if( p_sys->psz_uri == NULL )
        {
            msg_Err( p_demux, "no URI was found" );
            return -1;
        }

        char *uri;
        if( asprintf( &uri, "%s%%3FMeDiAbAsEshowingId=%d%%26MeDiAbAsEconcert"
                      "%%3FMeDiAbAsE", p_sys->psz_uri, p_sys->i_sid ) == -1 )
            return -1;
        free( p_sys->psz_uri );
        p_sys->psz_uri = uri;
    }

    p_child = input_item_NewWithType( p_sys->psz_uri,
                      p_sys->psz_name ? p_sys->psz_name : p_sys->psz_uri,
                      0, NULL, 0, p_sys->i_duration, ITEM_TYPE_NET );

    if( !p_child )
    {
        msg_Err( p_demux, "A valid playlistitem could not be created" );
        return -1;
    }

    input_item_CopyOptions( p_current_input, p_child );
    if( p_sys->i_packet_size && p_sys->psz_mcast_ip )
    {
        char *psz_option;
        p_sys->i_packet_size += 1000;
        if( asprintf( &psz_option, "mtu=%i", p_sys->i_packet_size ) != -1 )
        {
            input_item_AddOption( p_child, psz_option, VLC_INPUT_OPTION_TRUSTED );
            free( psz_option );
        }
    }
    if( !p_sys->psz_mcast_ip )
        input_item_AddOption( p_child, "rtsp-caching=5000", VLC_INPUT_OPTION_TRUSTED );
    if( !p_sys->psz_mcast_ip && p_sys->b_rtsp_kasenna )
        input_item_AddOption( p_child, "rtsp-kasenna", VLC_INPUT_OPTION_TRUSTED );

    input_item_PostSubItem( p_current_input, p_child );
    vlc_gc_decref( p_child );
    vlc_gc_decref(p_current_input);
    return 0; /* Needed for correct operation of go back */
}
