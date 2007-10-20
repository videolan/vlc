/*****************************************************************************
* VLCLibVLCbridging.h: VLC.framework VLCLibVLCBridging header
*****************************************************************************
* Copyright (C) 2007 Pierre d'Herbemont
* Copyright (C) 2007 the VideoLAN team
* $Id: VLCEventManager.h 21564 2007-08-29 21:09:27Z pdherbemont $
*
* Authors: Pierre d'Herbemont <pdherbemont # videolan.org>
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

// TODO: Documentation
@interface VLCMediaList (LibVLCBridging)

+ (id)medialistWithLibVLCMediaList:(void *)p_new_mlist;
- (id)initWithLibVLCMediaList:(void *)p_new_mlist;
- (void *)libVLCMediaList;

@end

/**
* Bridges functionality between libvlc and VLCMedia implementation.
 */
@interface VLCMedia (LibVLCBridging)

/* Object Factory */
/**
 * Manufactures new object wrapped around specified media descriptor.
 * \param md LibVLC media descriptor pointer.
 * \return Newly created media instance using specified descriptor.
 */
+ (id)mediaWithLibVLCMediaDescriptor:(void *)md;

/**
 * Initializes new object wrapped around specified media descriptor.
 * \param md LibVLC media descriptor pointer.
 * \return Newly created media instance using specified descriptor.
 */
- (id)initWithLibVLCMediaDescriptor:(void *)md;

/**
 * Returns the receiver's internal media descriptor pointer.
 * \return The receiver's internal media descriptor pointer.
 */
- (void *)libVLCMediaDescriptor;

@end

// TODO: Documentation
@interface VLCMedia (VLCMediaPlayerBridging)

- (void)setLength:(VLCTime *)value;

@end

// TODO: Documentation
@interface VLCLibrary (VLCAudioBridging)

- (void)setAudio:(VLCAudio *)value;

@end

