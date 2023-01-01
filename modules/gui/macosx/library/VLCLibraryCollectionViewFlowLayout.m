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

#import "library/VLCLibraryCollectionViewMediaItemSupplementaryDetailView.h"
#import "library/VLCLibraryUIUnits.h"

#import "library/audio-library/VLCLibraryAudioDataSource.h"
#import "library/audio-library/VLCLibraryAudioGroupDataSource.h"
#import "library/audio-library/VLCLibraryCollectionViewAlbumSupplementaryDetailView.h"
#import "library/audio-library/VLCLibraryCollectionViewAudioGroupSupplementaryDetailView.h"

#import "library/home-library/VLCLibraryHomeViewVideoContainerViewDataSource.h"

#import "library/video-library/VLCLibraryVideoDataSource.h"

#pragma mark - Private data
static const NSUInteger kAnimationSteps = 32;
static const NSUInteger kWrapAroundValue = (NSUInteger)-1;

static const CGFloat kDetailViewCollapsedHeight = 0.;

typedef NS_ENUM(NSUInteger, VLCDetailViewAnimationType)
{
    VLCDetailViewAnimationTypeExpand,
    VLCDetailViewAnimationTypeCollapse,
};

typedef NS_ENUM(NSUInteger, VLCExpandAnimationType) {
    VLCExpandAnimationTypeVerticalMedium = 0,
    VLCExpandAnimationTypeVerticalLarge,
    VLCExpandAnimationTypeHorizontalMedium,
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
    CVDisplayLinkRef _displayLinkRef;

    NSArray *_mediumHeightAnimationSteps;
    NSArray *_largeHeightAnimationSteps;
    NSArray *_mediumWidthAnimationSteps;
    
    VLCExpandAnimationType _animationType;
    CGFloat _prevProvidedAnimationStep;

    BOOL _invalidateAll;
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
    if (self) {
        _defaultHeightAnimationSteps = [NSArray arrayWithArray:[self generateAnimationStepsForExpandedViewHeight:VLCLibraryUIUnits.mediumDetailSupplementaryViewCollectionViewHeight]];
        _largeHeightAnimationSteps = [NSArray arrayWithArray:[self generateAnimationStepsForExpandedViewHeight:VLCLibraryUIUnits.largeDetailSupplementaryViewCollectionViewHeight]];
        _mediumWidthAnimationSteps = [NSArray arrayWithArray:[self generateAnimationStepsForExpandedViewDimension:VLCLibraryUIUnits.mediumDetailSupplementaryViewCollectionViewWidth]];

        _animationType = VLCExpandAnimationTypeVerticalMedium;
        _prevProvidedAnimationStep = 0;

        _invalidateAll = NO;

        [self resetLayout];
    }

    return self;
}

- (NSArray *)generateAnimationStepsForExpandedViewDimension:(NSInteger)dimension
{
    NSMutableArray *generatedAnimationSteps = [NSMutableArray arrayWithCapacity:kAnimationSteps];

    // Easing out cubic
    for(int i = 0; i < kAnimationSteps; ++i) {
        CGFloat progress = (CGFloat)i  / (CGFloat)kAnimationSteps;
        progress -= 1;
        generatedAnimationSteps[i] = @(dimension * (progress * progress * progress + 1) + kDetailViewCollapsedHeight);
    }

    return [generatedAnimationSteps copy];
}

- (CGFloat)currentAnimationStep
{
    if (_animationIndex < 0 || _animationIndex >= kAnimationSteps) {
        return _prevProvidedAnimationStep; // Try to disguise problem
    }

    switch(_animationType) {
        case VLCExpandAnimationTypeHorizontalMedium:
            _prevProvidedAnimationStep = [_mediumWidthAnimationSteps[_animationIndex] floatValue];
        case VLCExpandAnimationTypeVerticalLarge:
            _prevProvidedAnimationStep = [_largeHeightAnimationSteps[_animationIndex] floatValue];
            break;
        case VLCExpandAnimationTypeVerticalMedium:
        default:
            _prevProvidedAnimationStep = [_mediumHeightAnimationSteps[_animationIndex] floatValue];
            break;
    }

    return _prevProvidedAnimationStep;
}

#pragma mark - Public methods
- (void)expandDetailSectionAtIndex:(NSIndexPath *)indexPath
{
    if([_selectedIndexPath isEqual:indexPath]) {
        return;
    }

    BOOL newItemOnSameRow = NO;
    if (_selectedIndexPath != nil) {
        NSCollectionViewItem * const oldSelectedItem = [self.collectionView itemAtIndexPath:_selectedIndexPath];
        NSCollectionViewItem * const newSelectedItem = [self.collectionView itemAtIndexPath:indexPath];

        newItemOnSameRow = oldSelectedItem.view.frame.origin.y == newSelectedItem.view.frame.origin.y;
    }

    _selectedIndexPath = indexPath;

    if (!newItemOnSameRow) {
        [self animateDetailViewWithAnimation:VLCDetailViewAnimationTypeExpand];
    } else {
        _animationIsCollapse = NO;
    }
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

- (NSSize)collectionViewContentSize
{
    NSSize contentSize = [super collectionViewContentSize];

    if (!_selectedIndexPath) {
        return contentSize;
    }

    if (self.scrollDirection == NSCollectionViewScrollDirectionVertical) {
        contentSize.height += [self currentAnimationStep];
    } else if (self.scrollDirection == NSCollectionViewScrollDirectionHorizontal) {
        contentSize.width += [self currentAnimationStep];
    }
    return contentSize;
}

- (BOOL)shouldInvalidateLayoutForBoundsChange:(NSRect)newBounds
{
    [super shouldInvalidateLayoutForBoundsChange:newBounds];
    _invalidateAll = YES;
    return YES;
}

- (void)invalidateLayoutWithContext:(NSCollectionViewLayoutInvalidationContext *)context
{
    NSCollectionViewFlowLayoutInvalidationContext *flowLayoutContext = (NSCollectionViewFlowLayoutInvalidationContext *)context;
    if (flowLayoutContext && _invalidateAll) {
        flowLayoutContext.invalidateFlowLayoutAttributes = YES;
        flowLayoutContext.invalidateFlowLayoutDelegateMetrics = YES;
        _invalidateAll = NO;
    }

    [super invalidateLayoutWithContext:context];
}

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
        NSCollectionViewLayoutAttributes * const attributes = layoutAttributesArray[i];
        NSString * const elementKind = attributes.representedElementKind;

        if (@available(macOS 10.12, *)) {
            if (([elementKind isEqualToString:NSCollectionElementKindSectionHeader] && self.sectionHeadersPinToVisibleBounds) ||
                ([elementKind isEqualToString:NSCollectionElementKindSectionFooter] && self.sectionFootersPinToVisibleBounds)) {
                continue;
            }
        }

        [attributes setFrame:[self frameForDisplacedAttributes:attributes]];
        layoutAttributesArray[i] = attributes;
    }

    const id<NSCollectionViewDataSource> dataSource = self.collectionView.dataSource;

    if ([dataSource isKindOfClass:VLCLibraryAudioDataSource.class]) {
        VLCLibraryAudioDataSource * const audioDataSource = (VLCLibraryAudioDataSource *)dataSource;
        // Add detail view to the attributes set -- detail view about to be shown
        switch(audioDataSource.audioLibrarySegment) {
            case VLCAudioLibraryArtistsSegment:
            case VLCAudioLibraryGenresSegment:
                [layoutAttributesArray addObject:[self layoutAttributesForSupplementaryViewOfKind:VLCLibraryCollectionViewAudioGroupSupplementaryDetailViewKind atIndexPath:self.selectedIndexPath]];
                break;
            case VLCAudioLibraryAlbumsSegment:
                [layoutAttributesArray addObject:[self layoutAttributesForSupplementaryViewOfKind:VLCLibraryCollectionViewAlbumSupplementaryDetailViewKind atIndexPath:self.selectedIndexPath]];
                break;
            case VLCAudioLibrarySongsSegment:
            default:
                [layoutAttributesArray addObject:[self layoutAttributesForSupplementaryViewOfKind:VLCLibraryCollectionViewMediaItemSupplementaryDetailViewKind atIndexPath:self.selectedIndexPath]];
                break;
        }
    } else if ([dataSource isKindOfClass:VLCLibraryAudioGroupDataSource.class]) {
        [layoutAttributesArray addObject:[self layoutAttributesForSupplementaryViewOfKind:VLCLibraryCollectionViewAlbumSupplementaryDetailViewKind atIndexPath:self.selectedIndexPath]];
    } else if ([dataSource isKindOfClass:VLCLibraryHomeViewVideoContainerViewDataSource.class] ||
               [dataSource isKindOfClass:VLCLibraryVideoDataSource.class]) {
        [layoutAttributesArray addObject:[self layoutAttributesForSupplementaryViewOfKind:VLCLibraryCollectionViewMediaItemSupplementaryDetailViewKind atIndexPath:self.selectedIndexPath]];
    }

    return layoutAttributesArray;
}

- (NSCollectionViewLayoutAttributes *)layoutAttributesForSupplementaryViewOfKind:(NSCollectionViewSupplementaryElementKind)elementKind
                                                                     atIndexPath:(NSIndexPath *)indexPath
{
    BOOL isLibrarySupplementaryView = NO;

    if ([elementKind isEqualToString:VLCLibraryCollectionViewAudioGroupSupplementaryDetailViewKind]) {

        isLibrarySupplementaryView = YES;
        _animationType = VLCExpandAnimationTypeVerticalLarge;

    } else if ([elementKind isEqualToString:VLCLibraryCollectionViewAlbumSupplementaryDetailViewKind] ||
               [elementKind isEqualToString:VLCLibraryCollectionViewMediaItemSupplementaryDetailViewKind]) {

        isLibrarySupplementaryView = YES;
        _animationType = self.scrollDirection == NSCollectionViewScrollDirectionVertical ? VLCExpandAnimationTypeVerticalMedium : VLCExpandAnimationTypeHorizontalMedium;
    }

    if(isLibrarySupplementaryView) {
        NSCollectionViewLayoutAttributes *detailViewAttributes = [NSCollectionViewLayoutAttributes layoutAttributesForSupplementaryViewOfKind:elementKind
                                                                                                                                withIndexPath:indexPath];
        NSAssert1(detailViewAttributes != NULL,
                  @"Failed to create NSCollectionViewLayoutAttributes for view of kind %@.",
                  elementKind);

        const NSRect selectedItemFrame = [[self layoutAttributesForItemAtIndexPath:_selectedIndexPath] frame];

        if (self.scrollDirection == NSCollectionViewScrollDirectionVertical) {
            const float selectedItemFrameMaxY = _selectedIndexPath == nil ? 0 : NSMaxY(selectedItemFrame);
            detailViewAttributes.frame = NSMakeRect(NSMinX(self.collectionView.frame) + self.minimumInteritemSpacing,
                                                    selectedItemFrameMaxY + VLCLibraryUIUnits.mediumSpacing,
                                                    self.collectionViewContentSize.width - (self.minimumInteritemSpacing * 2),
                                                    [self currentAnimationStep]);

        } else if (self.scrollDirection == NSCollectionViewScrollDirectionHorizontal) {
            const float selectedItemFrameMinY = _selectedIndexPath == nil ? 0 : NSMinY(selectedItemFrame);
            const float selectedItemFrameMaxX = _selectedIndexPath == nil ? 0 : NSMaxX(selectedItemFrame);
            const float selectedItemFrameHeight = _selectedIndexPath == nil ? 0 : selectedItemFrame.size.height;
            detailViewAttributes.frame = NSMakeRect(selectedItemFrameMaxX + self.minimumInteritemSpacing,
                                                    selectedItemFrameMinY,
                                                    [self currentAnimationStep],
                                                    selectedItemFrameHeight);
        }

        return detailViewAttributes;
    }

    // Default attributes
    NSCollectionViewLayoutAttributes *attributes = [super layoutAttributesForSupplementaryViewOfKind:elementKind
                                                                                         atIndexPath:indexPath];
    [attributes setFrame:[self frameForDisplacedAttributes:attributes]];
    return attributes;
}

- (NSSet<NSIndexPath *> *)indexPathsToDeleteForSupplementaryViewOfKind:(NSString *)elementKind
{
    if ([elementKind isEqualToString:VLCLibraryCollectionViewAudioGroupSupplementaryDetailViewKind] ||
        [elementKind isEqualToString:VLCLibraryCollectionViewAlbumSupplementaryDetailViewKind] ||
        [elementKind isEqualToString:VLCLibraryCollectionViewMediaItemSupplementaryDetailViewKind]) {

        return [self.collectionView indexPathsForVisibleSupplementaryElementsOfKind:elementKind];
    }
    return [NSSet set];
}

# pragma mark - Calculation of displaced frame attributes

- (NSRect)frameForDisplacedAttributes:(NSCollectionViewLayoutAttributes *)inAttributes
{
    if(inAttributes == nil) {
        return NSZeroRect;
    }

    NSRect attributesFrame = inAttributes.frame;

    if (self.selectedIndexPath) {
        NSCollectionViewLayoutAttributes *selectedItemLayoutAttributes = [self layoutAttributesForItemAtIndexPath:_selectedIndexPath];

        if(selectedItemLayoutAttributes == nil) {
            return attributesFrame;
        }

        NSRect selectedItemFrame = selectedItemLayoutAttributes.frame;
        if (self.scrollDirection == NSCollectionViewScrollDirectionVertical &&
            NSMinY(attributesFrame) > (NSMaxY(selectedItemFrame))) {
        } else if (self.scrollDirection == NSCollectionViewScrollDirectionHorizontal &&
                   NSMinX(attributesFrame) > (NSMaxX(selectedItemFrame))) {

            attributesFrame.origin.y += [self currentAnimationStep] + VLCLibraryUIUnits.mediumSpacing;
            attributesFrame.origin.x += [self currentAnimationStep] + VLCLibraryUIUnits.mediumSpacing;
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
    } else {
        _animationIsCollapse = YES;
        _animationIndex = kAnimationSteps;
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
        } else {
            bridgedSelf.animationIndex = kAnimationSteps - 1;
        }
    }

    dispatch_async(dispatch_get_main_queue(), ^(void){
        [bridgedSelf invalidateLayout];
    });

    return kCVReturnSuccess;
}
