/*****************************************************************************
 * streamdata.h: streaming/transcoding data
 *****************************************************************************
 * Copyright (C) 2000-2005 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/


#define MUXERS_NUMBER 9

// Do not count dummy here !
#define VCODECS_NUMBER 12
#define ACODECS_NUMBER 9

#define MUX_PS          0
#define MUX_TS          1
#define MUX_MPEG        2
#define MUX_OGG         3
#define MUX_RAW         4
#define MUX_ASF         5
#define MUX_AVI         6
#define MUX_MP4         7
#define MUX_MOV         8
#define MUX_WAV         9

/* Muxer / Codecs / Access_out compatibility tables */


struct codec {
    char *psz_display;
    char *psz_codec;
    char *psz_descr;
    int muxers[MUXERS_NUMBER];
};

extern const struct codec vcodecs_array[];
extern const struct codec acodecs_array[];


struct method {
    char *psz_access;
    char *psz_method;
    char *psz_descr;
    char *psz_address;
    int   muxers[MUXERS_NUMBER];
};

extern const struct method methods_array[];


struct encap {
    int   id;
    char *psz_mux;
    char *psz_encap;
    char *psz_descr;
};

extern const struct encap encaps_array[];


/* Bitrates arrays */
static const wxString vbitrates_array[] =
{
    wxT("3072"),
    wxT("2048"),
    wxT("1024"),
    wxT("768"),
    wxT("512"),
    wxT("384"),
    wxT("256"),
    wxT("192"),
    wxT("128"),
    wxT("96"),
    wxT("64"),
    wxT("32"),
    wxT("16")
};

static const wxString abitrates_array[] =
{
    wxT("512"),
    wxT("256"),
    wxT("192"),
    wxT("128"),
    wxT("96"),
    wxT("64"),
    wxT("32"),
    wxT("16")
};
