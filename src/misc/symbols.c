/*****************************************************************************
 * symbols.c : Extra file used to force linking with some shared symbols
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: symbols.c,v 1.2 2002/06/01 12:32:01 sam Exp $
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
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
 * Rationale for this file:
 *
 * On certain architectures, such as IA64 or HPPA, it is forbidden to link
 * static objects with objects which have relocation information. This
 * basically means that if you are building libfoo.so, you cannot add libbar.a
 * to the link process. To bypass this restriction, we link the main app with
 * libbar.a, but then we need to tell the compiler that we will need symbols
 * from libbar.a, this is why this file is here.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <vlc/vlc.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/extensions/XShm.h>
#include <X11/extensions/Xv.h>
#include <X11/extensions/Xvlib.h>
#include <X11/extensions/dpms.h>
#include <X11/extensions/xf86dga.h>
#include <X11/extensions/xf86dgastr.h>
#include <X11/extensions/xf86vmode.h>
#include <X11/extensions/xf86vmstr.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
void _Use   ( int, ... );
void _Beurk ( void );

/*****************************************************************************
 * 
 *****************************************************************************/
void _Use( int i_dummy, ... )
{
   ;
}

/*****************************************************************************
 * 
 *****************************************************************************/
void _Beurk( void )
{   
    /* for i in Xxf86dga Xxf86vm Xv
     *   do nm /usr/X11R6/lib/lib$i.a | grep ' T ' | awk '{ print $3 }'
     * done
     */
    _Use( 0, XF86DGADirectVideo, XF86DGADirectVideoLL, XF86DGAForkApp,
             XF86DGAGetVidPage, XF86DGAGetVideo, XF86DGAGetVideoLL,
             XF86DGAGetViewPortSize, XF86DGAInstallColormap,
             XF86DGAQueryDirectVideo, XF86DGAQueryExtension,
             XF86DGAQueryVersion, XF86DGASetVidPage, XF86DGASetViewPort,
             XF86DGAViewPortChanged );

    _Use( 0, XDGAChangePixmapMode, XDGACloseFramebuffer, XDGACopyArea,
             XDGACopyTransparentArea, XDGACreateColormap, XDGAFillRectangle,
             /* XDGAGetMappedMemory, */ XDGAGetViewportStatus,
             XDGAInstallColormap, XDGAKeyEventToXKeyEvent,
             /* XDGAMapFramebuffer, */ XDGAOpenFramebuffer, XDGAQueryExtension,
             XDGAQueryModes, XDGAQueryVersion, XDGASelectInput,
             XDGASetClientVersion, XDGASetMode, XDGASetViewport, XDGASync
             /* XDGAUnmapFramebuffer, */ /* xdga_find_display */ );

    _Use( 0, XF86VidModeAddModeLine, XF86VidModeDeleteModeLine,
             XF86VidModeGetAllModeLines, XF86VidModeGetDotClocks,
             XF86VidModeGetGamma, XF86VidModeGetGammaRamp,
             XF86VidModeGetGammaRampSize, XF86VidModeGetModeLine,
             XF86VidModeGetMonitor, XF86VidModeGetViewPort,
             XF86VidModeLockModeSwitch, XF86VidModeModModeLine,
             XF86VidModeQueryExtension, XF86VidModeQueryVersion,
             XF86VidModeSetClientVersion, XF86VidModeSetGamma,
             XF86VidModeSetGammaRamp, XF86VidModeSetViewPort,
             XF86VidModeSwitchMode, XF86VidModeSwitchToMode,
             XF86VidModeValidateModeLine );

    _Use( 0, XvCreateImage, XvFreeAdaptorInfo, XvFreeEncodingInfo,
             XvGetPortAttribute, XvGetStill, XvGetVideo, XvGrabPort,
             XvListImageFormats, XvPutImage, XvPutStill, XvPutVideo,
             XvQueryAdaptors, XvQueryBestSize, XvQueryEncodings,
             XvQueryExtension, XvQueryPortAttributes, XvSelectPortNotify,
             XvSelectVideoNotify, XvSetPortAttribute, XvShmCreateImage,
             XvShmPutImage, XvStopVideo, XvUngrabPort );
}

