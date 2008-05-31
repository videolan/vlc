/*****************************************************************************
 * buffer.c : DirectMedia Object decoder module for vlc
 *****************************************************************************
 * Copyright (C) 2002, 2003 the VideoLAN team
 * $Id$
 *
 * Author: Gildas Bazin <gbazin@videolan.org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_codec.h>
#include <vlc_vout.h>

#ifndef WIN32
#    define LOADER
#else
#   include <objbase.h>
#endif

#ifdef LOADER
#   include <wine/winerror.h>
#   include <wine/windef.h>
#endif

#include <vlc_codecs.h>
#include "dmo.h"

static long STDCALL QueryInterface( IUnknown *This,
                                    const GUID *riid, void **ppv )
{
    CMediaBuffer *p_mb = (CMediaBuffer *)This;
    if( !memcmp( riid, &IID_IUnknown, sizeof(GUID) ) ||
        !memcmp( riid, &IID_IMediaBuffer, sizeof(GUID) ) )
    {
        p_mb->i_ref++;
        *ppv = (void *)This;
        return NOERROR;
    }
    else
    {
        *ppv = NULL;
        return E_NOINTERFACE;
    }
}

static long STDCALL AddRef( IUnknown *This )
{
    CMediaBuffer *p_mb = (CMediaBuffer *)This;
    return p_mb->i_ref++;
}

static long STDCALL Release( IUnknown *This )
{
    CMediaBuffer *p_mb = (CMediaBuffer *)This;
    p_mb->i_ref--;

    if( p_mb->i_ref == 0 )
    {
        if( p_mb->b_own ) block_Release( p_mb->p_block );
        free( p_mb->vt );
        free( p_mb );
    }

    return 0;
}

static long STDCALL SetLength( IMediaBuffer *This, uint32_t cbLength )
{
    CMediaBuffer *p_mb = (CMediaBuffer *)This;
    if( cbLength > (uint32_t)p_mb->i_max_size ) return E_INVALIDARG;
    p_mb->p_block->i_buffer = cbLength;
    return S_OK;
}

static long STDCALL GetMaxLength( IMediaBuffer *This, uint32_t *pcbMaxLength )
{
    CMediaBuffer *p_mb = (CMediaBuffer *)This;
    if( !pcbMaxLength ) return E_POINTER;
    *pcbMaxLength = p_mb->i_max_size;
    return S_OK;
}

static long STDCALL GetBufferAndLength( IMediaBuffer *This,
                                        char **ppBuffer, uint32_t *pcbLength )
{
    CMediaBuffer *p_mb = (CMediaBuffer *)This;

    if( !ppBuffer && !pcbLength ) return E_POINTER;
    if( ppBuffer ) *ppBuffer = (char*)p_mb->p_block->p_buffer;
    if( pcbLength ) *pcbLength = p_mb->p_block->i_buffer;
    return S_OK;
}

CMediaBuffer *CMediaBufferCreate( block_t *p_block, int i_max_size,
                                  bool b_own )
{
    CMediaBuffer *p_mb = (CMediaBuffer *)malloc( sizeof(CMediaBuffer) );
    if( !p_mb ) return NULL;

    p_mb->vt = (IMediaBuffer_vt *)malloc( sizeof(IMediaBuffer_vt) );
    if( !p_mb->vt )
    {
        free( p_mb );
        return NULL;
    }

    p_mb->i_ref = 1;
    p_mb->p_block = p_block;
    p_mb->i_max_size = i_max_size;
    p_mb->b_own = b_own;

    p_mb->vt->QueryInterface = QueryInterface;
    p_mb->vt->AddRef = AddRef;
    p_mb->vt->Release = Release;

    p_mb->vt->SetLength = SetLength;
    p_mb->vt->GetMaxLength = GetMaxLength;
    p_mb->vt->GetBufferAndLength = GetBufferAndLength;

    return p_mb;
}
