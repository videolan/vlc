//
//  PXSourceList.m
//  PXSourceList
//
//  Created by Alex Rozanski on 05/09/2009.
//  Copyright 2009-10 Alex Rozanski http://perspx.com
//
//  GC-enabled code revised by Stefan Vogt http://byteproject.net
//

#import "PXSourceList.h"
#import "SideBarItem.h"

//Layout constants
#define MIN_BADGE_WIDTH     22.0            //The minimum badge width for each item (default 22.0)
#define BADGE_HEIGHT        14.0            //The badge height for each item (default 14.0)
#define BADGE_MARGIN        5.0             //The spacing between the badge and the cell for that row
#define ROW_RIGHT_MARGIN    5.0             //The spacing between the right edge of the badge and the edge of the table column
#define ICON_SPACING        2.0             //The spacing between the icon and it's adjacent cell
#define DISCLOSURE_TRIANGLE_SPACE   18.0    //The indentation reserved for disclosure triangles for non-group items

//Drawing constants
#define BADGE_BACKGROUND_COLOR              [NSColor colorWithCalibratedRed:(152/255.0) green:(168/255.0) blue:(202/255.0) alpha:1]
#define BADGE_HIDDEN_BACKGROUND_COLOR       [NSColor colorWithDeviceWhite:(180/255.0) alpha:1]
#define BADGE_SELECTED_TEXT_COLOR           [NSColor keyboardFocusIndicatorColor]
#define BADGE_SELECTED_UNFOCUSED_TEXT_COLOR [NSColor colorWithCalibratedRed:(153/255.0) green:(169/255.0) blue:(203/255.0) alpha:1]
#define BADGE_SELECTED_HIDDEN_TEXT_COLOR    [NSColor colorWithCalibratedWhite:(170/255.0) alpha:1]
#define BADGE_FONT                          [NSFont boldSystemFontOfSize:11]

//Delegate notification constants
NSString * const PXSLSelectionIsChangingNotification = @"PXSourceListSelectionIsChanging";
NSString * const PXSLSelectionDidChangeNotification = @"PXSourceListSelectionDidChange";
NSString * const PXSLItemWillExpandNotification = @"PXSourceListItemWillExpand";
NSString * const PXSLItemDidExpandNotification = @"PXSourceListItemDidExpand";
NSString * const PXSLItemWillCollapseNotification = @"PXSourceListItemWillCollapse";
NSString * const PXSLItemDidCollapseNotification = @"PXSourceListItemDidCollapse";
NSString * const PXSLDeleteKeyPressedOnRowsNotification = @"PXSourceListDeleteKeyPressedOnRows";

#pragma mark -
@interface PXSourceList ()

- (NSSize)sizeOfBadgeAtRow:(NSInteger)rowIndex;
- (void)drawBadgeForRow:(NSInteger)rowIndex inRect:(NSRect)badgeFrame;
- (void)registerDelegateToReceiveNotification:(NSString*)notification withSelector:(SEL)selector;

@end

#pragma mark -
@implementation PXSourceList

@synthesize iconSize = _iconSize;
@dynamic dataSource;
@dynamic delegate;

#pragma mark Init/Dealloc/Finalize

- (id)initWithCoder:(NSCoder*)decoder
{
    if(self=[super initWithCoder:decoder])
    {
        [self setDelegate:(id<PXSourceListDelegate>)[super delegate]];
        [super setDelegate:self];
        [self setDataSource:(id<PXSourceListDataSource>)[super dataSource]];
        [super setDataSource:self];

        _iconSize = NSMakeSize(16,16);
    }

    return self;
}

- (void)dealloc
{
    //Unregister the delegate from receiving notifications
    [[NSNotificationCenter defaultCenter] removeObserver:_secondaryDelegate name:nil object:self];

    [super dealloc];
}

- (void)finalize
{
    //Unregister the delegate from receiving notifications
    [[NSNotificationCenter defaultCenter] removeObserver:_secondaryDelegate name:nil object:self];

    [super finalize];
}

#pragma mark -
#pragma mark Custom Accessors

- (void)setDelegate:(id<PXSourceListDelegate>)aDelegate
{
    //Unregister the old delegate from receiving notifications
    [[NSNotificationCenter defaultCenter] removeObserver:_secondaryDelegate name:nil object:self];

    _secondaryDelegate = aDelegate;
    //Register the new delegate to receive notifications
    [self registerDelegateToReceiveNotification:PXSLSelectionIsChangingNotification withSelector:@selector(sourceListSelectionIsChanging:)];
    [self registerDelegateToReceiveNotification:PXSLSelectionDidChangeNotification withSelector:@selector(sourceListSelectionDidChange:)];
    [self registerDelegateToReceiveNotification:PXSLItemWillExpandNotification withSelector:@selector(sourceListItemWillExpand:)];
    [self registerDelegateToReceiveNotification:PXSLItemDidExpandNotification withSelector:@selector(sourceListItemDidExpand:)];
    [self registerDelegateToReceiveNotification:PXSLItemWillCollapseNotification withSelector:@selector(sourceListItemWillCollapse:)];
    [self registerDelegateToReceiveNotification:PXSLItemDidCollapseNotification withSelector:@selector(sourceListItemDidCollapse:)];
    [self registerDelegateToReceiveNotification:PXSLDeleteKeyPressedOnRowsNotification withSelector:@selector(sourceListDeleteKeyPressedOnRows:)];
}


- (void)setDataSource:(id<PXSourceListDataSource>)aDataSource
{
    _secondaryDataSource = aDataSource;

    if ([self respondsToSelector:@selector(reloadData)])
        [self reloadData];
}

- (void)setIconSize:(NSSize)newIconSize
{
    _iconSize = newIconSize;
    CGFloat rowHeight = [self rowHeight];

    //Make sure icon height does not exceed row height; if so constrain, keeping width and height in proportion
    if(_iconSize.height>rowHeight)
    {
        _iconSize.width = _iconSize.width * (rowHeight/_iconSize.height);
        _iconSize.height = rowHeight;
    }
}

#pragma mark -
#pragma mark Data Management

- (void)reloadData
{
    if ([super respondsToSelector:@selector(reloadData)])
        [super reloadData];

    //Expand items that are displayed as always expanded
    if([_secondaryDataSource conformsToProtocol:@protocol(PXSourceListDataSource)] &&
       [_secondaryDelegate respondsToSelector:@selector(sourceList:isGroupAlwaysExpanded:)])
    {
        for(NSUInteger i=0;i<[self numberOfGroups];i++)
        {
            id item = [_secondaryDataSource sourceList:self child:i ofItem:nil];

            if([self isGroupAlwaysExpanded:item]) {
                [self expandItem:item expandChildren:NO];
            }
        }
    }

    //If there are selected rows and the item hierarchy has changed, make sure a Group row isn't
    //selected
    if([self numberOfSelectedRows]>0) {
        NSIndexSet *selectedIndexes = [self selectedRowIndexes];
        NSUInteger firstSelectedRow = [selectedIndexes firstIndex];

        //Is a group item selected?
        if([self isGroupItem:[self itemAtRow:firstSelectedRow]]) {
            //Work backwards to find the first non-group row
            BOOL foundRow = NO;
            for(NSUInteger i=firstSelectedRow;i>0;i--)
            {
                if(![self isGroupItem:[self itemAtRow:i]]) {
                    [self selectRowIndexes:[NSIndexSet indexSetWithIndex:i] byExtendingSelection:NO];
                    foundRow = YES;
                    break;
                }
            }

            //If there is no non-group row preceding the currently selected group item, remove the selection
            //from the Source List
            if(!foundRow) {
                [self deselectAll:self];
            }
        }
    }
    else if(![self allowsEmptySelection]&&[self numberOfSelectedRows]==0)
    {
        //Select the first non-group row if no rows are selected, and empty selection is disallowed
        for(NSUInteger i=0;i<[self numberOfRows];i++)
        {
            if(![self isGroupItem:[self itemAtRow:i]]) {
                [self selectRowIndexes:[NSIndexSet indexSetWithIndex:i] byExtendingSelection:NO];
                break;
            }
        }
    }
}

- (NSUInteger)numberOfGroups
{
    if([_secondaryDataSource respondsToSelector:@selector(sourceList:numberOfChildrenOfItem:)]) {
        return [_secondaryDataSource sourceList:self numberOfChildrenOfItem:nil];
    }
    return 0;
}


- (BOOL)isGroupItem:(id)item
{
    //Groups are defined as root items (at level 0)
    return 0==[self levelForItem:item];
}


- (BOOL)isGroupAlwaysExpanded:(id)group
{
    //Make sure that the item IS a group to prevent unwanted queries sent to the data source
    if([self isGroupItem:group]) {
        //Query the data source
        if([_secondaryDelegate respondsToSelector:@selector(sourceList:isGroupAlwaysExpanded:)]) {
            return [_secondaryDelegate sourceList:self isGroupAlwaysExpanded:group];
        }
    }

    return NO;
}


- (BOOL)itemHasBadge:(id)item
{
    if([_secondaryDataSource respondsToSelector:@selector(sourceList:itemHasBadge:)]) {
        return [_secondaryDataSource sourceList:self itemHasBadge:item];
    }

    return NO;
}

- (NSInteger)badgeValueForItem:(id)item
{
    //Make sure that the item has a badge
    if(![self itemHasBadge:item]) {
        return NSNotFound;
    }

    if([_secondaryDataSource respondsToSelector:@selector(sourceList:badgeValueForItem:)]) {
        return [_secondaryDataSource sourceList:self badgeValueForItem:item];
    }

    return NSNotFound;
}

#pragma mark -
#pragma mark Selection Handling

- (void)selectRowIndexes:(NSIndexSet*)indexes byExtendingSelection:(BOOL)extend
{
    NSUInteger numberOfIndexes = [indexes count];

    //Prevent empty selection if we don't want it
    if(![self allowsEmptySelection]&&0==numberOfIndexes) {
        return;
    }

    //Would use blocks but we're also targeting 10.5...
    //Get the selected indexes
    NSUInteger *selectedIndexes = malloc(sizeof(NSUInteger)*numberOfIndexes);
    [indexes getIndexes:selectedIndexes maxCount:numberOfIndexes inIndexRange:nil];

    //Loop through the indexes and only add non-group row indexes
    //Allows selection across groups without selecting the group rows
    NSMutableIndexSet *newSelectionIndexes = [NSMutableIndexSet indexSet];
    for(NSInteger i=0;i<numberOfIndexes;i++)
    {
        if(![self isGroupItem:[self itemAtRow:selectedIndexes[i]]]) {
            [newSelectionIndexes addIndex:selectedIndexes[i]];
        }
    }

    //If there are any non-group rows selected
    if([newSelectionIndexes count]>0) {
        [super selectRowIndexes:newSelectionIndexes byExtendingSelection:extend];
    }

    //C memory management... *sigh*
    free(selectedIndexes);
}

#pragma mark -
#pragma mark Layout

- (NSRect)frameOfOutlineCellAtRow:(NSInteger)row
{
    //Return a zero-rect if the item is always expanded (a disclosure triangle will not be drawn)
    if([self isGroupAlwaysExpanded:[self itemAtRow:row]]) {
        return NSZeroRect;
    }

    return [super frameOfOutlineCellAtRow:row];
}


- (NSRect)frameOfCellAtColumn:(NSInteger)column row:(NSInteger)row
{
    id item = [self itemAtRow:row];

    NSCell *cell = [self preparedCellAtColumn:column row:row];
    NSSize cellSize = [cell cellSize];
    if (!([cell type] == NSImageCellType) && !([cell type] == NSTextCellType))
        cellSize = [cell cellSizeForBounds:[super frameOfCellAtColumn:column row:row]];

    NSRect cellFrame = [super frameOfCellAtColumn:column row:row];
    NSRect rowRect = [self rectOfRow:row];

    if([self isGroupItem:item])
    {
        CGFloat minX = NSMinX(cellFrame);

        //Set the origin x-coord; if there are no children of the group at current, there will still be a
        //margin to the left of the cell (in cellFrame), which we don't want
        if([self isGroupAlwaysExpanded:[self itemAtRow:row]]) {
            minX = 7;
        }

    return NSMakeRect(minX, NSMidY(cellFrame)-(cellSize.height/2.0), NSWidth(rowRect)-minX, cellSize.height);
    }
    else
    {
        CGFloat leftIndent = ([self levelForRow:row] -1)*[self indentationPerLevel]+DISCLOSURE_TRIANGLE_SPACE;

        //Calculate space left for a badge if need be
        CGFloat rightIndent = [self sizeOfBadgeAtRow:row].width+ROW_RIGHT_MARGIN;

        //Allow space for an icon if need be
        if(![self isGroupItem:item]&&[_secondaryDataSource respondsToSelector:@selector(sourceList:itemHasIcon:)])
        {
            if([_secondaryDataSource sourceList:self itemHasIcon:item]) {
                leftIndent += [self iconSize].width+(ICON_SPACING*2);
            }
        }
    return NSMakeRect(leftIndent, NSMidY(rowRect)-(cellSize.height/2.0), NSWidth(rowRect)-rightIndent-leftIndent, cellSize.height);
    }
}


//This method calculates and returns the size of the badge for the row index passed to the method. If the
//row for the row index passed to the method does not have a badge, then NSZeroSize is returned.
- (NSSize)sizeOfBadgeAtRow:(NSInteger)rowIndex
{
    id rowItem = [self itemAtRow:rowIndex];

    //Make sure that the item has a badge
    if(![self itemHasBadge:rowItem]) {
        return NSZeroSize;
    }

    NSAttributedString *badgeAttrString = [[NSAttributedString alloc] initWithString:[NSString stringWithFormat:@"%ld", [self badgeValueForItem:rowItem]] attributes:[NSDictionary dictionaryWithObjectsAndKeys:BADGE_FONT, NSFontAttributeName, nil]];
    NSSize stringSize = [badgeAttrString size];

    //Calculate the width needed to display the text or the minimum width if it's smaller
    CGFloat width = stringSize.width+(2*BADGE_MARGIN);

    if(width<MIN_BADGE_WIDTH) {
        width = MIN_BADGE_WIDTH;
    }

    [badgeAttrString release];

    return NSMakeSize(width, BADGE_HEIGHT);
}


#pragma mark -
#pragma mark Drawing

- (void)drawRow:(NSInteger)rowIndex clipRect:(NSRect)clipRect
{
    [super drawRow:rowIndex clipRect:clipRect];
    id item = [self itemAtRow:rowIndex];

    //Draw an icon if the item has one
    if(![self isGroupItem:item]&&[_secondaryDataSource respondsToSelector:@selector(sourceList:itemHasIcon:)])
    {
        if([_secondaryDataSource sourceList:self itemHasIcon:item])
        {
            NSRect cellFrame = [self frameOfCellAtColumn:0 row:rowIndex];
            NSSize iconSize = [self iconSize];
            NSRect iconRect = NSMakeRect(NSMinX(cellFrame)-iconSize.width-ICON_SPACING, NSMidY(cellFrame)-(iconSize.width/2.0f), iconSize.width, iconSize.height);

            if([_secondaryDataSource respondsToSelector:@selector(sourceList:iconForItem:)])
            {
                NSImage *icon = [_secondaryDataSource sourceList:self iconForItem:item];
                if(icon!=nil)
                {
                    NSSize actualIconSize = [icon size];
                    //If the icon is *smaller* than the size retrieved from the -iconSize property, make sure we
                    //reduce the size of the rectangle to draw the icon in, so that it is not stretched.
                    if((actualIconSize.width<iconSize.width)||(actualIconSize.height<iconSize.height))
                    {
                        iconRect = NSMakeRect(NSMidX(iconRect)-(actualIconSize.width/2.0f), NSMidY(iconRect)-(actualIconSize.height/2.0f), actualIconSize.width, actualIconSize.height);
                    }

                    //Use 10.6 NSImage drawing if we can
                    if(NSAppKitVersionNumber >= 1115.2) { // Lion
                        [icon drawInRect:iconRect
                                fromRect:NSZeroRect
                               operation:NSCompositeSourceOver
                                fraction:1
                          respectFlipped:YES hints:nil];
                    }
                    else {
                        [icon setFlipped:[self isFlipped]];
                        [icon drawInRect:iconRect
                                fromRect:NSZeroRect
                               operation:NSCompositeSourceOver
                                fraction:1];
                    }
                }
            }
        }
    }

    //Draw the badge if the item has one
    if([self itemHasBadge:item])
    {
        NSRect rowRect = [self rectOfRow:rowIndex];
        NSSize badgeSize = [self sizeOfBadgeAtRow:rowIndex];

        NSRect badgeFrame = NSMakeRect(NSMaxX(rowRect)-badgeSize.width-ROW_RIGHT_MARGIN,
                                       NSMidY(rowRect)-(badgeSize.height/2.0),
                                       badgeSize.width,
                                       badgeSize.height);
        [self drawBadgeForRow:rowIndex inRect:badgeFrame];
    }
}

- (void)drawBadgeForRow:(NSInteger)rowIndex inRect:(NSRect)badgeFrame
{
    id rowItem = [self itemAtRow:rowIndex];
    NSBezierPath *badgePath = [NSBezierPath bezierPathWithRoundedRect:badgeFrame
                                                              xRadius:(BADGE_HEIGHT/2.0)
                                                              yRadius:(BADGE_HEIGHT/2.0)];

    //Get window and control state to determine colours used
    BOOL isVisible = [[NSApp mainWindow] isVisible];
    BOOL isFocused = [[[self window] firstResponder] isEqual:self];
    NSInteger rowBeingEdited = [self editedRow];

    //Set the attributes based on the row state
    NSDictionary *attributes;
    NSColor *backgroundColor;

    if([[self selectedRowIndexes] containsIndex:rowIndex])
    {
        backgroundColor = [NSColor whiteColor];
        //Set the text color based on window and control state
        NSColor *textColor;
        if(isVisible && (isFocused || rowBeingEdited==rowIndex)) {
            textColor = BADGE_SELECTED_TEXT_COLOR;
        }
        else if(isVisible && !isFocused) {
            textColor = BADGE_SELECTED_UNFOCUSED_TEXT_COLOR;
        }
        else {
            textColor = BADGE_SELECTED_HIDDEN_TEXT_COLOR;
        }

        attributes = [[NSDictionary alloc] initWithObjectsAndKeys:BADGE_FONT, NSFontAttributeName, textColor, NSForegroundColorAttributeName, nil];
    }
    else
    {
        //Set the text colour based on window and control state
        NSColor *badgeColor = [NSColor whiteColor];

        if(isVisible) {
            //If the data source returns a custom colour..
            if([_secondaryDataSource respondsToSelector:@selector(sourceList:badgeBackgroundColorForItem:)]) {
                backgroundColor = [_secondaryDataSource sourceList:self badgeBackgroundColorForItem:rowItem];

            if(backgroundColor==nil)
                backgroundColor = BADGE_BACKGROUND_COLOR;
        }
        else { //Otherwise use the default (purple-blue colour)
            backgroundColor = BADGE_BACKGROUND_COLOR;
        }

        //If the delegate wants a custom badge text colour..
        if([_secondaryDataSource respondsToSelector:@selector(sourceList:badgeTextColorForItem:)]) {
            badgeColor = [_secondaryDataSource sourceList:self badgeTextColorForItem:rowItem];

            if(badgeColor==nil)
                badgeColor = [NSColor whiteColor];
        }
    }
    else { //Gray colour
        backgroundColor = BADGE_HIDDEN_BACKGROUND_COLOR;
        }
    attributes = [[NSDictionary alloc] initWithObjectsAndKeys:BADGE_FONT, NSFontAttributeName, badgeColor, NSForegroundColorAttributeName, nil];
    }

    [backgroundColor set];
    [badgePath fill];

    //Draw the badge text
    NSAttributedString *badgeAttrString = [[NSAttributedString alloc] initWithString:[NSString stringWithFormat:@"%ld", [self badgeValueForItem:rowItem]] attributes:attributes];

    NSSize stringSize = [badgeAttrString size];
    NSPoint badgeTextPoint = NSMakePoint(NSMidX(badgeFrame)-(stringSize.width/2.0), //Center in the badge frame
                                         NSMidY(badgeFrame)-(stringSize.height/2.0)); //Center in the badge frame
    [badgeAttrString drawAtPoint:badgeTextPoint];
    [attributes release];
    [badgeAttrString release];
}

#pragma mark -
#pragma mark Keyboard Handling

- (void)keyDown:(NSEvent *)theEvent
{
    NSIndexSet *selectedIndexes = [self selectedRowIndexes];
    NSString *keyCharacters = [theEvent characters];

    //Make sure we have a selection
    if([selectedIndexes count]>0)
    {
        if([keyCharacters length]>0)
        {
            unichar firstKey = [keyCharacters characterAtIndex:0];
            if(firstKey==NSUpArrowFunctionKey||firstKey==NSDownArrowFunctionKey)
            {
                //Handle keyboard navigation across groups
                if([selectedIndexes count]==1&&!([theEvent modifierFlags] & NSShiftKeyMask))
                {
                    int delta = firstKey==NSDownArrowFunctionKey?1:-1;
                    //Search "backwards" if up arrow, "forwards" if down

                    NSInteger newRow = [selectedIndexes firstIndex];
                    //Keep incrementing/decrementing the row until a non-header row is reached
                    do {
                        newRow+=delta;

                        //If out of bounds of the number of rows..
                        if(newRow<0||newRow==[self numberOfRows])
                            break;
                    } while([self isGroupItem:[self itemAtRow:newRow]]);

                    [self selectRowIndexes:[NSIndexSet indexSetWithIndex:newRow] byExtendingSelection:NO];

                    return;
                }
            }
            else if(firstKey==NSDeleteCharacter||firstKey==NSBackspaceCharacter)
            {
                //Post the notification
                [[NSNotificationCenter defaultCenter] postNotificationName:PXSLDeleteKeyPressedOnRowsNotification object:self  userInfo:[NSDictionary dictionaryWithObject:selectedIndexes forKey:@"rows"]];

                return;

            }

        }

    }

    //We don't care about it
    [super keyDown:theEvent];
}

#pragma mark -
#pragma mark Menu Handling


- (NSMenu *)menuForEvent:(NSEvent *)theEvent
{
    NSMenu * m = nil;

    if([_secondaryDelegate respondsToSelector:@selector(sourceList:menuForEvent:item:)]) {
        NSPoint clickPoint = [self convertPoint:[theEvent locationInWindow] fromView:nil];
        NSInteger row = [self rowAtPoint:clickPoint];
        id clickedItem = [self itemAtRow:row];

        if ([clickedItem sdtype] > 0)
            m = [_secondaryDelegate sourceList:self menuForEvent:theEvent item:clickedItem];
        else
            m = [super menuForEvent:theEvent];
    }

    if (m == nil) {
        m = [super menuForEvent:theEvent];
    }

    return m;
}

#pragma mark -
#pragma mark NSOutlineView Data Source methods

- (NSInteger)outlineView:(NSOutlineView *)outlineView numberOfChildrenOfItem:(id)item
{
    if([_secondaryDataSource conformsToProtocol:@protocol(PXSourceListDataSource)]) {
        return [_secondaryDataSource sourceList:self numberOfChildrenOfItem:item];
    }

    return 0;
}


- (id)outlineView:(NSOutlineView *)outlineView child:(NSInteger)index ofItem:(id)item
{
    if([_secondaryDataSource conformsToProtocol:@protocol(PXSourceListDataSource)]) {
        return [_secondaryDataSource sourceList:self child:index ofItem:item];
    }

    return nil;
}


- (BOOL)outlineView:(NSOutlineView *)outlineView isItemExpandable:(id)item
{
    if([_secondaryDataSource conformsToProtocol:@protocol(PXSourceListDataSource)]) {
        return [_secondaryDataSource sourceList:self isItemExpandable:item];
    }

    return NO;
}


- (id)outlineView:(NSOutlineView *)outlineView objectValueForTableColumn:(NSTableColumn *)tableColumn byItem:(id)item
{
    if([_secondaryDataSource conformsToProtocol:@protocol(PXSourceListDataSource)]) {
        return [_secondaryDataSource sourceList:self objectValueForItem:item];
    }

    return nil;
}


- (void)outlineView:(NSOutlineView *)outlineView setObjectValue:(id)object forTableColumn:(NSTableColumn *)tableColumn byItem:(id)item
{
    if([_secondaryDataSource conformsToProtocol:@protocol(PXSourceListDataSource)]) {
        [_secondaryDataSource sourceList:self setObjectValue:object forItem:item];
    }
}


- (id)outlineView:(NSOutlineView *)outlineView itemForPersistentObject:(id)object
{
    if([_secondaryDataSource respondsToSelector:@selector(sourceList:itemForPersistentObject:)]) {
        return [_secondaryDataSource sourceList:self itemForPersistentObject:object];
    }

    return nil;
}

- (id)outlineView:(NSOutlineView *)outlineView persistentObjectForItem:(id)item
{
    if([_secondaryDataSource respondsToSelector:@selector(sourceList:persistentObjectForItem:)]) {
        return [_secondaryDataSource sourceList:self persistentObjectForItem:item];
    }

    return nil;
}

- (BOOL)outlineView:(NSOutlineView *)outlineView writeItems:(NSArray *)items toPasteboard:(NSPasteboard *)pasteboard
{
    if([_secondaryDataSource respondsToSelector:@selector(sourceList:writeItems:toPasteboard:)]) {
        return [_secondaryDataSource sourceList:self writeItems:items toPasteboard:pasteboard];
    }

    return NO;
}

- (NSDragOperation)outlineView:(NSOutlineView *)outlineView validateDrop:(id <NSDraggingInfo>)info proposedItem:(id)item proposedChildIndex:(NSInteger)index
{
    if([_secondaryDataSource respondsToSelector:@selector(sourceList:validateDrop:proposedItem:proposedChildIndex:)]) {
        return [_secondaryDataSource sourceList:self validateDrop:info proposedItem:item proposedChildIndex:index];
    }

    return NSDragOperationNone;
}

- (BOOL)outlineView:(NSOutlineView *)outlineView acceptDrop:(id <NSDraggingInfo>)info item:(id)item childIndex:(NSInteger)index
{
    if([_secondaryDataSource respondsToSelector:@selector(sourceList:acceptDrop:item:childIndex:)]) {
        return [_secondaryDataSource sourceList:self acceptDrop:info item:item childIndex:index];
    }

    return NO;
}
- (NSArray *)outlineView:(NSOutlineView *)outlineView namesOfPromisedFilesDroppedAtDestination:(NSURL *)dropDestination forDraggedItems:(NSArray *)items
{
    if([_secondaryDataSource respondsToSelector:@selector(sourceList:namesOfPromisedFilesDroppedAtDestination:forDraggedItems:)]) {
        return [_secondaryDataSource sourceList:self namesOfPromisedFilesDroppedAtDestination:dropDestination forDraggedItems:items];
    }

    return nil;
}


#pragma mark -
#pragma mark NSOutlineView Delegate methods

- (BOOL)outlineView:(NSOutlineView *)outlineView shouldExpandItem:(id)item
{
    if([_secondaryDelegate respondsToSelector:@selector(sourceList:shouldExpandItem:)]) {
        return [_secondaryDelegate sourceList:self shouldExpandItem:item];
    }

    return YES;
}

- (BOOL)outlineView:(NSOutlineView *)outlineView shouldCollapseItem:(id)item
{
    //Make sure the item isn't displayed as always expanded
    if([self isGroupItem:item])
    {
        if([self isGroupAlwaysExpanded:item]) {
            return NO;
        }
    }

    if([_secondaryDelegate respondsToSelector:@selector(sourceList:shouldCollapseItem:)]) {
        return [_secondaryDelegate sourceList:self shouldCollapseItem:item];
    }

    return YES;
}

- (NSCell *)outlineView:(NSOutlineView *)outlineView dataCellForTableColumn:(NSTableColumn *)tableColumn item:(id)item
{
    if([_secondaryDelegate respondsToSelector:@selector(sourceList:dataCellForItem:)]) {
        return [_secondaryDelegate sourceList:self dataCellForItem:item];
    }

    NSInteger row = [self rowForItem:item];

    //Return the default table column
    return [[[self tableColumns] objectAtIndex:0] dataCellForRow:row];
}

- (void)outlineView:(NSOutlineView *)outlineView willDisplayCell:(id)cell forTableColumn:(NSTableColumn *)tableColumn item:(id)item
{
    if([_secondaryDelegate respondsToSelector:@selector(sourceList:willDisplayCell:forItem:)]) {
        [_secondaryDelegate sourceList:self willDisplayCell:cell forItem:item];
    }
}

- (BOOL)outlineView:(NSOutlineView *)outlineView shouldSelectItem:(id)item
{
    //Make sure that the item isn't a group as they can't be selected
    if(![self isGroupItem:item]) {
        if([_secondaryDelegate respondsToSelector:@selector(sourceList:shouldSelectItem:)]) {
            return [_secondaryDelegate sourceList:self shouldSelectItem:item];
        }
    }
    else {
        return NO;
    }

    return YES;
}


- (NSIndexSet *)outlineView:(NSOutlineView *)outlineView selectionIndexesForProposedSelection:(NSIndexSet *)proposedSelectionIndexes
{
    //The outline view will try to select the first row if -[allowsEmptySelection:] is set to NO – if this is a group row
    //stop it from doing so and leave it to our implementation of-[reloadData] which will select the first non-group row
    //for us.
    if([self numberOfSelectedRows]==0) {
        if([self isGroupItem:[self itemAtRow:[proposedSelectionIndexes firstIndex]]]) {
            return [NSIndexSet indexSet];
        }
    }

    if([_secondaryDelegate respondsToSelector:@selector(sourceList:selectionIndexesForProposedSelection:)]) {
        return [_secondaryDelegate sourceList:self selectionIndexesForProposedSelection:proposedSelectionIndexes];
    }

    //Since we implement this method, something must be returned to the outline view
        return proposedSelectionIndexes;
}

- (BOOL)outlineView:(NSOutlineView *)outlineView shouldEditTableColumn:(NSTableColumn *)tableColumn item:(id)item
{
    //Group titles can't be edited
    if([self isGroupItem:item])
        return NO;

    if([_secondaryDelegate respondsToSelector:@selector(sourceList:shouldEditItem:)]) {
        return [_secondaryDelegate sourceList:self shouldEditItem:item];
    }

    return YES;
}


- (BOOL)outlineView:(NSOutlineView *)outlineView shouldTrackCell:(NSCell *)cell forTableColumn:(NSTableColumn *)tableColumn item:(id)item
{
    if([_secondaryDelegate respondsToSelector:@selector(sourceList:shouldTrackCell:forItem:)]) {
        return [_secondaryDelegate sourceList:self shouldTrackCell:cell forItem:item];
    }

    return NO;
}

- (CGFloat)outlineView:(NSOutlineView *)outlineView heightOfRowByItem:(id)item
{
    if([_secondaryDelegate respondsToSelector:@selector(sourceList:heightOfRowByItem:)]) {
        return [_secondaryDelegate sourceList:self heightOfRowByItem:item];
    }

    return [self rowHeight];
}

- (BOOL)outlineView:(NSOutlineView *)outlineView isGroupItem:(id)item
{
    return [self isGroupItem:item];
}

#pragma mark -
#pragma mark Notification handling

/* Notification wrappers */
- (void)outlineViewSelectionIsChanging:(NSNotification *)notification
{
    [[NSNotificationCenter defaultCenter] postNotificationName:PXSLSelectionIsChangingNotification object:self];
}


- (void)outlineViewSelectionDidChange:(NSNotification *)notification
{
    [[NSNotificationCenter defaultCenter] postNotificationName:PXSLSelectionDidChangeNotification object:self];
}

- (void)outlineViewItemWillExpand:(NSNotification *)notification
{
    [[NSNotificationCenter defaultCenter] postNotificationName:PXSLItemWillExpandNotification object:self userInfo:[notification userInfo]];
}

- (void)outlineViewItemDidExpand:(NSNotification *)notification
{
    [[NSNotificationCenter defaultCenter] postNotificationName:PXSLItemDidExpandNotification object:self userInfo:[notification userInfo]];
}

- (void)outlineViewItemWillCollapse:(NSNotification *)notification
{
    [[NSNotificationCenter defaultCenter] postNotificationName:PXSLItemWillCollapseNotification object:self userInfo:[notification userInfo]];
}

- (void)outlineViewItemDidCollapse:(NSNotification *)notification
{
    [[NSNotificationCenter defaultCenter] postNotificationName:PXSLItemDidCollapseNotification object:self userInfo:[notification userInfo]];
}

- (void)registerDelegateToReceiveNotification:(NSString*)notification withSelector:(SEL)selector
{
    NSNotificationCenter *defaultCenter = [NSNotificationCenter defaultCenter];

    //Set the delegate as a receiver of the notification if it implements the notification method
    if([_secondaryDelegate respondsToSelector:selector]) {
        [defaultCenter addObserver:_secondaryDelegate selector:selector name:notification object:self];
    }
}

@end
