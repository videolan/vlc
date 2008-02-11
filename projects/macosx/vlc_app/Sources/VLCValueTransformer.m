//
//  VLCValueTransformer.m
//  VLC
//
//  Created by Pierre d'Herbemont on 12/29/07.
//  Copyright 2007 __MyCompanyName__. All rights reserved.
//

#import "VLCValueTransformer.h"


@implementation VLCFloat10000FoldTransformer

+ (Class)transformedValueClass
{
    return [NSNumber class];
}

+ (BOOL)allowsReverseTransformation
{
    return YES;
}

- (id)transformedValue:(id)value
{
    if( !value ) return nil;
 
    if(![value respondsToSelector: @selector(floatValue)])
    {
        [NSException raise: NSInternalInconsistencyException
                    format: @"Value (%@) does not respond to -floatValue.",
        [value class]];
        return nil;
    }
 
    return [NSNumber numberWithFloat: [value floatValue]*10000.];
}

- (id)reverseTransformedValue:(id)value
{
    if( !value ) return nil;
 
    if(![value respondsToSelector: @selector(floatValue)])
    {
        [NSException raise: NSInternalInconsistencyException
                    format: @"Value (%@) does not respond to -floatValue.",
        [value class]];
        return nil;
    }
 
    return [NSNumber numberWithFloat: [value floatValue]/10000.];
}
@end

@implementation VLCNonNilAsBoolTransformer

+ (Class)transformedValueClass
{
    return [NSObject class];
}

+ (BOOL)allowsReverseTransformation
{
    return NO;
}

- (NSNumber *)transformedValue:(id)value
{
    return [NSNumber numberWithBool: !!value];
}

@end

@implementation VLCURLToRepresentedFileNameTransformer

+ (Class)transformedValueClass
{
    return [NSURL class];
}

+ (BOOL)allowsReverseTransformation
{
    return NO;
}

- (NSString *)transformedValue:(id)value
{
    if( ![value isKindOfClass:[NSURL class]] || ![value isFileURL] )
        return @"";

    return [value path];
}

@end

@implementation VLCSelectionIndexToDescriptionTransformer

+ (Class)transformedValueClass
{
    return [NSNumber class];
}

+ (BOOL)allowsReverseTransformation
{
    return NO;
}

- (NSString *)transformedValue:(id)value
{
    if( ![value isKindOfClass:[NSNumber class]])
        return @"";

    return [value intValue] == NSNotFound ? @"" : [NSString stringWithFormat:@"%@ of ", value];
}

@end

