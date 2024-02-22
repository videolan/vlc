/*****************************************************************************
 * vlc_config_cat.h : Definition of configuration categories
 *****************************************************************************
 * Copyright (C) 2003 VLC authors and VideoLAN
 * Copyright (C) 2023 Videolabs
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Anil Daoud <anil@videolan.org>
 *          Alexandre Janniaux <ajanni@videolabs.io>
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <vlc_common.h>
#include <vlc_config_cat.h>

#include <assert.h>

static const struct config_category_t categories_array[] =
{
    { CAT_PLAYLIST,   SUBCAT_PLAYLIST_GENERAL,   PLAYLIST_HELP  },
    { CAT_INTERFACE,  SUBCAT_INTERFACE_GENERAL,  INTF_HELP      },
    { CAT_AUDIO,      SUBCAT_AUDIO_GENERAL,      AUDIO_HELP     },
    { CAT_VIDEO,      SUBCAT_VIDEO_GENERAL,      VIDEO_HELP     },
    { CAT_INPUT,      SUBCAT_INPUT_GENERAL,      INPUT_HELP     },
    { CAT_SOUT,       SUBCAT_SOUT_GENERAL,       SOUT_HELP      },
    { CAT_ADVANCED,   SUBCAT_ADVANCED_MISC,      AADVANCED_HELP },
};

static const struct config_subcategory_t subcategories_array[] =
{
    { SUBCAT_PLAYLIST_GENERAL,     CAT_PLAYLIST,   PLAYLIST_TITLE,            PGENERAL_HELP      },
    { SUBCAT_PLAYLIST_EXPORT,      CAT_PLAYLIST,   N_("Export"),              PEXPORT_HELP       },
    { SUBCAT_PLAYLIST_SD,          CAT_PLAYLIST,   N_("Services discovery"),  SD_HELP            },

    { SUBCAT_INTERFACE_GENERAL,    CAT_INTERFACE,  INTF_TITLE,                INTF_GENERAL_HELP  },
    { SUBCAT_INTERFACE_CONTROL,    CAT_INTERFACE,  N_("Control interfaces"),  INTF_CONTROL_HELP  },
    { SUBCAT_INTERFACE_HOTKEYS,    CAT_INTERFACE,  N_("Hotkeys settings"),    INTF_HOTKEYS_HELP  },
    { SUBCAT_INTERFACE_MAIN,       CAT_INTERFACE,  N_("Main interfaces"),     INTF_MAIN_HELP     },

    { SUBCAT_AUDIO_GENERAL,        CAT_AUDIO,      AUDIO_TITLE,               AUDIO_GENERAL_HELP },
    { SUBCAT_AUDIO_RESAMPLER,      CAT_AUDIO,      N_("Audio resampler"),     AFILTER_HELP       },
    { SUBCAT_AUDIO_AFILTER,        CAT_AUDIO,      N_("Filters"),             AFILTER_HELP       },
    { SUBCAT_AUDIO_AOUT,           CAT_AUDIO,      N_("Output modules"),      AOUT_HELP          },
    { SUBCAT_AUDIO_VISUAL,         CAT_AUDIO,      N_("Visualizations"),      AVISUAL_HELP       },

    { SUBCAT_VIDEO_GENERAL,        CAT_VIDEO,      VIDEO_TITLE,               VIDEO_GENERAL_HELP },
    { SUBCAT_VIDEO_VFILTER,        CAT_VIDEO,      N_("Filters"),             VFILTER_HELP       },
    { SUBCAT_VIDEO_VOUT,           CAT_VIDEO,      N_("Output modules"),      VOUT_HELP          },
    { SUBCAT_VIDEO_SPLITTER,       CAT_VIDEO,      N_("Splitters"),           SPLITTER_HELP      },
    { SUBCAT_VIDEO_SUBPIC,         CAT_VIDEO,      N_("Subtitles / OSD"),     SUBPIC_HELP        },

    { SUBCAT_INPUT_GENERAL,        CAT_INPUT,      INPUT_TITLE,               INPUT_HELP         },
    { SUBCAT_INPUT_ACCESS,         CAT_INPUT,      N_("Access modules"),      ACCESS_HELP        },
    { SUBCAT_INPUT_ACODEC,         CAT_INPUT,      N_("Audio codecs"),        ADEC_HELP          },
    { SUBCAT_INPUT_DEMUX,          CAT_INPUT,      N_("Demuxers"),            DEMUX_HELP         },
    { SUBCAT_INPUT_STREAM_FILTER,  CAT_INPUT,      N_("Stream filters"),      STREAM_FILTER_HELP },
    { SUBCAT_INPUT_SCODEC,         CAT_INPUT,      N_("Subtitle codecs"),     SDEC_HELP          },
    { SUBCAT_INPUT_VCODEC,         CAT_INPUT,      N_("Video codecs"),        VDEC_HELP          },

    { SUBCAT_SOUT_GENERAL,         CAT_SOUT,       SOUT_TITLE,                SOUT_GENERAL_HELP  },
    { SUBCAT_SOUT_ACO,             CAT_SOUT,       N_("Access output"),       SOUT_ACO_HELP      },
    { SUBCAT_SOUT_MUX,             CAT_SOUT,       N_("Muxers"),              SOUT_MUX_HELP      },
    { SUBCAT_SOUT_PACKETIZER,      CAT_SOUT,       N_("Packetizers"),         SOUT_PACKET_HELP   },
    { SUBCAT_SOUT_RENDERER,        CAT_SOUT,       N_("Renderers"),           SOUT_RENDER_HELP   },
    { SUBCAT_SOUT_STREAM,          CAT_SOUT,       N_("Sout stream"),         SOUT_STREAM_HELP   },
    { SUBCAT_SOUT_VOD,             CAT_SOUT,       N_("VOD"),                 SOUT_VOD_HELP      },

    { SUBCAT_ADVANCED_MISC,        CAT_ADVANCED,   AADVANCED_TITLE,           AADVANCED_HELP     },
    { SUBCAT_ADVANCED_NETWORK,     CAT_ADVANCED,   N_("Network"),             ANETWORK_HELP      },

    { SUBCAT_HIDDEN,               CAT_HIDDEN,     NULL,                      NULL               },
};

const struct config_category_t *
vlc_config_cat_Find(enum vlc_config_cat cat)
{
    for (size_t i=0; i < ARRAY_SIZE(categories_array); i++ )
    {
        if( categories_array[i].id == cat )
            return &categories_array[i];
    }
    return NULL;
}

const struct config_category_t *
vlc_config_cat_GetAt(size_t index)
{
    assert(index < ARRAY_SIZE(categories_array));
    return &categories_array[index];
}

const struct config_subcategory_t *
vlc_config_subcat_Find(enum vlc_config_subcat subcat)
{
    for (size_t i=0; i < ARRAY_SIZE(subcategories_array); i++)
    {
        if (subcategories_array[i].id == subcat)
            return &subcategories_array[i];
    }
    return NULL;
}

const struct config_subcategory_t *
vlc_config_subcat_GetAt(size_t index)
{
    assert(index < ARRAY_SIZE(subcategories_array));
    return &subcategories_array[index];
}

size_t
vlc_config_cat_Count(void)
{
    return ARRAY_SIZE(categories_array);
}

size_t
vlc_config_subcat_Count(void)
{
    return ARRAY_SIZE(subcategories_array);
}
