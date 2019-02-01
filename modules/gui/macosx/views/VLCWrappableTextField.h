/*****************************************************************************
 * VLCWrappableTextField.h
 *****************************************************************************
 * Copyright (C) 2017 VideoLAN and authors
 * Author:       David Fuhrmann <dfuhrmann at videolan dot org>
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

#import <Cocoa/Cocoa.h>

/**
 * Helper class for wrappable text multi line text fields on 10.7.
 *
 * Makes sure to try to wrap the text while calulating an intrinsic size for
 * the field.
 *
 * For this to work, make sure that:
 * - Field has a minimum width (best is to use >= constraint)
 * - Field has layout set to wrap
 * - Fields preferred with setting is explicit with constant 0 (auto or runtime
 *   width are not compatible with 10.7)
 * - If text can change, make sure to have vertical hugging priorities > 500 so
 *   that window height can shrink again if text gets smaller.
 *
 * TODO: Revisit that code one 10.7 is dropped.
 */
@interface VLCWrappableTextField : NSTextField

@end