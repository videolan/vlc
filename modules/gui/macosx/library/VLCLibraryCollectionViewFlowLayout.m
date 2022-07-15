/*****************************************************************************
 * VLCLibraryCollectionViewFlowLayout.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2022 VLC authors and VideoLAN
 *
 * Authors: Claudio Cambra <claudio.cambra@gmail.com>
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

#import "VLCLibraryCollectionViewFlowLayout.h"
#import "VLCLibraryCollectionViewAlbumSupplementaryDetailView.h"

#pragma mark - Private data
static const NSUInteger kAnimationSteps = 16;
static const NSUInteger kWrapAroundValue = (NSUInteger)-1;

static const CGFloat kDetailViewMargin = 8.;
static const CGFloat kDetailViewCollapsedHeight = 0.;
static const CGFloat kDetailViewExpandedHeight = 300.;

static const CGFloat kAnimationHeightIncrement = kDetailViewExpandedHeight / (CGFloat)kAnimationSteps;

typedef NS_ENUM(NSUInteger, VLCDetailViewAnimationType)
{
    VLCDetailViewAnimationTypeExpand,
    VLCDetailViewAnimationTypeCollapse,
};

static CVReturn detailViewAnimationCallback(CVDisplayLinkRef displayLink,
                                            const CVTimeStamp *inNow,
                                            const CVTimeStamp *inOutputTime,
                                            CVOptionFlags flagsIn,
                                            CVOptionFlags *flagsOut,
                                            void *displayLinkContext);

#pragma mark - VLCLibraryCollectionViewFlowLayout
@interface VLCLibraryCollectionViewFlowLayout ()
{
    NSUInteger _lastHeightIndex;
    CVDisplayLinkRef _displayLinkRef;    
}

@property (nonatomic, readwrite) BOOL detailViewIsAnimating;
@property (nonatomic, readonly)  BOOL animationIsCollapse;
@property (nonatomic, readwrite) NSUInteger animationIndex;
@property (nonatomic, readwrite, strong) NSIndexPath *selectedIndexPath;

@end

@implementation VLCLibraryCollectionViewFlowLayout

- (instancetype)init
{
    self = [super init];
    if (self == nil) {
        return nil;
    }
    
    [self resetLayout];
    
    return self;
}

#pragma mark - Public methods
- (void)expandDetailSectionAtIndex:(NSIndexPath *)indexPath
{
    if([_selectedIndexPath isEqual:indexPath]) {
        return;
    }

    _selectedIndexPath = indexPath;
    [self animateDetailViewWithAnimation:VLCDetailViewAnimationTypeExpand];
}

- (void)collapseDetailSectionAtIndex:(NSIndexPath *)indexPath
{
    if(![_selectedIndexPath isEqual:indexPath]) {
        return;
    }

    [self animateDetailViewWithAnimation:VLCDetailViewAnimationTypeCollapse];
}

- (void)resetLayout
{
    [self releaseDisplayLink];

    _selectedIndexPath = nil;
    _detailViewIsAnimating = NO;
    _animationIndex = 0;

    [self invalidateLayout];
}

#pragma mark - Flow Layout methods

- (NSCollectionViewLayoutAttributes *)layoutAttributesForItemAtIndexPath:(NSIndexPath *)indexPath
{
    NSCollectionViewLayoutAttributes *attributes = [super layoutAttributesForItemAtIndexPath:indexPath];

    if(_selectedIndexPath == nil || indexPath == _selectedIndexPath) {
        return attributes;
    }

    [attributes setFrame:[self frameForDisplacedAttributes:attributes]];
    return attributes;
}

- (NSArray<__kindof NSCollectionViewLayoutAttributes *> *)layoutAttributesForElementsInRect:(NSRect)rect
{
    if (_selectedIndexPath == nil) {
        return [super layoutAttributesForElementsInRect:rect];
    }

    NSRect selectedItemFrame = [[self layoutAttributesForItemAtIndexPath:_selectedIndexPath] frame];

    // Computed attributes from parent
    NSMutableArray<__kindof NSCollectionViewLayoutAttributes *> *layoutAttributesArray = [[super layoutAttributesForElementsInRect:rect] mutableCopy];
    for (int i = 0; i < layoutAttributesArray.count; i++) {
        NSCollectionViewLayoutAttributes *attributes = layoutAttributesArray[i];
        [attributes setFrame:[self frameForDisplacedAttributes:attributes]];
        layoutAttributesArray[i] = attributes;
    }

    // Add detail view to the attributes set -- detail view about to be shown
    [layoutAttributesArray addObject:[self layoutAttributesForSupplementaryViewOfKind:VLCLibraryCollectionViewAlbumSupplementaryDetailViewKind atIndexPath:self.selectedIndexPath]];
    
    return layoutAttributesArray;
}

- (NSCollectionViewLayoutAttributes *)layoutAttributesForSupplementaryViewOfKind:(NSCollectionViewSupplementaryElementKind)elementKind
                                                                     atIndexPath:(NSIndexPath *)indexPath
{
    if ([elementKind isEqualToString:VLCLibraryCollectionViewAlbumSupplementaryDetailViewKind]) {
        NSCollectionViewLayoutAttributes *detailViewAttributes = [NSCollectionViewLayoutAttributes layoutAttributesForSupplementaryViewOfKind:elementKind
                                                                                                                                withIndexPath:indexPath];
        NSAssert1(detailViewAttributes != NULL,
                  @"Failed to create NSCollectionViewLayoutAttributes for view of kind %@.",
                  VLCLibraryCollectionViewAlbumSupplementaryDetailViewKind );
        
        float selectedItemFrameMaxY = _selectedIndexPath == nil ? 0 : NSMaxY([[self layoutAttributesForItemAtIndexPath:_selectedIndexPath] frame]);
        detailViewAttributes.frame = NSMakeRect(NSMinX(self.collectionView.frame),
                                                selectedItemFrameMaxY + kDetailViewMargin,
                                                self.collectionViewContentSize.width - 16.0,
                                                (_animationIndex * kAnimationHeightIncrement));

        return detailViewAttributes;
    }
    
    NSCollectionViewLayoutAttributes *attributes = [super layoutAttributesForSupplementaryViewOfKind:elementKind
                                                                                         atIndexPath:indexPath];
    [attributes setFrame:[self frameForDisplacedAttributes:attributes]];
    return attributes;
}

- (NSSet<NSIndexPath *> *)indexPathsToDeleteForSupplementaryViewOfKind:(NSString *)elementKind 
{
    if ([elementKind isEqualToString:VLCLibraryCollectionViewAlbumSupplementaryDetailViewKind]) {
        return [self.collectionView indexPathsForVisibleSupplementaryElementsOfKind:elementKind];
    }
    return [NSSet set];
}

# pragma mark - Calculation of displaced frame attributes

- (NSRect)frameForDisplacedAttributes:(NSCollectionViewLayoutAttributes *)inAttributes {
    NSRect attributesFrame = inAttributes.frame;
    if (self.selectedIndexPath) {
        NSRect selectedItemFrame = [[self layoutAttributesForItemAtIndexPath:_selectedIndexPath] frame];
        if (NSMinY(attributesFrame) > (NSMaxY(selectedItemFrame))) {
            attributesFrame.origin.y += (_animationIndex * kAnimationHeightIncrement) + kDetailViewMargin;
        }
    }
    return attributesFrame;
}

#pragma mark - Detail view animation
- (void)animateDetailViewWithAnimation:(VLCDetailViewAnimationType)type
{
    if (type == VLCDetailViewAnimationTypeExpand) {
        _animationIsCollapse = NO;
        _animationIndex = kWrapAroundValue;
        _lastHeightIndex = kAnimationSteps - 1;
    } else {
        _animationIsCollapse = YES;
        _animationIndex = kAnimationSteps;
        _lastHeightIndex = 0;
    }
    
    _detailViewIsAnimating = YES;
    
    if (_displayLinkRef == NULL) {
        [self initDisplayLink];
    }
}

- (void)initDisplayLink
{
    const CVReturn createResult = CVDisplayLinkCreateWithActiveCGDisplays(&_displayLinkRef);
    
    if ((createResult != kCVReturnSuccess) || (_displayLinkRef == NULL)) {
        _detailViewIsAnimating = NO;
        return;
    }
    
    CVDisplayLinkSetOutputCallback(_displayLinkRef, detailViewAnimationCallback, (__bridge void *)self);
    CVDisplayLinkStart(_displayLinkRef);
}

- (void)releaseDisplayLink
{
    if (_displayLinkRef == NULL ) {
        return;
    }
    
    CVDisplayLinkStop(_displayLinkRef);
    CVDisplayLinkRelease(_displayLinkRef);
    
    _displayLinkRef = NULL;
}

@end

static CVReturn detailViewAnimationCallback(
                                            CVDisplayLinkRef displayLink,
                                            const CVTimeStamp *inNow,
                                            const CVTimeStamp *inOutputTime,
                                            CVOptionFlags flagsIn,
                                            CVOptionFlags *flagsOut,
                                            void *displayLinkContext)
{
    VLCLibraryCollectionViewFlowLayout *bridgedSelf = (__bridge VLCLibraryCollectionViewFlowLayout *)displayLinkContext;
    BOOL animationFinished = NO;
    
    if(bridgedSelf.detailViewIsAnimating) {
        if (bridgedSelf.animationIsCollapse) {
            --bridgedSelf.animationIndex;
            animationFinished = (bridgedSelf.animationIndex == kWrapAroundValue);
        } else {
            ++bridgedSelf.animationIndex;
            animationFinished = (bridgedSelf.animationIndex == kAnimationSteps);
        }
    }
    
    if (bridgedSelf.detailViewIsAnimating == NO || animationFinished) {
        bridgedSelf.detailViewIsAnimating = NO;
        [bridgedSelf releaseDisplayLink];
        
        if (bridgedSelf.animationIsCollapse) {
            bridgedSelf.selectedIndexPath = nil;
            bridgedSelf.animationIndex = 0;
        }
    }

    dispatch_async(dispatch_get_main_queue(), ^(void){
        [bridgedSelf invalidateLayout];
    });
    
    return kCVReturnSuccess;
}

