/*****************************************************************************
 * VLCLibraryNavigationStack.m: MacOS X interface module
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

#import "VLCLibraryNavigationStack.h"

#import "VLCLibraryWindow.h"
#import "VLCLibraryNavigationState.h"
#import "VLCInputItem.h"

#import "library/audio-library/VLCLibraryAudioViewController.h"

#import "media-source/VLCMediaSourceBaseDataSource.h"
#import "media-source/VLCMediaSourceDataSource.h"
#import "media-source/VLCMediaSource.h"

@interface VLCLibraryNavigationCurrentStackPosition : NSObject

@property (readonly) NSUInteger navigationStackIndex;
@property (readonly) VLCLibraryNavigationState *navigationState;

- (instancetype)initWithStackIndex:(NSUInteger)index andState:(VLCLibraryNavigationState *)state;

@end

@implementation VLCLibraryNavigationCurrentStackPosition

- (instancetype)initWithStackIndex:(NSUInteger)index andState:(VLCLibraryNavigationState *)state
{
    self = [super init];

    if(self) {
        _navigationStackIndex = index;
        _navigationState = state;
    }

    return self;
}

@end


@interface VLCLibraryNavigationStack ()
{
    NSMutableArray<VLCLibraryNavigationState *> *_navigationStates;
    VLCLibraryNavigationCurrentStackPosition *_currentPosition;
}

@end

@implementation VLCLibraryNavigationStack

- (instancetype)init
{
    self = [super init];
    if (self) {
        _navigationStates = [[NSMutableArray alloc] init];
    }
    return self;
}

- (void)setDelegate:(VLCLibraryWindow *)delegate
{
    _delegate = delegate;
    [self updateDelegateNavigationButtons];
}

- (BOOL)forwardsAvailable
{
    NSUInteger numNavigationStates = _navigationStates.count;

    return _currentPosition != nil && 
        _currentPosition.navigationStackIndex < numNavigationStates - 1 && 
        numNavigationStates > 1;
}

- (BOOL)backwardsAvailable
{
    return _currentPosition != nil && 
        _currentPosition.navigationStackIndex > 0 && 
        _navigationStates.count > 1;
}

- (void)forwards
{
    if(_delegate == nil || !self.forwardsAvailable) {
        return;
    }

    NSUInteger newPositionIndex = _currentPosition.navigationStackIndex + 1;
    _currentPosition = [[VLCLibraryNavigationCurrentStackPosition alloc] initWithStackIndex:newPositionIndex andState:_navigationStates[newPositionIndex]];
    [self setDelegateToState:_currentPosition.navigationState];
}

- (void)backwards
{
    if(_delegate == nil || !self.backwardsAvailable) {
        return;
    }

    NSUInteger newPositionIndex = _currentPosition.navigationStackIndex - 1;
    _currentPosition = [[VLCLibraryNavigationCurrentStackPosition alloc] initWithStackIndex:newPositionIndex andState:_navigationStates[newPositionIndex]];
    [self setDelegateToState:_currentPosition.navigationState];
}

- (void)appendCurrentLibraryState
{
    if(_delegate == nil) {
        return;
    }

    if(self.forwardsAvailable) {
        NSUInteger firstIndexToRemove = _currentPosition.navigationStackIndex + 1;
        // -1 to account for the array count
        NSRange rangeToRemove = NSMakeRange(firstIndexToRemove, (_navigationStates.count - 1) - _currentPosition.navigationStackIndex);
        [self removeAndCleanUpStatesInRange:rangeToRemove];
        [_navigationStates removeObjectsInRange:rangeToRemove];
    }

    VLCLibraryNavigationState *navigationState = [[VLCLibraryNavigationState alloc] initFromLibraryWindow:_delegate];
    _currentPosition = [[VLCLibraryNavigationCurrentStackPosition alloc] initWithStackIndex:_navigationStates.count andState:navigationState];
    [_navigationStates addObject:navigationState];

    [self updateDelegateNavigationButtons];
}

- (void)removeAndCleanUpStatesInRange:(NSRange)range
{
    NSAssert(range.location + range.length - 1 < _navigationStates.count, @"Invalid range for state removal and cleanup, out of bounds.");
    
    for (NSUInteger i = range.location; i < range.length; ++i) {
        VLCLibraryNavigationState *state = [_navigationStates objectAtIndex:i];
        VLCInputNode *stateNode = state.currentNodeDisplayed;
        
        if (stateNode) {
            [state.currentMediaSource.displayedMediaSource clearChildNodesForNode:stateNode.vlcInputItemNode];
        }
        
        [_navigationStates removeObjectAtIndex:i];
    }
}

- (void)updateDelegateNavigationButtons
{
    if(_delegate == nil) {
        return;
    }

    _delegate.forwardsNavigationButton.enabled = self.forwardsAvailable;
    _delegate.backwardsNavigationButton.enabled = self.backwardsAvailable;
}

- (void)setDelegateToState:(VLCLibraryNavigationState *)state
{
    if(_delegate == nil) {
        return;
    }

    [_delegate.segmentedTitleControl setSelectedSegment:state.libraryWindowSelectedSegment];
    [_delegate.audioSegmentedControl setSelectedSegment:state.audioLibraryViewSelectedSegment];
    [_delegate.gridVsListSegmentedControl setSelectedSegment:state.viewModeSelectedSegment];
    [_delegate.mediaSourceDataSource setChildDataSource:state.currentMediaSource];
    [_delegate.mediaSourceDataSource.childDataSource setNodeToDisplay:state.currentNodeDisplayed];

    [_delegate segmentedControlAction:self];
    [_delegate.libraryAudioViewController segmentedControlAction:self];
    [_delegate.mediaSourceDataSource setGridOrListMode:self];

    [self updateDelegateNavigationButtons];
}

@end
