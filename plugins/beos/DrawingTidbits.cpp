/*****************************************************************************
 * DrawingTidbits.cpp
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: DrawingTidbits.cpp,v 1.2.4.1 2002/09/03 12:00:24 tcastley Exp $
 *
 * Authors: Tony Castley <tcastley@mail.powerup.com.au>
 *          Stephan Aßmus <stippi@yellowbites.com>
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

#include <Bitmap.h>
#include <Debug.h>
#include <Screen.h>

#include "DrawingTidbits.h"

// ShiftComponent
inline uchar
ShiftComponent(uchar component, float percent)
{
	// change the color by <percent>, make sure we aren't rounding
	// off significant bits
	if (percent >= 1)
		return (uchar)(component * (2 - percent));
	else
		return (uchar)(255 - percent * (255 - component));
}

// ShiftColor
rgb_color
ShiftColor(rgb_color color, float percent)
{
	rgb_color result = {
		ShiftComponent(color.red, percent),
		ShiftComponent(color.green, percent),
		ShiftComponent(color.blue, percent),
		0
	};
	
	return result;
}

// CompareColors
static bool
CompareColors(const rgb_color a, const rgb_color b)
{
	return a.red == b.red
		&& a.green == b.green
		&& a.blue == b.blue
		&& a.alpha == b.alpha;
}

// ==
bool
operator==(const rgb_color &a, const rgb_color &b)
{
	return CompareColors(a, b);
}

// !=
bool
operator!=(const rgb_color &a, const rgb_color &b)
{
	return !CompareColors(a, b);
}

// ReplaceColor
void
ReplaceColor(BBitmap *bitmap, rgb_color from, rgb_color to)
{
	ASSERT(bitmap->ColorSpace() == B_COLOR_8_BIT); // other color spaces not implemented yet
	
	BScreen screen(B_MAIN_SCREEN_ID);
	uint32 fromIndex = screen.IndexForColor(from);
	uint32 toIndex = screen.IndexForColor(to); 
	
	uchar *bits = (uchar *)bitmap->Bits();
	int32 bitsLength = bitmap->BitsLength();	
	for (int32 index = 0; index < bitsLength; index++) 
		if (bits[index] == fromIndex)
			bits[index] = toIndex;
}

// ReplaceTransparentColor
void 
ReplaceTransparentColor(BBitmap *bitmap, rgb_color with)
{
	ASSERT(bitmap->ColorSpace() == B_COLOR_8_BIT); // other color spaces not implemented yet
	
	BScreen screen(B_MAIN_SCREEN_ID);
	uint32 withIndex = screen.IndexForColor(with); 
	
	uchar *bits = (uchar *)bitmap->Bits();
	int32 bitsLength = bitmap->BitsLength();	
	for (int32 index = 0; index < bitsLength; index++) 
		if (bits[index] == B_TRANSPARENT_8_BIT)
			bits[index] = withIndex;
}


// convert_bitmap
status_t
convert_bitmap(BBitmap* inBitmap, BBitmap* outBitmap)
{
	status_t status = B_BAD_VALUE;
/*	// see that we got valid bitmaps
	if (inBitmap && inBitmap->IsValid()
		&& outBitmap && outBitmap->IsValid())
	{
		status = B_MISMATCHED_VALUES;
		// see that bitmaps are compatible and that we support the conversion
		if (inBitmap->Bounds().Width() == outBitmap->Bounds().Width()
			&& inBitmap->Bounds().Height() == outBitmap->Bounds().Height()
			&& (outBitmap->ColorSpace() == B_RGB32
				|| outBitmap->ColorSpace() == B_RGBA32))
		{
			int32 width = inBitmap->Bounds().IntegerWidth() + 1;
			int32 height = inBitmap->Bounds().IntegerHeight() + 1;
			int32 srcBpr = inBitmap->BytesPerRow();
			int32 dstBpr = outBitmap->BytesPerRow();
			uint8* srcbits = (uint8*)inbitmap->bits();
			uint8* dstbits = (uint8*)outbitmap->bits();
			switch (inBitmap->ColorSpace())
			{
				case B_YCbCr422:
					for (int32 y = 0; y < height; y ++)
					{
						for (int32 x = 0; x < width; x += 2)
						{
							uint8 y = 
							uint8 cb = 
							uint8 cr = 
						}
						srcbits += srcBpr;
						dstbits += dstBpr;
					}
					status = B_OK;
					break;
				case B_YCbCr420:
					status = B_OK;
					break;
				case B_YUV422:
					status = B_OK;
					break;
				case B_RGB32:
					memcpy(dstBits, srcBits, inBitmap->BitsLength());
					status = B_OK;
					break;
				default:
					status = B_MISMATCHED_VALUES;
					break;
			}
		}
	}*/
	return status;
}

// clip_float
inline uint8
clip_float(float value)
{
	if (value < 0)
		value = 0;
	if (value > 255)
		value = 255;
	return (uint8)value;
}

// dim_bitmap
status_t
dim_bitmap(BBitmap* bitmap, rgb_color center, float dimLevel)
{
	status_t status = B_BAD_VALUE;
	if (bitmap && bitmap->IsValid())
	{
		switch (bitmap->ColorSpace())
		{
			case B_CMAP8:
			{
				BScreen screen(B_MAIN_SCREEN_ID);
				if (screen.IsValid())
				{
					// iterate over each pixel, get the respective
					// color from the screen object, find the distance
					// to the "center" color and shorten the distance
					// by "dimLevel"
					int32 length = bitmap->BitsLength();
					uint8* bits = (uint8*)bitmap->Bits();
					for (int32 i = 0; i < length; i++)
					{
						// preserve transparent pixels
						if (bits[i] != B_TRANSPARENT_MAGIC_CMAP8)
						{
							// get color for this index
							rgb_color c = screen.ColorForIndex(bits[i]);
							// red
							float dist = (c.red - center.red) * dimLevel;
							c.red = clip_float(center.red + dist);
							// green
							dist = (c.green - center.green) * dimLevel;
							c.green = clip_float(center.green + dist);
							// blue
							dist = (c.blue - center.blue) * dimLevel;
							c.blue = clip_float(center.blue + dist);
							// write correct index of the dimmed color
							// back into bitmap (and hope the match is close...)
							bits[i] = screen.IndexForColor(c);
						}
					}
					status = B_OK;
				}
				break;
			}
			case B_RGB32:
			case B_RGBA32:
			{
				// iterate over each color component, find the distance
				// to the "center" color and shorten the distance
				// by "dimLevel"
				uint8* bits = (uint8*)bitmap->Bits();
				int32 bpr = bitmap->BytesPerRow();
				int32 pixels = bitmap->Bounds().IntegerWidth() + 1;
				int32 lines = bitmap->Bounds().IntegerHeight() + 1;
				// iterate over color components
				for (int32 y = 0; y < lines; y++) {
					for (int32 x = 0; x < pixels; x++) {
						int32 offset = 4 * x; // four bytes per pixel
						// blue
						float dist = (bits[offset + 0] - center.blue) * dimLevel;
						bits[offset + 0] = clip_float(center.blue + dist);
						// green
						dist = (bits[offset + 1] - center.green) * dimLevel;
						bits[offset + 1] = clip_float(center.green + dist);
						// red
						dist = (bits[offset + 2] - center.red) * dimLevel;
						bits[offset + 2] = clip_float(center.red + dist);
						// ignore alpha channel
					}
					// next line
					bits += bpr;
				}
				status = B_OK;
				break;
			}
			default:
				status = B_ERROR;
				break;
		}
	}
	return status;
}

// dimmed_color_cmap8
rgb_color
dimmed_color_cmap8(rgb_color color, rgb_color center, float dimLevel)
{
	BScreen screen(B_MAIN_SCREEN_ID);
	if (screen.IsValid())
	{
		// red
		float dist = (color.red - center.red) * dimLevel;
		color.red = clip_float(center.red + dist);
		// green
		dist = (color.green - center.green) * dimLevel;
		color.green = clip_float(center.green + dist);
		// blue
		dist = (color.blue - center.blue) * dimLevel;
		color.blue = clip_float(center.blue + dist);
		// get color index for dimmed color
		int32 index = screen.IndexForColor(color);
		// put color at index (closest match in palette
		// to dimmed result) into returned color
		color = screen.ColorForIndex(index);
	}
	return color;
}
