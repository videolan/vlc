/*****************************************************************************
 * DrawingTidbits.cpp
 *****************************************************************************
 * Copyright (C) 2001 the VideoLAN team
 * $Id$
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include <math.h>

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

// ycrcb_to_rgb
inline void
ycbcr_to_rgb( uint8 y, uint8 cb, uint8 cr,
			  uint8& r, uint8& g, uint8& b)
{
	r = (uint8)max_c( 0, min_c( 255, 1.164 * ( y - 16 ) + 1.596 * ( cr - 128 ) ) );
	g = (uint8)max_c( 0, min_c( 255, 1.164 * ( y - 16 ) - 0.813 * ( cr - 128 )
								- 0.391 * ( cb - 128 ) ) );
	b = (uint8)max_c( 0, min_c( 255, 1.164 * ( y - 16 ) + 2.018 * ( cb - 128 ) ) );
}

// this function will not produce visually pleasing results!
// we'd have to convert to Lab colorspace, do the mixing
// and convert back to RGB - in an ideal world...
//
// mix_colors
inline void
mix_colors( uint8 ra, uint8 ga, uint8 ba,
			uint8 rb, uint8 gb, uint8 bb,
			uint8& r, uint8& g, uint8& b, float mixLevel )
{
	float mixA = ( 1.0 - mixLevel );
	float mixB = mixLevel;
	r = (uint8)(mixA * ra + mixB * rb);
	g = (uint8)(mixA * ga + mixB * gb);
	b = (uint8)(mixA * ba + mixB * bb);
}

// the algorithm used is probably pretty slow, but it should be easy
// to understand what's going on...
//
// scale_bitmap
status_t
scale_bitmap( BBitmap* bitmap, uint32 fromWidth, uint32 fromHeight )
{
	status_t status = B_BAD_VALUE;
	
	if ( bitmap && bitmap->IsValid()
		 && ( bitmap->ColorSpace() == B_RGB32 || bitmap->ColorSpace() == B_RGBA32 ) )
	{
		status = B_MISMATCHED_VALUES;
		// we only support upscaling as of now
		uint32 destWidth = bitmap->Bounds().IntegerWidth() + 1;
		uint32 destHeight = bitmap->Bounds().IntegerHeight() + 1;
		if ( fromWidth <= destWidth && fromHeight <= destHeight )
		{
			status = B_OK;
			uint32 bpr = bitmap->BytesPerRow();
			if ( fromWidth < destWidth )
			{
				// scale horizontally
				uint8* src = (uint8*)bitmap->Bits();
				uint8* p = new uint8[fromWidth * 4];	// temp buffer
				for ( uint32 y = 0; y < fromHeight; y++ )
				{
					// copy valid pixels into temp buffer
					memcpy( p, src, fromWidth * 4 );
					for ( uint32 x = 0; x < destWidth; x++ )
					{
						// mix colors of left and right pixels and write it back
						// into the bitmap
						float xPos = ( (float)x / (float)destWidth ) * (float)fromWidth;
						uint32 leftIndex = (uint32)floorf( xPos ) * 4;
						uint32 rightIndex = (uint32)ceilf( xPos ) * 4;
						rgb_color left;
						left.red = p[leftIndex + 2];
						left.green = p[leftIndex + 1];
						left.blue = p[leftIndex + 0];
						rgb_color right;
						right.red = p[rightIndex + 2];
						right.green = p[rightIndex + 1];
						right.blue = p[rightIndex + 0];
						rgb_color mix;
						mix_colors( left.red, left.green, left.blue,
									right.red, right.green, right.blue,
									mix.red, mix.green, mix.blue, xPos - floorf( xPos ) );
						uint32 destIndex = x * 4;
						src[destIndex + 2] = mix.red;
						src[destIndex + 1] = mix.green;
						src[destIndex + 0] = mix.blue;
					}
					src += bpr;
				}
				delete[] p;
			}
			if ( fromHeight < destHeight )
			{
				// scale vertically
				uint8* src = (uint8*)bitmap->Bits();
				uint8* p = new uint8[fromHeight * 3];	// temp buffer
				for ( uint32 x = 0; x < destWidth; x++ )
				{
					// copy valid pixels into temp buffer
					for ( uint32 y = 0; y < fromHeight; y++ )
					{
						uint32 destIndex = y * 3;
						uint32 srcIndex = x * 4 + y * bpr;
						p[destIndex + 0] = src[srcIndex + 0];
						p[destIndex + 1] = src[srcIndex + 1];
						p[destIndex + 2] = src[srcIndex + 2];
					}
					// do the scaling
					for ( uint32 y = 0; y < destHeight; y++ )
					{
						// mix colors of upper and lower pixels and write it back
						// into the bitmap
						float yPos = ( (float)y / (float)destHeight ) * (float)fromHeight;
						uint32 upperIndex = (uint32)floorf( yPos ) * 3;
						uint32 lowerIndex = (uint32)ceilf( yPos ) * 3;
						rgb_color upper;
						upper.red = p[upperIndex + 2];
						upper.green = p[upperIndex + 1];
						upper.blue = p[upperIndex + 0];
						rgb_color lower;
						lower.red = p[lowerIndex + 2];
						lower.green = p[lowerIndex + 1];
						lower.blue = p[lowerIndex + 0];
						rgb_color mix;
						mix_colors( upper.red, upper.green, upper.blue,
									lower.red, lower.green, lower.blue,
									mix.red, mix.green, mix.blue, yPos - floorf( yPos ) );
						uint32 destIndex = x * 4 + y * bpr;
						src[destIndex + 2] = mix.red;
						src[destIndex + 1] = mix.green;
						src[destIndex + 0] = mix.blue;
					}
				}
				delete[] p;
			}
		}
	}
	return status;
}

// convert_bitmap
status_t
convert_bitmap( BBitmap* inBitmap, BBitmap* outBitmap )
{
	status_t status = B_BAD_VALUE;
	// see that we got valid bitmaps
	if ( inBitmap && inBitmap->IsValid()
		 && outBitmap && outBitmap->IsValid() )
	{
		status = B_MISMATCHED_VALUES;
		// see that bitmaps are compatible and that we support the conversion
		if ( inBitmap->Bounds().Width() <= outBitmap->Bounds().Width()
			 && inBitmap->Bounds().Height() <= outBitmap->Bounds().Height()
			 && ( outBitmap->ColorSpace() == B_RGB32
				  || outBitmap->ColorSpace() == B_RGBA32) )
		{
			int32 width = inBitmap->Bounds().IntegerWidth() + 1;
			int32 height = inBitmap->Bounds().IntegerHeight() + 1;
			int32 srcBpr = inBitmap->BytesPerRow();
			int32 dstBpr = outBitmap->BytesPerRow();
			uint8* srcBits = (uint8*)inBitmap->Bits();
			uint8* dstBits = (uint8*)outBitmap->Bits();
			switch (inBitmap->ColorSpace())
			{
				case B_YCbCr422:
					// Y0[7:0]  Cb0[7:0]  Y1[7:0]  Cr0[7:0]
					// Y2[7:0]  Cb2[7:0]  Y3[7:0]  Cr2[7:0]
					for ( int32 y = 0; y < height; y++ )
					{
						for ( int32 x = 0; x < width; x += 2 )
						{
							int32 srcOffset = x * 2;
							int32 dstOffset = x * 4;
							ycbcr_to_rgb( srcBits[srcOffset + 0],
										  srcBits[srcOffset + 1],
										  srcBits[srcOffset + 3],
										  dstBits[dstOffset + 2],
										  dstBits[dstOffset + 1],
										  dstBits[dstOffset + 0] );
							ycbcr_to_rgb( srcBits[srcOffset + 2],
										  srcBits[srcOffset + 1],
										  srcBits[srcOffset + 3],
										  dstBits[dstOffset + 6],
										  dstBits[dstOffset + 5],
										  dstBits[dstOffset + 4] );
							// take care of alpha
							dstBits[x * 4 + 3] = 255;
							dstBits[x * 4 + 7] = 255;
						}
						srcBits += srcBpr;
						dstBits += dstBpr;
					}
					status = B_OK;
					break;
				case B_YCbCr420:
					// Non-interlaced only!
					// Cb0  Y0  Y1  Cb2 Y2  Y3  on even scan lines ...
					// Cr0  Y0  Y1  Cr2 Y2  Y3  on odd scan lines
					status = B_ERROR;
					break;
				case B_YUV422:
					// U0[7:0]  Y0[7:0]   V0[7:0]  Y1[7:0] 
					// U2[7:0]  Y2[7:0]   V2[7:0]  Y3[7:0]
					status = B_ERROR;
					break;
				case B_RGB32:
				case B_RGBA32:
					memcpy( dstBits, srcBits, inBitmap->BitsLength() );
					status = B_OK;
					break;
				case B_RGB16:
					// G[2:0],B[4:0]  R[4:0],G[5:3]
					for ( int32 y = 0; y < height; y ++ )
					{
						for ( int32 x = 0; x < width; x++ )
						{
							int32 srcOffset = x * 2;
							int32 dstOffset = x * 4;
							uint8 blue = srcBits[srcOffset + 0] & 0x1f;
							uint8 green = ( srcBits[srcOffset + 0] >> 5 )
										  | ( ( srcBits[srcOffset + 1] & 0x07 ) << 3 );
							uint8 red = srcBits[srcOffset + 1] & 0xf8;
							// homogeneously scale each component to 8 bit
							dstBits[dstOffset + 0] = (blue << 3) | (blue >> 2);
							dstBits[dstOffset + 1] = (green << 2) | (green >> 4);
							dstBits[dstOffset + 2] = red | (red >> 5);
						}
						srcBits += srcBpr;
						dstBits += dstBpr;
					}
					status = B_OK;
					break;
				default:
					status = B_MISMATCHED_VALUES;
					break;
			}
			if ( status == B_OK )
			{
				if ( width < outBitmap->Bounds().IntegerWidth() + 1
					 || height < outBitmap->Bounds().IntegerHeight() + 1 )
				{
					scale_bitmap( outBitmap, width, height );
				}
			}
		}
	}
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
