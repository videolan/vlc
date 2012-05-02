/*****************************************************************************
 * VLCMedia.h: VLCKit.framework VLCMedia header
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

#import <Foundation/Foundation.h>
#import "VLCMediaList.h"
#import "VLCTime.h"

/* Meta Dictionary Keys */
/**
 * Standard dictionary keys for retreiving meta data.
 */
extern NSString * VLCMetaInformationTitle;          /* NSString */
extern NSString * VLCMetaInformationArtist;         /* NSString */
extern NSString * VLCMetaInformationGenre;          /* NSString */
extern NSString * VLCMetaInformationCopyright;      /* NSString */
extern NSString * VLCMetaInformationAlbum;          /* NSString */
extern NSString * VLCMetaInformationTrackNumber;    /* NSString */
extern NSString * VLCMetaInformationDescription;    /* NSString */
extern NSString * VLCMetaInformationRating;         /* NSString */
extern NSString * VLCMetaInformationDate;           /* NSString */
extern NSString * VLCMetaInformationSetting;        /* NSString */
extern NSString * VLCMetaInformationURL;            /* NSString */
extern NSString * VLCMetaInformationLanguage;       /* NSString */
extern NSString * VLCMetaInformationNowPlaying;     /* NSString */
extern NSString * VLCMetaInformationPublisher;      /* NSString */
extern NSString * VLCMetaInformationEncodedBy;      /* NSString */
extern NSString * VLCMetaInformationArtworkURL;     /* NSString */
extern NSString * VLCMetaInformationArtwork;        /* NSImage  */
extern NSString * VLCMetaInformationTrackID;        /* NSString */

/* Notification Messages */
/**
 * Available notification messages.
 */
extern NSString * VLCMediaMetaChanged;  //< Notification message for when the media's meta data has changed

// Forward declarations, supresses compiler error messages
@class VLCMediaList;
@class VLCMedia;

typedef enum VLCMediaState
{
    VLCMediaStateNothingSpecial,        //< Nothing
    VLCMediaStateBuffering,             //< Stream is buffering
    VLCMediaStatePlaying,               //< Stream is playing
    VLCMediaStateError,                 //< Can't be played because an error occurred
} VLCMediaState;

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

/**
 * Delegate method called whenever the media was parsed.
 * \param aMedia The media resource whose meta data has been changed.
 */

- (void)mediaDidFinishParsing:(VLCMedia *)aMedia;
@end

/**
 * Defines files and streams as a managed object.  Each media object can be
 * administered seperately.  VLCMediaPlayer or VLCMediaList must be used
 * to execute the appropriate playback functions.
 * \see VLCMediaPlayer
 * \see VLCMediaList
 */
@interface VLCMedia : NSObject
{
    void *                p_md;              //< Internal media descriptor instance
    NSURL *               url;               //< URL (MRL) for this media resource
    VLCMediaList *        subitems;          //< Sub list of items
    VLCTime *             length;            //< Cached duration of the media
    NSMutableDictionary * metaDictionary;    //< Meta data storage
    id                    delegate;          //< Delegate object
    BOOL                  isArtFetched;      //< Value used to determine of the artwork has been parsed
    BOOL                  areOthersMetaFetched; //< Value used to determine of the other meta has been parsed
    BOOL                  isArtURLFetched;   //< Value used to determine of the other meta has been preparsed
    VLCMediaState         state;             //< Current state of the media
    BOOL                  isParsed;
}

/* Factories */
/**
 * Manufactures a new VLCMedia object using the URL specified.
 * \param anURL URL to media to be accessed.
 * \return A new VLCMedia object, only if there were no errors.  This object will be automatically released.
 * \see initWithMediaURL
 */
+ (id)mediaWithURL:(NSURL *)anURL;

/**
 * Manufactures a new VLCMedia object using the path specified.
 * \param aPath Path to the media to be accessed.
 * \return A new VLCMedia object, only if there were no errors.  This object will be automatically released.
 * \see initWithPath
 */
+ (id)mediaWithPath:(NSString *)aPath;

/**
 * TODO
 * \param aName TODO
 * \return a new VLCMedia object, only if there were no errors.  This object
 * will be automatically released.
 * \see initAsNodeWithName
 */
+ (id)mediaAsNodeWithName:(NSString *)aName;

/* Initializers */
/**
 * Initializes a new VLCMedia object to use the specified URL.
 * \param aPath Path to media to be accessed.
 * \return A new VLCMedia object, only if there were no errors.
 */
- (id)initWithURL:(NSURL *)anURL;

/**
 * Initializes a new VLCMedia object to use the specified path.
 * \param aPath Path to media to be accessed.
 * \return A new VLCMedia object, only if there were no errors.
 */
- (id)initWithPath:(NSString *)aPath;

/**
 * TODO
 * \param aName TODO
 * \return A new VLCMedia object, only if there were no errors.
 */
- (id)initAsNodeWithName:(NSString *)aName;

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
 * Receiver's delegate.
 */
@property (assign) id delegate;

/**
 * A VLCTime object describing the length of the media resource, only if it is
 * available.  Use lengthWaitUntilDate: to wait for a specified length of time.
 * \see lengthWaitUntilDate
 */
@property (retain, readonly) VLCTime * length;

/**
 * Returns a VLCTime object describing the length of the media resource,
 * however, this is a blocking operation and will wait until the preparsing is
 * completed before returning anything.
 * \param aDate Time for operation to wait until, if there are no results
 * before specified date then nil is returned.
 * \return The length of the media resource, nil if it couldn't wait for it.
 */
- (VLCTime *)lengthWaitUntilDate:(NSDate *)aDate;

/**
 * Determines if the media has already been preparsed.
 */
@property (readonly) BOOL isParsed;

/**
 * The URL for the receiver's media resource.
 */
@property (retain, readonly) NSURL * url;

/**
 * The receiver's sub list.
 */
@property (retain, readonly) VLCMediaList * subitems;

/**
 * The receiver's meta data as a NSDictionary object.
 */
@property (retain, readonly) NSDictionary * metaDictionary;

/**
 * The receiver's state, such as Playing, Error, NothingSpecial, Buffering.
 */
@property (readonly) VLCMediaState state;

/**
 * Tracks information NSDictionary Possible Keys
 */

/**
 * \returns a NSNumber
 */
extern NSString *VLCMediaTracksInformationCodec;

/**
 * \returns a NSNumber
 */
extern NSString *VLCMediaTracksInformationId;
/**
 * \returns a NSString
 * \see VLCMediaTracksInformationTypeAudio
 * \see VLCMediaTracksInformationTypeVideo
 * \see VLCMediaTracksInformationTypeText
 * \see VLCMediaTracksInformationTypeUnknown
 */
extern NSString *VLCMediaTracksInformationType;

/**
 * \returns a NSNumber
 */
extern NSString *VLCMediaTracksInformationCodecProfile;
/**
 * \returns a NSNumber
 */
extern NSString *VLCMediaTracksInformationCodecLevel;

/**
 * \returns the audio channels number as NSNumber
 */
extern NSString *VLCMediaTracksInformationAudioChannelsNumber;
/**
 * \returns the audio rate as NSNumber
 */
extern NSString *VLCMediaTracksInformationAudioRate;

/**
 * \returns the height as NSNumber
 */
extern NSString *VLCMediaTracksInformationVideoHeight;
/**
 * \returns the width as NSNumber
 */
extern NSString *VLCMediaTracksInformationVideoWidth;

/**
 * Tracks information NSDictionary values for
 * VLCMediaTracksInformationType
 */
extern NSString *VLCMediaTracksInformationTypeAudio;
extern NSString *VLCMediaTracksInformationTypeVideo;
extern NSString *VLCMediaTracksInformationTypeText;
extern NSString *VLCMediaTracksInformationTypeUnknown;


/**
 * Returns the tracks information.
 *
 * This is an array of NSDictionary representing each track.
 * It can contains the following keys:
 *
 * \see VLCMediaTracksInformationCodec
 * \see VLCMediaTracksInformationId
 * \see VLCMediaTracksInformationType
 *
 * \see VLCMediaTracksInformationCodecProfile
 * \see VLCMediaTracksInformationCodecLevel
 *
 * \see VLCMediaTracksInformationAudioChannelsNumber
 * \see VLCMediaTracksInformationAudioRate
 *
 * \see VLCMediaTracksInformationVideoHeight
 * \see VLCMediaTracksInformationVideoWidth
 */

- (NSArray *)tracksInformation;

/**
 * Start asynchronously to parse the media.
 * This will attempt to fetch the meta data and tracks information.
 *
 * This is automatically done when an accessor requiring parsing
 * is called.
 *
 * \see -[VLCMediaDelegate mediaDidFinishParsing:]
 */
- (void)parse;

/**
 * Add options to the media, that will be used to determine how
 * VLCMediaPlayer will read the media. This allow to use VLC advanced
 * reading/streaming options in a per-media basis
 *
 * The options are detailed in vlc --long-help, for instance "--sout-all"
 * And on the web: http://wiki.videolan.org/VLC_command-line_help
*/
- (void) addOptions:(NSDictionary*) options;

/**
 * Getter for statistics information
 * Returns a NSDictionary with NSNumbers for values.
 *
 */
- (NSDictionary*) stats;

@end
