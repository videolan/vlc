/*****************************************************************************
 * encoder.c : audio/video encoder using ffmpeg library
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: encoder.c,v 1.2 2003/02/20 01:52:46 sigmunau Exp $
 *
 * Authors: Laurent Aimar
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
 * Preamble
 *****************************************************************************/
#include <vlc/vlc.h>
#include <vlc/vout.h>
#include <vlc/input.h>
#include <vlc/decoder.h>

#include <stdlib.h>

#include "codecs.h"
#include "encoder.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
int  E_( OpenEncoderVideo  )( vlc_object_t * );
void E_( CloseEncoderVideo )( vlc_object_t * );

int  E_( OpenEncoderAudio ) ( vlc_object_t * );
void E_( CloseEncoderAudio )( vlc_object_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

vlc_module_begin();
    set_description( _("ffmpeg encoder") );
    add_shortcut( "ffmpeg" );


    add_submodule();
        set_capability( "video encoder", 100 );
        set_callbacks( E_( OpenEncoderVideo ), E_( CloseEncoderVideo ) );
        add_category_hint( "video setting", NULL, VLC_TRUE );
            add_integer( "encoder-ffmpeg-video-bitrate", 1000, NULL, "bitrate (kb/s)", "bitrate (kb/s)", VLC_TRUE );
            add_integer( "encoder-ffmpeg-video-max-key-interval", 10, NULL, "max key interval", "maximum   value  of   frames  between   two  keyframes", VLC_TRUE );
            add_integer( "encoder-ffmpeg-min-quantizer", 2, NULL, "min quantizer", "range 1-31", VLC_TRUE );
            add_integer( "encoder-ffmpeg-max-quantizer", 31, NULL, "max quantizer", "range 1-31", VLC_TRUE );

    add_submodule();
        set_capability( "audio encoder", 50 );
        set_callbacks( E_( OpenEncoderAudio ), E_( CloseEncoderAudio ) );
        add_category_hint( "audio setting", NULL, VLC_TRUE );
            add_integer( "encoder-ffmpeg-audio-bitrate", 64, NULL, "bitrate (kb/s)", "bitrate (kb/s)", VLC_TRUE );

vlc_module_end();


#if 0
    add_category_hint( "general setting", NULL );
        add_integer( "encoder-xvid-bitrate", 1000, NULL, "bitrate (kb/s)", "bitrate (kb/s)" );
        add_integer( "encoder-xvid-min-quantizer", 2, NULL, "min quantizer", "range 1-31" );
        add_integer( "encoder-xvid-max-quantizer", 31, NULL, "max quantizer", "1-31" );
    add_category_hint( "advanced setting", NULL );
        add_integer( "encoder-xvid-reaction-delay-factor", -1, NULL, "rc reaction delay factor", "rate controler parameters");
        add_integer( "encoder-xvid-averaging-period", -1, NULL, "rc averaging period", "rate controler parameters" );
        add_integer( "encoder-xvid-buffer", -1, NULL, "rc buffer", "rate controler parameters" );
    add_category_hint( "advanced frame setting", NULL );
        add_string_from_list( "encoder-xvid-quantization", "MPEG", ppsz_xvid_quant_algo, NULL, "quantization algorithm", "" );
        add_bool( "encoder-xvid-halfpel", 1, NULL, "half pixel  motion estimation.", "" );
        add_bool( "encoder-xvid-4mv", 0, NULL, "fourc vector per macroblock(need halfpel)", "" );
        add_bool( "encoder-xvid-lumi-mask", 0, NULL, "use a lumimasking algorithm", "" );
        add_bool( "encoder-xvid-adaptive-quant", 0, NULL, "perform  an  adaptative quantization", "" );
        add_bool( "encoder-xvid-interlacing", 0, NULL, "use MPEG4  interlaced mode", "" );
        add_string_from_list( "encoder-xvid-me", "", ppsz_xvid_me, NULL, "motion estimation", "" );
        add_bool( "encoder-xvid-motion-advanceddiamond", 1, NULL, "motion advanceddiamond", "" );
        add_bool( "encoder-xvid-motion-halfpeldiamond", 1, NULL, "motion halfpel diamond", "" );
        add_bool( "encoder-xvid-motion-halfpelrefine", 1, NULL, "motion halfpelrefine", "" );
        add_bool( "encoder-xvid-motion-extsearch", 1, NULL, "motion extsearch", "" );
        add_bool( "encoder-xvid-motion-earlystop", 1, NULL, "motion earlystop", "" );
        add_bool( "encoder-xvid-motion-quickstop", 1, NULL, "motion quickstop", "" );
        add_bool( "encoder-xvid-motion-usesquares", 0, NULL, "use a square search", "" );
vlc_module_end();
#endif
