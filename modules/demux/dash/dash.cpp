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

#include <stdint.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_demux.h>

#include "xml/DOMParser.h"
#include "mpd/MPDFactory.h"
#include "mpd/Period.h"
#include "DASHManager.h"

using namespace adaptative::logic;
using namespace adaptative::playlist;
using namespace dash::mpd;
using namespace dash::xml;
using namespace dash;

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

static const int pi_logics[] = {AbstractAdaptationLogic::RateBased,
                                AbstractAdaptationLogic::FixedRate,
                                AbstractAdaptationLogic::AlwaysLowest,
                                AbstractAdaptationLogic::AlwaysBest};

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

    if(!b_mimematched && !DOMParser::isDash(p_demux->s))
        return VLC_EGENERIC;

    //Build a XML tree
    DOMParser        parser(p_demux->s);
    if( !parser.parse() )
    {
        msg_Err( p_demux, "Could not parse MPD" );
        return VLC_EGENERIC;
    }

    //Begin the actual MPD parsing:
    MPD *mpd = MPDFactory::create(parser.getRootNode(), p_demux->s, parser.getProfile());
    if(mpd == NULL)
    {
        msg_Err( p_demux, "Cannot create/unknown MPD for profile");
        return VLC_EGENERIC;
    }

    int logic = var_InheritInteger( p_obj, "dash-logic" );
    DASHManager *p_dashManager = new (std::nothrow) DASHManager(p_demux, mpd,
            new (std::nothrow) DASHStreamOutputFactory,
            static_cast<AbstractAdaptationLogic::LogicType>(logic));

    BasePeriod *period = mpd->getFirstPeriod();
    if(period && !p_dashManager->start())
    {
        delete p_dashManager;
        return VLC_EGENERIC;
    }

    p_demux->p_sys         = reinterpret_cast<demux_sys_t *>(p_dashManager);
    p_demux->pf_demux      = p_dashManager->demux_callback;
    p_demux->pf_control    = p_dashManager->control_callback;

    msg_Dbg(p_obj,"opening mpd file (%s)", p_demux->s->psz_path);

    return VLC_SUCCESS;
}
/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close(vlc_object_t *p_obj)
{
    demux_t         *p_demux       = (demux_t*) p_obj;
    PlaylistManager *p_manager  = reinterpret_cast<PlaylistManager *>(p_demux->p_sys);

    delete p_manager;
}

