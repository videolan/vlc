/*****************************************************************************
 * dash.cpp: DASH module
 *****************************************************************************
 * Copyright © 2010 - 2011 Klagenfurt University
 *
 * Created on: Aug 10, 2010
 * Authors: Christopher Mueller <christopher.mueller@itec.uni-klu.ac.at>
 *          Christian Timmerer  <christian.timmerer@itec.uni-klu.ac.at>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
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

#include <errno.h>

#include "DASHManager.h"
#include "xml/DOMParser.h"
#include "http/HTTPConnectionManager.h"
#include "adaptationlogic/IAdaptationLogic.h"
#include "mpd/MPDFactory.h"

#define SEEK 0

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open    (vlc_object_t *);
static void Close   (vlc_object_t *);

#define DASH_WIDTH_TEXT N_("Preferred Width")
#define DASH_WIDTH_LONGTEXT N_("Preferred Width")

#define DASH_HEIGHT_TEXT N_("Preferred Height")
#define DASH_HEIGHT_LONGTEXT N_("Preferred Height")

#define DASH_BUFFER_TEXT N_("Buffer Size (Seconds)")
#define DASH_BUFFER_LONGTEXT N_("Buffer size in seconds")

vlc_module_begin ()
        set_shortname( N_("DASH"))
        set_description( N_("Dynamic Adaptive Streaming over HTTP") )
        set_capability( "stream_filter", 19 )
        set_category( CAT_INPUT )
        set_subcategory( SUBCAT_INPUT_STREAM_FILTER )
        add_integer( "dash-prefwidth",  480, DASH_WIDTH_TEXT,  DASH_WIDTH_LONGTEXT,  true )
        add_integer( "dash-prefheight", 360, DASH_HEIGHT_TEXT, DASH_HEIGHT_LONGTEXT, true )
        add_integer( "dash-buffersize", 30, DASH_BUFFER_TEXT, DASH_BUFFER_LONGTEXT, true )
        set_callbacks( Open, Close )
vlc_module_end ()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
struct stream_sys_t
{
        dash::DASHManager   *p_dashManager;
        dash::mpd::MPD      *p_mpd;
        uint64_t                            position;
        bool                                isLive;
};

static int  Read            (stream_t *p_stream, void *p_ptr, unsigned int i_len);
static int  Peek            (stream_t *p_stream, const uint8_t **pp_peek, unsigned int i_peek);
static int  Control         (stream_t *p_stream, int i_query, va_list args);

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open(vlc_object_t *p_obj)
{
    stream_t *p_stream = (stream_t*) p_obj;

    if(!dash::xml::DOMParser::isDash(p_stream->p_source))
        return VLC_EGENERIC;

    //Build a XML tree
    dash::xml::DOMParser        parser(p_stream->p_source);
    if( !parser.parse() )
    {
        msg_Dbg( p_stream, "Could not parse mpd file." );
        return VLC_EGENERIC;
    }

    //Begin the actual MPD parsing:
    dash::mpd::MPD *mpd = dash::mpd::MPDFactory::create(parser.getRootNode(), p_stream->p_source, parser.getProfile());

    if(mpd == NULL)
        return VLC_EGENERIC;

    stream_sys_t        *p_sys = (stream_sys_t *) malloc(sizeof(stream_sys_t));
    if (unlikely(p_sys == NULL))
        return VLC_ENOMEM;

    p_sys->p_mpd = mpd;
    dash::DASHManager*p_dashManager = new dash::DASHManager(p_sys->p_mpd,
                                          dash::logic::IAdaptationLogic::RateBased,
                                          p_stream);

    if(!p_dashManager->start())
    {
        delete p_dashManager;
        free( p_sys );
        return VLC_EGENERIC;
    }
    p_sys->p_dashManager    = p_dashManager;
    p_sys->position         = 0;
    p_sys->isLive           = p_dashManager->getMpdManager()->getMPD()->isLive();
    p_stream->p_sys         = p_sys;
    p_stream->pf_read       = Read;
    p_stream->pf_peek       = Peek;
    p_stream->pf_control    = Control;

    msg_Dbg(p_obj,"opening mpd file (%s)", p_stream->psz_path);

    return VLC_SUCCESS;
}
/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close(vlc_object_t *p_obj)
{
    stream_t                            *p_stream       = (stream_t*) p_obj;
    stream_sys_t                        *p_sys          = (stream_sys_t *) p_stream->p_sys;
    dash::DASHManager                   *p_dashManager  = p_sys->p_dashManager;

    delete(p_dashManager);
    free(p_sys);
}
/*****************************************************************************
 * Callbacks:
 *****************************************************************************/
static int  Seek            ( stream_t *p_stream, uint64_t pos )
{
    stream_sys_t        *p_sys          = (stream_sys_t *) p_stream->p_sys;
    dash::DASHManager   *p_dashManager  = p_sys->p_dashManager;
    int                 i_ret           = 0;
    unsigned            i_len           = 0;
    long                i_read          = 0;

    if( pos < p_sys->position )
    {
        if( p_sys->position - pos > UINT_MAX )
        {
            msg_Err( p_stream, "Cannot seek backward that far!" );
            return VLC_EGENERIC;
        }
        i_len = p_sys->position - pos;
        i_ret = p_dashManager->seekBackwards( i_len );
        if( i_ret == VLC_EGENERIC )
        {
            msg_Err( p_stream, "Cannot seek backward outside the current block :-/" );
            return VLC_EGENERIC;
        }
        else
            return VLC_SUCCESS;
    }

    /* Seek forward */
    if( pos - p_sys->position > UINT_MAX )
    {
        msg_Err( p_stream, "Cannot seek forward that far!" );
        return VLC_EGENERIC;
    }
    i_len = pos - p_sys->position;
    i_read = Read( p_stream, (void *)NULL, i_len );
    if( (unsigned)i_read == i_len )
        return VLC_SUCCESS;
    else
        return VLC_EGENERIC;
}

static int  Read            (stream_t *p_stream, void *p_ptr, unsigned int i_len)
{
    stream_sys_t        *p_sys          = (stream_sys_t *) p_stream->p_sys;
    dash::DASHManager   *p_dashManager  = p_sys->p_dashManager;
    uint8_t             *p_buffer       = (uint8_t*)p_ptr;
    int                 i_ret           = 0;
    int                 i_read          = 0;

    while( i_len > 0 )
    {
        i_read = p_dashManager->read( p_buffer, i_len );
        if( i_read < 0 )
            break;
        p_buffer += i_read;
        i_ret += i_read;
        i_len -= i_read;
    }
    p_buffer -= i_ret;

    if (i_read < 0)
    {
        switch (errno)
        {
            case EINTR:
            case EAGAIN:
                break;
            default:
                msg_Dbg(p_stream, "DASH Read: failed to read (%m)");
                return 0;
        }
        return 0;
    }

    p_sys->position += i_ret;

    return i_ret;
}

static int  Peek            (stream_t *p_stream, const uint8_t **pp_peek, unsigned int i_peek)
{
    stream_sys_t        *p_sys          = (stream_sys_t *) p_stream->p_sys;
    dash::DASHManager   *p_dashManager  = p_sys->p_dashManager;

    return p_dashManager->peek( pp_peek, i_peek );
}

static int  Control         (stream_t *p_stream, int i_query, va_list args)
{
    stream_sys_t *p_sys = p_stream->p_sys;

    switch (i_query)
    {
        case STREAM_CAN_SEEK:
        case STREAM_CAN_FASTSEEK:
            /*TODO Support Seek */
            *(va_arg (args, bool *)) = SEEK;
            break;
        case STREAM_CAN_PAUSE:
        case STREAM_CAN_CONTROL_PACE:
            *(va_arg (args, bool *)) = false; /* TODO */
            break;

        case STREAM_GET_POSITION:
            *(va_arg (args, uint64_t *)) = p_sys->position;
            break;
        case STREAM_SET_POSITION:
        {
            uint64_t pos = (uint64_t)va_arg(args, uint64_t);
            if(Seek(p_stream, pos) == VLC_SUCCESS)
            {
                p_sys->position = pos;
                break;
            }
            else
                return VLC_EGENERIC;
        }
        case STREAM_GET_SIZE:
        {
            uint64_t*   res = (va_arg (args, uint64_t *));
            if(p_sys->isLive)
                *res = 0;
            else
            {
                const dash::mpd::Representation *rep = p_sys->p_dashManager->getAdaptionLogic()->getCurrentRepresentation();
                if ( rep == NULL )
                    *res = 0;
                else
                    *res = p_sys->p_mpd->getDuration() * rep->getBandwidth() / 8;
            }
            break;
        }
        default:
            return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}
