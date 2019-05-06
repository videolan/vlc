/*****************************************************************************
 * CompatibilityFixes.h: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2011-2017 VLC authors and VideoLAN
 *
 * Authors: Felix Paul Kühne <fkuehne -at- videolan -dot- org>
 *          Marvin Scholz <epirat07 -at- gmail -dot- com>
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

#import "CompatibilityFixes.h"
#import <objc/runtime.h>

/**
 Swaps out the implementation of the method at @c selector in Class @c cls
 with the implementation of that method from the superclass.
 
 @param cls         The class which this selector belongs to
 @param selector    The selector of whom to swap the implementation
 
 @note  The @c cls must be a subclass of another class and both
        must implement the @c selector for this function to work as expected!
 */
void swapoutOverride(Class cls, SEL selector)
{
    Method subclassMeth = class_getInstanceMethod(cls, selector);
    IMP baseImp = class_getMethodImplementation([cls superclass], selector);

    if (subclassMeth && baseImp)
        method_setImplementation(subclassMeth, baseImp);
}

#ifndef MAC_OS_X_VERSION_10_14

NSString *const NSAppearanceNameDarkAqua = @"NSAppearanceNameDarkAqua";

#endif

#ifndef MAC_OS_X_VERSION_10_13

NSString *const NSCollectionViewSupplementaryElementKind = @"NSCollectionViewSupplementaryElementKind";

#endif
