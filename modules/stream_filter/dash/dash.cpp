/*****************************************************************************
 * dash.cpp: DASH module
 *****************************************************************************
 * Copyright © 2010 - 2011 Klagenfurt University
 *
 * Created on: Aug 10, 2010
 * Authors: Christopher Mueller <christopher.mueller@itec.uni-klu.ac.at>
 *          Christian Timmerer  <christian.timmerer@itec.uni-klu.ac.at>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
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

#define SEEK 0

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open    (vlc_object_t *);
static void Close   (vlc_object_t *);

vlc_module_begin ()
        set_shortname( N_("DASH"))
        set_description( N_("Dynamic Adaptive Streaming over HTTP") )
        set_capability( "stream_filter", 19 )
        set_category( CAT_INPUT )
        set_subcategory( SUBCAT_INPUT_STREAM_FILTER )
        set_callbacks( Open, Close )
vlc_module_end ()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
struct stream_sys_t
{
        dash::DASHManager                   *p_dashManager;
        dash::http::HTTPConnectionManager   *p_conManager;
        dash::xml::Node                     *p_node;
        int                                 position;
        bool                                isLive;
};

static int  Read            (stream_t *p_stream, void *p_buffer, unsigned int i_len);
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

    dash::xml::DOMParser parser(p_stream->p_source);
    if(!parser.parse())
    {
        msg_Dbg(p_stream, "could not parse mpd file");
        return VLC_EGENERIC;
    }

    stream_sys_t *p_sys = (stream_sys_t *) malloc(sizeof(stream_sys_t));

    if (unlikely(p_sys == NULL))
        return VLC_ENOMEM;

    dash::http::HTTPConnectionManager *p_conManager =
        new dash::http::HTTPConnectionManager(p_stream);
    dash::xml::Node *p_node = parser.getRootNode();
    dash::DASHManager*p_dashManager =
        new dash::DASHManager( p_conManager, p_node,
                              dash::logic::IAdaptationLogic::RateBased );

    p_sys->p_dashManager    = p_dashManager;
    p_sys->p_node           = p_node;
    p_sys->p_conManager     = p_conManager;
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
    dash::http::HTTPConnectionManager   *p_conManager   = p_sys->p_conManager;

    delete(p_conManager);
    delete(p_dashManager);
    free(p_sys);
}
/*****************************************************************************
 * Callbacks:
 *****************************************************************************/
static int  Read            (stream_t *p_stream, void *p_buffer, unsigned int i_len)
{
    stream_sys_t        *p_sys          = (stream_sys_t *) p_stream->p_sys;
    dash::DASHManager   *p_dashManager  = p_sys->p_dashManager;
    int                 i_ret           = 0;

    i_ret = p_dashManager->read(p_buffer, i_len);

    if (i_ret < 0)
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

    return p_dashManager->peek(pp_peek, i_peek);
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
        case STREAM_GET_POSITION:
            *(va_arg (args, uint64_t *)) = p_sys->position;
            break;
        case STREAM_SET_POSITION:
            return VLC_EGENERIC;
        case STREAM_GET_SIZE:
            if(p_sys->isLive)
                *(va_arg (args, uint64_t *)) = 0;
            break;
        default:
            return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}
