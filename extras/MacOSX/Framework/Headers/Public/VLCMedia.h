/*****************************************************************************
 * VLCMedia.h: VLC.framework VLCMedia header
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

#import <Cocoa/Cocoa.h>
#import "VLCMediaList.h"
#import "VLCTime.h"

/* Meta Dictionary Keys */
/**
 * Standard dictionary keys for retreiving meta data.
 */
extern NSString *VLCMetaInformationTitle;        /* NSString */
extern NSString *VLCMetaInformationArtist;        /* NSString */
extern NSString *VLCMetaInformationTitle;       /* NSString */
extern NSString *VLCMetaInformationArtist;        /* NSString */
extern NSString *VLCMetaInformationGenre;        /* NSString */
extern NSString *VLCMetaInformationCopyright;    /* NSString */
extern NSString *VLCMetaInformationAlbum;        /* NSString */
extern NSString *VLCMetaInformationTrackNumber;    /* NSString */
extern NSString *VLCMetaInformationDescription;    /* NSString */
extern NSString *VLCMetaInformationRating;        /* NSString */
extern NSString *VLCMetaInformationDate;        /* NSString */
extern NSString *VLCMetaInformationSetting;        /* NSString */
extern NSString *VLCMetaInformationURL;            /* NSString */
extern NSString *VLCMetaInformationLanguage;    /* NSString */
extern NSString *VLCMetaInformationNowPlaying;    /* NSString */
extern NSString *VLCMetaInformationPublisher;    /* NSString */
extern NSString *VLCMetaInformationEncodedBy;    /* NSString */
extern NSString *VLCMetaInformationArtworkURL;    /* NSString */
extern NSString *VLCMetaInformationArtwork;     /* NSImage  */
extern NSString *VLCMetaInformationTrackID;        /* NSString */

/* Notification Messages */
/**
 * Available notification messages.
 */
extern NSString *VLCMediaMetaChanged;        //< Notification message for when the media's meta data has changed

// Forward declarations, supresses compiler error messages
@class VLCMediaList;
@class VLCMedia;

/**
 * Informal protocol declaration for VLCMedia delegates.  Allows data changes to be
 * trapped.
 */
@protocol VLCMediaDelegate
// TODO: SubItemAdded/SubItemRemoved implementation.  Not sure if we really want to implement this.
///**
// * Delegate method called whenever a sub item has been added to the specified VLCMedia.
// * \param aMedia The media resource that has received the new sub item.
// * \param childMedia The new sub item added.
// * \param index Location of the new subitem in the aMedia's sublist.
// */
// - (void)media:(VLCMedia *)media addedSubItem:(VLCMedia *)childMedia atIndex:(int)index;

///**
// * Delegate method called whenever a sub item has been removed from the specified VLCMedia.
// * \param aMedia The media resource that has had a sub item removed from.
// * \param childMedia The sub item removed.
// * \param index The previous location of the recently removed sub item.
// */
// - (void)media:(VLCMedia *)aMedia removedSubItem:(VLCMedia *)childMedia atIndex:(int)index;

/**
 * Delegate method called whenever the meta has changed for the receiver.
 * \param aMedia The media resource whose meta data has been changed.
 * \param oldValue The old meta data value.
 * \param key The key of the value that was changed.
 */
- (void)media:(VLCMedia *)aMedia metaValueChangedFrom:(id)oldValue forKey:(NSString *)key;
@end

/**
 * Defines files and streams as a managed object.  Each media object can be 
 * administered seperately.  VLCMediaPlayer or VLCMediaControl must be used 
 * to execute the appropriate playback functions.
 * \see VLCMediaPlayer
 * \see VLCMediaControl
 */
@interface VLCMedia : NSObject
{
    void *                p_md;              //< Internal media descriptor instance
    NSString *            url;               //< URL for this media resource
    VLCMediaList *        subitems;          //< Sub list of items
    VLCTime *             length;            //< Duration of the media
    NSMutableDictionary * metaDictionary;    //< Meta data storage
    id                    delegate;          //< Delegate object
    BOOL                  preparsed;         //< Value used to determine of the file has been preparsed
}

/* Object Factories */
/**
 * Manufactures a new VLCMedia object using the URL specified.  Will return nil if
 * the specified URL references a directory that does not comply with DVD file 
 * structure.
 * \param anURL URL to media to be accessed.
 * \return A new VLCMedia object, only if there were no errors.  This object 
 * will be automatically released.
 * \see initWithMediaURL
 */
+ (id)mediaWithURL:(NSString *)anURL;

/* Initializers */
/**
 * Initializes a new VLCMedia object to use the specified URL.  Will return nil if
 * the specified URL references a directory that does not comply with DVD file
 * structure.
 * \param anURL URL to media to be accessed.
 * \return A new VLCMedia object, only if there were no errors.
 */
- (id)initWithURL:(NSString *)anURL;

/**
 * Returns an NSComparisonResult value that indicates the lexical ordering of 
 * the receiver and a given meda.
 * \param media The media with which to compare with the receiver.
 * \return NSOrderedAscending if the URL of the receiver precedes media in 
 * lexical ordering, NSOrderedSame if the URL of the receiver and media are 
 * equivalent in lexical value, and NSOrderedDescending if the URL of the 
 * receiver follows media. If media is nil, returns NSOrderedDescending.
 */
- (NSComparisonResult)compare:(VLCMedia *)media;

/* Properties */
/**
 * Sets the receiver's delegate.
 * \param delegate The delegate for the receiver.
 */
- (void)setDelegate:(id)delegate;

/**
 * Returns the receiver's delegate
 * \return The receiver's delegate
 */
- (id)delegate;

/**
 * Returns a VLCTime object describing the length of the media resource.
 * \return The length of the media resource.
 */
- (VLCTime *)length;

/**
 * Returns a VLCTime object describing the length of the media resource,
 * however, this is a blocking operation and will wait until the preparsing is
 * completed before returning anything.
 * \param aDate Time for operation to wait until, if there are no results
 * before specified date then nil is returned.
 * \return The length of the media resource, nil if it couldn't wait for it.
 */
- (VLCTime *)lengthWaitUntilDate:(NSDate *)aDate;

- (BOOL)isPreparsed;

/**
 * Returns the URL for the receiver's media resource.
 * \return The URL for the receiver's media resource.
 */
- (NSString *)url;

/**
 * Returns the receiver's sub list.
 * \return The receiver's sub list.
 */
- (VLCMediaList *)subitems;

/**
 * Returns the receiver's meta data as a NSDictionary object.
 * \return The receiver's meta data as a NSDictionary object.
 */
- (NSDictionary *)metaDictionary;
@end