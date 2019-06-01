/*****************************************************************************
 * NSFont+VLCAdditions.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
 *
 * Authors: Felix Paul Kühne <fkuehne # videolan -dot- org>
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

#import "NSFont+VLCAdditions.h"

@implementation NSFont (VLCAdditions)

+ (instancetype)VLClibrarySectionHeaderFont
{
    return [NSFont systemFontOfSize:24. weight:NSFontWeightBold];
}

+ (instancetype)VLClibraryHighlightCellTitleFont
{
    return [NSFont systemFontOfSize:21. weight:NSFontWeightMedium];
}

+ (instancetype)VLClibraryHighlightCellSubtitleFont
{
    return [NSFont systemFontOfSize:13. weight:NSFontWeightSemibold];
}

+ (instancetype)VLClibraryHighlightCellHighlightLabelFont
{
    return [NSFont systemFontOfSize:11. weight:NSFontWeightBold];
}

+ (instancetype)VLClibraryLargeCellTitleFont
{
    return [NSFont systemFontOfSize:17. weight:NSFontWeightMedium];
}

+ (instancetype)VLClibraryLargeCellSubtitleFont
{
    return [NSFont systemFontOfSize:13. weight:NSFontWeightSemibold];
}

+ (instancetype)VLClibrarySmallCellTitleFont
{
    return [NSFont systemFontOfSize:13. weight:NSFontWeightMedium];
}

+ (instancetype)VLClibrarySmallCellSubtitleFont
{
    return [NSFont systemFontOfSize:10. weight:NSFontWeightSemibold];
}

+ (instancetype)VLClibraryCellAnnotationFont
{
    return [NSFont systemFontOfSize:15. weight:NSFontWeightBold];
}

+ (instancetype)VLClibraryButtonFont
{
    return [NSFont systemFontOfSize:15. weight:NSFontWeightBold];
}

+ (instancetype)VLCplaylistLabelFont
{
    return [NSFont systemFontOfSize:13. weight:NSFontWeightRegular];
}

+ (instancetype)VLCplaylistSelectedItemLabelFont
{
    return [NSFont systemFontOfSize:13. weight:NSFontWeightBold];
}

+ (instancetype)VLCsmallPlaylistLabelFont
{
    return [NSFont systemFontOfSize:10. weight:NSFontWeightRegular];
}

+ (instancetype)VLCsmallPlaylistSelectedItemLabelFont
{
    return [NSFont systemFontOfSize:10. weight:NSFontWeightBold];
}


@end
