/*****************************************************************************
 * VLCMediaArrayController.m: NSArrayController subclass specific to media
 * list.
 *****************************************************************************
 * Copyright (C) 2007 Pierre d'Herbemont
 * Copyright (C) 2007 the VideoLAN team
 * $Id$
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

#import "VLCMediaArrayController.h"


@implementation VLCMediaArrayController
@synthesize contentMediaList;
@end

/******************************************************************************
 * VLCMediaArrayController (NSTableViewDataSource)
 */
@implementation VLCMediaArrayController (NSTableViewDataSource)

/* Dummy implementation, because that seems to be needed */
- (int)numberOfRowsInTableView:(NSTableView *)tableView
{
    return 0;
}

- (id)tableView:(NSTableView *)tableView objectValueForTableColumn:(NSTableColumn *)tableColumn
			row:(int)row
{
    return nil;
}

/* Implement drag and drop */
- (NSDragOperation)tableView:(NSTableView*)tv validateDrop:(id <NSDraggingInfo>)info 
				 proposedRow:(int)row proposedDropOperation:(NSTableViewDropOperation)op
{
    return [contentMediaList isReadOnly] || op == NSTableViewDropOn ? NSDragOperationNone : NSDragOperationGeneric;
}

- (BOOL)tableView:(NSTableView *)aTableView acceptDrop:(id <NSDraggingInfo>)info
			  row:(int)row dropOperation:(NSTableViewDropOperation)operation
{
    int i;
    row = 0;
    NSArray *droppedItems = [[info draggingPasteboard] propertyListForType:NSFilenamesPboardType];
    if( !droppedItems )
        droppedItems = [[info draggingPasteboard] propertyListForType:NSURLPboardType];
    for (i = 0; i < [droppedItems count]; i++)
    {
        NSString * filename = [droppedItems objectAtIndex:i];
		VLCMedia *media = [VLCMedia mediaWithPath:filename];
        [contentMediaList lock];
		[contentMediaList insertMedia:media atIndex:row];
        [contentMediaList unlock];
    }
    return YES;
}

- (BOOL)tableView:(NSTableView *)aTableView writeRowsWithIndexes:(NSIndexSet *)rowIndexes toPasteboard:(NSPasteboard *)pboard
{
    NSMutableArray *array = [NSMutableArray arrayWithCapacity:[rowIndexes count]];
    int i = [rowIndexes firstIndex];
    do {
        [array addObject:[[contentMediaList mediaAtIndex:i] url]];
    } while ((i = [rowIndexes indexGreaterThanIndex:i]) != NSNotFound);

    [pboard declareTypes:[NSArray arrayWithObject:@"VLCMediaURLType"] owner:self];
    [pboard setPropertyList:array forType:@"VLCMediaURLType"];
    return YES;
}
@end
