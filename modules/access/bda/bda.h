/*****************************************************************************
 * bda.h : DirectShow BDA access header for vlc
 *****************************************************************************
 * Copyright ( C ) 2007 the VideoLAN team
 *
 * Author: Ken Self <kens@campoz.fslife.co.uk>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <vlc_common.h>
#include <vlc_input.h>
#include <vlc_access.h>

#ifndef _MSC_VER
#   include <wtypes.h>
#   include <unknwn.h>
#   include <ole2.h>
#   include <limits.h>
#   ifdef _WINGDI_
#      undef _WINGDI_
#   endif
#   define _WINGDI_ 1
#   define AM_NOVTABLE
#   define _OBJBASE_H_
#   undef _X86_
#   ifndef _I64_MAX
#     define _I64_MAX LONG_LONG_MAX
#   endif
#   define LONGLONG long long
#endif

#ifdef __cplusplus
class BDAGraph;
extern "C" {
#else
typedef struct BDAGraph BDAGraph;
#endif

void dvb_newBDAGraph( access_t* p_access );
void dvb_deleteBDAGraph( access_t* p_access );
int dvb_SubmitATSCTuneRequest( access_t* p_access );
int dvb_SubmitDVBTTuneRequest( access_t* p_access );
int dvb_SubmitDVBCTuneRequest( access_t* p_access );
int dvb_SubmitDVBSTuneRequest( access_t* p_access );
long dvb_GetBufferSize( access_t* p_access );
long dvb_ReadBuffer( access_t* p_access, long* l_buffer_len, BYTE* p_buff );

#ifdef __cplusplus
}
#endif

/****************************************************************************
 * Access descriptor declaration
 ****************************************************************************/
struct access_sys_t
{
    /* These 2 must be left at the beginning */
    vlc_mutex_t lock;
    vlc_cond_t  wait;
    BDAGraph *p_bda_module;
};
