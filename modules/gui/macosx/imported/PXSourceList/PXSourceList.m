//
//  PXSourceList.m
//  PXSourceList
//
//  Created by Alex Rozanski on 05/09/2009.
//  Copyright 2009-14 Alex Rozanski http://alexrozanski.com and other contributors.
//  This software is licensed under the New BSD License. Full details can be found in the README.
//

#import "PXSourceList.h"
#import "PXSourceListBadgeCell.h"
#import "Internal/PXSourceListDelegateDataSourceProxy.h"
#import "Internal/PXSourceListPrivateConstants.h"

//Layout constants
static const CGFloat minBadgeWidth = 22.0;              // The minimum badge width for each item (default 22.0).
static const CGFloat badgeHeight = 14.0;                // The badge height for each item (default 14.0).
static const CGFloat rowRightMargin = 5.0;              // The spacing between the right edge of the badge and the edge of the table column.
static const CGFloat iconSpacing = 2.0;                 // The spacing between the icon and it's adjacent cell.
static const CGFloat disclosureTriangleSpace = 18.0;    // The indentation reserved for disclosure triangles for non-group items.

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

@property (strong, nonatomic) PXSourceListDelegateDataSourceProxy *delegateDataSourceProxy;
@property (strong, readonly) PXSourceListBadgeCell *reusableBadgeCell;

@end

#pragma mark -
@implementation PXSourceList

@synthesize reusableBadgeCell = _reusableBadgeCell;

#pragma mark - Setup/Teardown

- (id)initWithCoder:(NSCoder*)decoder
{
    if(self=[super initWithCoder:decoder]) {
        [self PXSL_setup];
    }

    return self;
}

- (id)initWithFrame:(NSRect)frameRect
{
    if((self = [super initWithFrame:frameRect])) {
        [self PXSL_setup];
    }

    return self;
}

- (void)PXSL_setup
{
    _iconSize = NSMakeSize(16,16);
    _delegateDataSourceProxy = [[PXSourceListDelegateDataSourceProxy alloc] initWithSourceList:self];
}

- (void)dealloc
{
    _delegateDataSourceProxy = nil;
    //Remove ourselves as the delegate and data source to be safe
    [super setDataSource:nil];
    [super setDelegate:nil];
}


#pragma mark -
#pragma mark Custom Accessors

- (void)setDelegate:(id<PXSourceListDelegate>)aDelegate
{
    self.delegateDataSourceProxy.delegate = aDelegate;

    [super setDelegate:nil];
    if (aDelegate)
        [super setDelegate:self.delegateDataSourceProxy];
}


- (void)setDataSource:(id<PXSourceListDataSource>)aDataSource
{
    self.delegateDataSourceProxy.dataSource = aDataSource;

    [super setDataSource:nil];
    if (aDataSource)
        [super setDataSource:self.delegateDataSourceProxy];

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

- (PXSourceListBadgeCell *)reusableBadgeCell
{
    if (!_reusableBadgeCell)
        _reusableBadgeCell = [[PXSourceListBadgeCell alloc] init];

    return _reusableBadgeCell;
}

- (BOOL)floatsGroupRows
{
    return NO;
}

- (void)setFloatsGroupRows:(BOOL)value
{
    [super setFloatsGroupRows:NO];
}

#pragma mark -
#pragma mark Data Management

- (BOOL)isViewBasedSourceList
{
    return [self.delegateDataSourceProxy respondsToSelector:@selector(sourceList:viewForItem:)];
}

- (void)reloadData
{
    [super reloadData];

    //Expand items that are displayed as always expanded
    if([self.delegateDataSourceProxy conformsToProtocol:@protocol(PXSourceListDataSource)] &&
       [self.delegateDataSourceProxy respondsToSelector:@selector(sourceList:isGroupAlwaysExpanded:)])
    {
        for(NSUInteger i=0;i<[self numberOfGroups];i++)
        {
            id item = [self.delegateDataSourceProxy sourceList:self child:i ofItem:nil];

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
    if([self.delegateDataSourceProxy respondsToSelector:@selector(sourceList:numberOfChildrenOfItem:)]) {
        return [self.delegateDataSourceProxy sourceList:self numberOfChildrenOfItem:nil];
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
        if([self.delegateDataSourceProxy respondsToSelector:@selector(sourceList:isGroupAlwaysExpanded:)]) {
            return [self.delegateDataSourceProxy sourceList:self isGroupAlwaysExpanded:group];
        }
    }

    return NO;
}


- (BOOL)itemHasBadge:(id)item
{
    // Since badges are managed by custom views and logic in view-based mode, we can't determine this.
    if (self.isViewBasedSourceList)
        return NO;

    if([self.delegateDataSourceProxy respondsToSelector:@selector(sourceList:itemHasBadge:)]) {
        return [self.delegateDataSourceProxy sourceList:self itemHasBadge:item];
    }

    return NO;
}

- (NSInteger)badgeValueForItem:(id)item
{
    // Since badges are managed by custom views and logic in view-based mode, we can't determine this.
    if (self.isViewBasedSourceList || ![self itemHasBadge:item])
        return NSNotFound;

    if([self.delegateDataSourceProxy respondsToSelector:@selector(sourceList:badgeValueForItem:)]) {
        return [self.delegateDataSourceProxy sourceList:self badgeValueForItem:item];
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

    NSRect frame = [super frameOfOutlineCellAtRow:row];

    if([self levelForRow:row] > 0) {
        frame.origin.x = [self levelForRow:row] * [self indentationPerLevel];
    }

    return frame;
}


- (NSRect)frameOfCellAtColumn:(NSInteger)column row:(NSInteger)row
{
    if (self.isViewBasedSourceList)
        return [super frameOfCellAtColumn:column row:row];

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

        return NSMakeRect(minX,
                          NSMidY(cellFrame)-(cellSize.height/2.0),
                          NSWidth(rowRect)-minX,
                          cellSize.height);
    }
    else
    {
        CGFloat leftIndent = [self levelForRow:row]*[self indentationPerLevel]+disclosureTriangleSpace;

        //Calculate space left for a badge if need be
        CGFloat rightIndent = [self sizeOfBadgeAtRow:row].width+rowRightMargin;

        //Allow space for an icon if need be
        if(![self isGroupItem:item] && [self.delegateDataSourceProxy respondsToSelector:@selector(sourceList:itemHasIcon:)])
        {
            if([self.delegateDataSourceProxy sourceList:self itemHasIcon:item]) {
                leftIndent += [self iconSize].width+(iconSpacing*2);
            }
        }

        return NSMakeRect(leftIndent,
                          NSMidY(rowRect)-(cellSize.height/2.0),
                          NSWidth(rowRect)-rightIndent-leftIndent,
                          cellSize.height);
    }
}


//This method calculates and returns the size of the badge for the row index passed to the method. If the
//row for the row index passed to the method does not have a badge, then NSZeroSize is returned.
- (NSSize)sizeOfBadgeAtRow:(NSInteger)rowIndex
{
    id rowItem = [self itemAtRow:rowIndex];

    if (![self itemHasBadge:rowItem])
        return NSZeroSize;

    self.reusableBadgeCell.integerValue = [self badgeValueForItem:rowItem];

    return NSMakeSize(fmax(self.reusableBadgeCell.cellSize.width, minBadgeWidth), badgeHeight);
}

- (void)viewDidMoveToSuperview
{
    //If set to YES, this will cause display issues in Lion where the right part of the outline view is cut off
    [self setAutoresizesOutlineColumn:NO];
}

#pragma mark -
#pragma mark Drawing

- (void)drawRow:(NSInteger)rowIndex clipRect:(NSRect)clipRect
{
    [super drawRow:rowIndex clipRect:clipRect];

    // We only do drawing here if the Source List is cell-based.
    if (self.isViewBasedSourceList)
        return;

    id item = [self itemAtRow:rowIndex];

    //Draw an icon if the item has one
    if(![self isGroupItem:item] && [self.delegateDataSourceProxy respondsToSelector:@selector(sourceList:itemHasIcon:)])
    {
        if([self.delegateDataSourceProxy sourceList:self itemHasIcon:item])
        {
            NSRect cellFrame = [self frameOfCellAtColumn:0 row:rowIndex];
            NSSize iconSize = [self iconSize];
            NSRect iconRect = NSMakeRect(NSMinX(cellFrame)-iconSize.width-iconSpacing,
                                         NSMidY(cellFrame)-(iconSize.height/2.0f),
                                         iconSize.width,
                                         iconSize.height);

            if([self.delegateDataSourceProxy respondsToSelector:@selector(sourceList:iconForItem:)])
            {
                NSImage *icon = [self.delegateDataSourceProxy sourceList:self iconForItem:item];

                if(icon!=nil)
                {
                    NSSize actualIconSize = [icon size];

                    //If the icon is *smaller* than the size retrieved from the -iconSize property, make sure we
                    //reduce the size of the rectangle to draw the icon in, so that it is not stretched.
                    if((actualIconSize.width<iconSize.width)||(actualIconSize.height<iconSize.height))
                    {
                        iconRect = NSMakeRect(NSMidX(iconRect)-(actualIconSize.width/2.0f),
                                              NSMidY(iconRect)-(actualIconSize.height/2.0f),
                                              actualIconSize.width,
                                              actualIconSize.height);
                    }

                    [icon drawInRect:iconRect
                            fromRect:NSZeroRect
                           operation:NSCompositeSourceOver
                            fraction:1
                      respectFlipped:YES hints:nil];
                }
            }
        }
    }

    //Draw the badge if the item has one
    if([self itemHasBadge:item])
    {
        NSRect rowRect = [self rectOfRow:rowIndex];
        NSSize badgeSize = [self sizeOfBadgeAtRow:rowIndex];

        NSRect badgeFrame = NSMakeRect(NSMaxX(rowRect)-badgeSize.width-rowRightMargin,
                                       NSMidY(rowRect)-(badgeSize.height/2.0),
                                       badgeSize.width,
                                       badgeSize.height);

        [self drawBadgeForRow:rowIndex inRect:badgeFrame];
    }
}

- (void)drawBadgeForRow:(NSInteger)rowIndex inRect:(NSRect)badgeFrame
{
    id rowItem = [self itemAtRow:rowIndex];

    self.reusableBadgeCell.badgeValue = [self badgeValueForItem:rowItem];
    if ([self.delegateDataSourceProxy respondsToSelector:@selector(sourceList:badgeTextColorForItem:)])
        self.reusableBadgeCell.textColor = [self.delegateDataSourceProxy sourceList:self badgeTextColorForItem:rowItem];
    if ([self.delegateDataSourceProxy respondsToSelector:@selector(sourceList:badgeBackgroundColorForItem:)])
        self.reusableBadgeCell.textColor = [self.delegateDataSourceProxy sourceList:self badgeBackgroundColorForItem:rowItem];
    self.reusableBadgeCell.highlighted = [self.selectedRowIndexes containsIndex:rowIndex];

    [self.reusableBadgeCell drawWithFrame:badgeFrame inView:self];
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
                    int delta = firstKey==NSDownArrowFunctionKey?1:-1;  //Search "backwards" if up arrow, "forwards" if down
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
            else if(firstKey==NSDeleteCharacter||firstKey==NSBackspaceCharacter||firstKey==0xf728)
            {
                //Post the notification
                [[NSNotificationCenter defaultCenter] postNotificationName:PXSLDeleteKeyPressedOnRowsNotification
                                                                    object:self
                                                                  userInfo:[NSDictionary dictionaryWithObject:selectedIndexes forKey:@"rows"]];

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
    if([self.delegateDataSourceProxy respondsToSelector:@selector(sourceList:menuForEvent:item:)]) {
        NSPoint clickPoint = [self convertPoint:[theEvent locationInWindow] fromView:nil];
        NSInteger row = [self rowAtPoint:clickPoint];
        id clickedItem = [self itemAtRow:row];
        m = [self.delegateDataSourceProxy sourceList:self menuForEvent:theEvent item:clickedItem];
    }
    if (m == nil) {
        m = [super menuForEvent:theEvent];
    }
    return m;
}

#pragma mark - Custom NSOutlineView Delegate Method Implementations

- (BOOL)outlineView:(NSOutlineView *)outlineView shouldCollapseItem:(id)item
{
    // Make sure the item isn't displayed as always expanded
    if([self isGroupItem:item] && [self isGroupAlwaysExpanded:item])
        return NO;

    if([self.delegateDataSourceProxy respondsToSelector:@selector(sourceList:shouldCollapseItem:)])
        return [self.delegateDataSourceProxy sourceList:self shouldCollapseItem:item];

    return YES;
}

- (BOOL)outlineView:(NSOutlineView *)outlineView shouldSelectItem:(id)item
{
    // Make sure that the item isn't a group as they can't be selected
    if(![self isGroupItem:item] && [self.delegateDataSourceProxy respondsToSelector:@selector(sourceList:shouldSelectItem:)])
        return [self.delegateDataSourceProxy sourceList:self shouldSelectItem:item];

    return ![self isGroupItem:item];
}

- (BOOL)outlineView:(NSOutlineView *)outlineView shouldEditTableColumn:(NSTableColumn *)tableColumn item:(id)item
{
    if(![self isGroupItem:item] && [self.delegateDataSourceProxy respondsToSelector:@selector(sourceList:shouldEditItem:)])
        return [self.delegateDataSourceProxy sourceList:self shouldEditItem:item];

    return ![self isGroupItem:item];
}

- (BOOL)outlineView:(NSOutlineView *)outlineView isGroupItem:(id)item
{
    return [self isGroupItem:item];
}

@end
