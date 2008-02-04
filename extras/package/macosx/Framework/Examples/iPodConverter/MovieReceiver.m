//
//  MovieReceiver.m
//  iPodConverter
//
//  Created by Pierre d'Herbemont on 1/12/08.
//  Copyright 2008 __MyCompanyName__. All rights reserved.
//

#import "MovieReceiver.h"

/**********************************************************
 * This handles drag-and-drop in the main window
 */

@implementation MovieReceiver
- (void)awakeFromNib
{
    [self registerForDraggedTypes:[NSArray arrayWithObjects:NSFilenamesPboardType, NSURLPboardType, nil]];

}
- (NSDragOperation)draggingEntered:(id <NSDraggingInfo>)sender
{
    return NSDragOperationGeneric;
}
- (NSDragOperation)performDragOperation:(id <NSDraggingInfo>)sender
{
    NSPasteboard *pboard = [sender draggingPasteboard];

    if ( [[pboard types] containsObject:NSFilenamesPboardType] )
    {
        NSArray *files = [pboard propertyListForType:NSFilenamesPboardType];
        for( NSString * filename in files )
        {
            [controller setMedia:[VLCMedia mediaWithPath:filename]];
        }
    }
    return YES;

    return NSDragOperationGeneric;
}
@end
