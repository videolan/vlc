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
#define __STDC_CONSTANT_MACROS 1

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdint.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_demux.h>
#include <vlc_meta.h>

#include <errno.h>

#include "dash.hpp"
#include "xml/DOMParser.h"
#include "mpd/MPDFactory.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open    (vlc_object_t *);
static void Close   (vlc_object_t *);

#define DASH_WIDTH_TEXT N_("Preferred Width")
#define DASH_WIDTH_LONGTEXT N_("Preferred Width")

#define DASH_HEIGHT_TEXT N_("Preferred Height")
#define DASH_HEIGHT_LONGTEXT N_("Preferred Height")

#define DASH_BW_TEXT N_("Fixed Bandwidth in KiB/s")
#define DASH_BW_LONGTEXT N_("Preferred bandwidth for non adaptative streams")

#define DASH_LOGIC_TEXT N_("Adaptation Logic")

static const int pi_logics[] = {dash::logic::AbstractAdaptationLogic::RateBased,
                                dash::logic::AbstractAdaptationLogic::FixedRate,
                                dash::logic::AbstractAdaptationLogic::AlwaysLowest,
                                dash::logic::AbstractAdaptationLogic::AlwaysBest};

static const char *const ppsz_logics[] = { N_("Bandwidth Adaptive"),
                                           N_("Fixed Bandwidth"),
                                           N_("Lowest Bandwidth/Quality"),
                                           N_("Highest Bandwith/Quality")};

vlc_module_begin ()
        set_shortname( N_("DASH"))
        set_description( N_("Dynamic Adaptive Streaming over HTTP") )
        set_capability( "demux", 10 )
        set_category( CAT_INPUT )
        set_subcategory( SUBCAT_INPUT_DEMUX )
        add_integer( "dash-logic",      dash::logic::AbstractAdaptationLogic::Default,
                                             DASH_LOGIC_TEXT, NULL, false )
            change_integer_list( pi_logics, ppsz_logics )
        add_integer( "dash-prefwidth",  480, DASH_WIDTH_TEXT,  DASH_WIDTH_LONGTEXT,  true )
        add_integer( "dash-prefheight", 360, DASH_HEIGHT_TEXT, DASH_HEIGHT_LONGTEXT, true )
        add_integer( "dash-prefbw",     250, DASH_BW_TEXT,     DASH_BW_LONGTEXT,     false )
        set_callbacks( Open, Close )
vlc_module_end ()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

static int  Demux( demux_t * );
static int  Control         (demux_t *p_demux, int i_query, va_list args);

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open(vlc_object_t *p_obj)
{
    demux_t *p_demux = (demux_t*) p_obj;

    bool b_mimematched = false;
    char *psz_mime = stream_ContentType(p_demux->s);
    if(psz_mime)
    {
        b_mimematched = !strcmp(psz_mime, "application/dash+xml");
        free(psz_mime);
    }

    if(!b_mimematched && !dash::xml::DOMParser::isDash(p_demux->s))
        return VLC_EGENERIC;

    //Build a XML tree
    dash::xml::DOMParser        parser(p_demux->s);
    if( !parser.parse() )
    {
        msg_Err( p_demux, "Could not parse MPD" );
        return VLC_EGENERIC;
    }

    //Begin the actual MPD parsing:
    dash::mpd::MPD *mpd = dash::mpd::MPDFactory::create(parser.getRootNode(), p_demux->s, parser.getProfile());
    if(mpd == NULL)
    {
        msg_Err( p_demux, "Cannot create/unknown MPD for profile");
        return VLC_EGENERIC;
    }

    demux_sys_t        *p_sys = (demux_sys_t *) malloc(sizeof(demux_sys_t));
    if (unlikely(p_sys == NULL))
        return VLC_ENOMEM;

    p_sys->p_mpd = mpd;
    int logic = var_InheritInteger( p_obj, "dash-logic" );
    dash::DASHManager*p_dashManager = new dash::DASHManager(p_sys->p_mpd,
            static_cast<dash::logic::AbstractAdaptationLogic::LogicType>(logic),
            p_demux->s);

    dash::mpd::Period *period = mpd->getFirstPeriod();
    if(period && !p_dashManager->start(p_demux))
    {
        delete p_dashManager;
        free( p_sys );
        return VLC_EGENERIC;
    }
    p_sys->p_dashManager    = p_dashManager;
    p_demux->p_sys         = p_sys;
    p_demux->pf_demux      = Demux;
    p_demux->pf_control    = Control;

    msg_Dbg(p_obj,"opening mpd file (%s)", p_demux->s->psz_path);

    return VLC_SUCCESS;
}
/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close(vlc_object_t *p_obj)
{
    demux_t                            *p_demux       = (demux_t*) p_obj;
    demux_sys_t                        *p_sys          = (demux_sys_t *) p_demux->p_sys;
    dash::DASHManager                   *p_dashManager  = p_sys->p_dashManager;

    delete p_dashManager;
    free(p_sys);
}
/*****************************************************************************
 * Callbacks:
 *****************************************************************************/
static int Demux(demux_t *p_demux)
{
    demux_sys_t *p_sys = p_demux->p_sys;
    if ( p_sys->p_dashManager->read() > 0 )
    {
        if ( p_sys->p_dashManager->esCount() )
        {
            mtime_t pcr = p_sys->p_dashManager->getPCR();
            int group = p_sys->p_dashManager->getGroup();
            if(group > 0)
                es_out_Control(p_demux->out, ES_OUT_SET_GROUP_PCR, group, pcr);
            else
                es_out_Control(p_demux->out, ES_OUT_SET_PCR, pcr);
        }

        if( !p_sys->p_dashManager->updateMPD() )
            return VLC_DEMUXER_EOF;

        return VLC_DEMUXER_SUCCESS;
    }
    else
        return VLC_DEMUXER_EOF;
}

static int  Control         (demux_t *p_demux, int i_query, va_list args)
{
    demux_sys_t *p_sys = p_demux->p_sys;

    switch (i_query)
    {
        case DEMUX_CAN_SEEK:
            *(va_arg (args, bool *)) = p_sys->p_dashManager->seekAble();
            break;

        case DEMUX_CAN_CONTROL_PACE:
            *(va_arg (args, bool *)) = true;
            break;

        case DEMUX_CAN_PAUSE:
            *(va_arg (args, bool *)) = p_sys->p_mpd->isLive();
            break;

        case DEMUX_GET_TIME:
            *(va_arg (args, int64_t *)) = p_sys->p_dashManager->getPCR();
            break;

        case DEMUX_GET_LENGTH:
            *(va_arg (args, int64_t *)) = p_sys->p_dashManager->getDuration();
            break;

        case DEMUX_GET_POSITION:
            if(!p_sys->p_dashManager->getDuration())
                return VLC_EGENERIC;

            *(va_arg (args, double *)) = (double) p_sys->p_dashManager->getPCR()
                                         / p_sys->p_dashManager->getDuration();
            break;

        case DEMUX_SET_POSITION:
            if(p_sys->p_mpd->isLive() ||
               !p_sys->p_dashManager->getDuration() ||
               !p_sys->p_dashManager->setPosition( p_sys->p_dashManager->getDuration() * va_arg(args, double)))
                return VLC_EGENERIC;
            break;

        case DEMUX_SET_TIME:
            if(p_sys->p_mpd->isLive() ||
               !p_sys->p_dashManager->setPosition(va_arg(args, int64_t)))
                return VLC_EGENERIC;
            break;

        case DEMUX_GET_PTS_DELAY:
            *va_arg (args, int64_t *) = INT64_C(1000) *
                var_InheritInteger(p_demux, "network-caching");
             break;

        case DEMUX_GET_META:
        {
            if(!p_sys->p_mpd->programInfo.Get())
                break;

            vlc_meta_t *p_meta = (vlc_meta_t *) va_arg (args, vlc_meta_t*);
            vlc_meta_t *meta = vlc_meta_New();
            if (meta == NULL)
                return VLC_EGENERIC;

            if(!p_sys->p_mpd->programInfo.Get()->getTitle().empty())
                vlc_meta_SetTitle(meta, p_sys->p_mpd->programInfo.Get()->getTitle().c_str());

            if(!p_sys->p_mpd->programInfo.Get()->getSource().empty())
                vlc_meta_SetPublisher(meta, p_sys->p_mpd->programInfo.Get()->getSource().c_str());

            if(!p_sys->p_mpd->programInfo.Get()->getCopyright().empty())
                vlc_meta_SetCopyright(meta, p_sys->p_mpd->programInfo.Get()->getCopyright().c_str());

            if(!p_sys->p_mpd->programInfo.Get()->getMoreInformationUrl().empty())
                vlc_meta_SetURL(meta, p_sys->p_mpd->programInfo.Get()->getMoreInformationUrl().c_str());

            vlc_meta_Merge(p_meta, meta);
            vlc_meta_Delete(meta);
            break;
        }

        default:
            return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}
