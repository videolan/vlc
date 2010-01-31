/*****************************************************************************
 * VLCMedia.m: VLCKit.framework VLCMedia implementation
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

#import "VLCMedia.h"
#import "VLCMediaList.h"
#import "VLCEventManager.h"
#import "VLCLibrary.h"
#import "VLCLibVLCBridging.h"
#include <vlc/libvlc.h>

/* Meta Dictionary Keys */
NSString * VLCMetaInformationTitle          = @"title";
NSString * VLCMetaInformationArtist         = @"artist";
NSString * VLCMetaInformationGenre          = @"genre";
NSString * VLCMetaInformationCopyright      = @"copyright";
NSString * VLCMetaInformationAlbum          = @"album";
NSString * VLCMetaInformationTrackNumber    = @"trackNumber";
NSString * VLCMetaInformationDescription    = @"description";
NSString * VLCMetaInformationRating         = @"rating";
NSString * VLCMetaInformationDate           = @"date";
NSString * VLCMetaInformationSetting        = @"setting";
NSString * VLCMetaInformationURL            = @"url";
NSString * VLCMetaInformationLanguage       = @"language";
NSString * VLCMetaInformationNowPlaying     = @"nowPlaying";
NSString * VLCMetaInformationPublisher      = @"publisher";
NSString * VLCMetaInformationEncodedBy      = @"encodedBy";
NSString * VLCMetaInformationArtworkURL     = @"artworkURL";
NSString * VLCMetaInformationArtwork        = @"artwork";
NSString * VLCMetaInformationTrackID        = @"trackID";

/* Notification Messages */
NSString * VLCMediaMetaChanged              = @"VLCMediaMetaChanged";

/******************************************************************************
 * @property (readwrite)
 */
@interface VLCMedia ()
@property (readwrite) VLCMediaState state;
@end

/******************************************************************************
 * Interface (Private)
 */
// TODO: Documentation
@interface VLCMedia (Private)
/* Statics */
+ (libvlc_meta_t)stringToMetaType:(NSString *)string;
+ (NSString *)metaTypeToString:(libvlc_meta_t)type;

/* Initializers */
- (void)initInternalMediaDescriptor;

/* Operations */
- (void)fetchMetaInformationFromLibVLCWithType:(NSString*)metaType;
- (void)fetchMetaInformationForArtWorkWithURL:(NSString *)anURL;
- (void)setArtwork:(NSImage *)art;

/* Callback Methods */
- (void)metaChanged:(NSString *)metaType;
- (void)subItemAdded;
- (void)setStateAsNumber:(NSNumber *)newStateAsNumber;
@end

static VLCMediaState libvlc_state_to_media_state[] =
{
    [libvlc_NothingSpecial] = VLCMediaStateNothingSpecial,
    [libvlc_Stopped]        = VLCMediaStateNothingSpecial,
    [libvlc_Opening]        = VLCMediaStateNothingSpecial,
    [libvlc_Buffering]      = VLCMediaStateBuffering,
    [libvlc_Ended]          = VLCMediaStateNothingSpecial,
    [libvlc_Error]          = VLCMediaStateError,
    [libvlc_Playing]        = VLCMediaStatePlaying,
    [libvlc_Paused]         = VLCMediaStatePlaying,
};

static inline VLCMediaState LibVLCStateToMediaState( libvlc_state_t state )
{
    return libvlc_state_to_media_state[state];
}

/******************************************************************************
 * LibVLC Event Callback
 */
static void HandleMediaMetaChanged(const libvlc_event_t * event, void * self)
{
    if( event->u.media_meta_changed.meta_type == libvlc_meta_Publisher ||
        event->u.media_meta_changed.meta_type == libvlc_meta_NowPlaying )
    {
        /* Skip those meta. We don't really care about them for now.
         * And they occure a lot */
        return;
    }
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
    [[VLCEventManager sharedManager] callOnMainThreadObject:self
                                                 withMethod:@selector(metaChanged:)
                                       withArgumentAsObject:[VLCMedia metaTypeToString:event->u.media_meta_changed.meta_type]];
    [pool release];
}

static void HandleMediaDurationChanged(const libvlc_event_t * event, void * self)
{
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];

    [[VLCEventManager sharedManager] callOnMainThreadObject:self
                                                 withMethod:@selector(setLength:)
                                       withArgumentAsObject:[VLCTime timeWithNumber:
                                           [NSNumber numberWithLongLong:event->u.media_duration_changed.new_duration]]];
    [pool release];
}

static void HandleMediaStateChanged(const libvlc_event_t * event, void * self)
{
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];

    [[VLCEventManager sharedManager] callOnMainThreadObject:self
                                                 withMethod:@selector(setStateAsNumber:)
                                       withArgumentAsObject:[NSNumber numberWithInt:
                                            LibVLCStateToMediaState(event->u.media_state_changed.new_state)]];
    [pool release];
}

static void HandleMediaSubItemAdded(const libvlc_event_t * event, void * self)
{
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
    [[VLCEventManager sharedManager] callOnMainThreadObject:self
                                                 withMethod:@selector(subItemAdded)
                                       withArgumentAsObject:nil];
    [pool release];
}

/******************************************************************************
 * Implementation
 */
@implementation VLCMedia
+ (id)mediaWithURL:(NSURL *)anURL;
{
    return [[[VLCMedia alloc] initWithURL:anURL] autorelease];
}

+ (id)mediaWithPath:(NSString *)aPath;
{
    return [[[VLCMedia alloc] initWithPath:aPath] autorelease];
}

+ (id)mediaAsNodeWithName:(NSString *)aName;
{
    return [[[VLCMedia alloc] initAsNodeWithName:aName] autorelease];
}

- (id)initWithPath:(NSString *)aPath
{
    return [self initWithURL:[NSURL fileURLWithPath:aPath isDirectory:NO]];
}

- (id)initWithURL:(NSURL *)anURL
{
    if (self = [super init])
    {
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        p_md = libvlc_media_new([VLCLibrary sharedInstance],
                                           [[anURL absoluteString] UTF8String],
                                           &ex);
        catch_exception(&ex);

        delegate = nil;
        metaDictionary = [[NSMutableDictionary alloc] initWithCapacity:3];

        // This value is set whenever the demuxer figures out what the length is.
        // TODO: Easy way to tell the length of the movie without having to instiate the demuxer.  Maybe cached info?
        length = nil;

        [self initInternalMediaDescriptor];
    }
    return self;
}

- (id)initAsNodeWithName:(NSString *)aName
{
    if (self = [super init])
    {
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);

        p_md = libvlc_media_new_as_node([VLCLibrary sharedInstance],
                                                   [aName UTF8String],
                                                   &ex);
        catch_exception(&ex);

        delegate = nil;
        metaDictionary = [[NSMutableDictionary alloc] initWithCapacity:3];

        // This value is set whenever the demuxer figures out what the length is.
        // TODO: Easy way to tell the length of the movie without having to instiate the demuxer.  Maybe cached info?
        length = nil;

        [self initInternalMediaDescriptor];
    }
    return self;
}

- (void)release
{
    @synchronized(self)
    {
        if([self retainCount] <= 1)
        {
            /* We must make sure we won't receive new event after an upcoming dealloc
             * We also may receive a -retain in some event callback that may occcur
             * Before libvlc_event_detach. So this can't happen in dealloc */
            libvlc_event_manager_t * p_em = libvlc_media_event_manager(p_md);
            libvlc_event_detach(p_em, libvlc_MediaMetaChanged,     HandleMediaMetaChanged,     self);
            libvlc_event_detach(p_em, libvlc_MediaDurationChanged, HandleMediaDurationChanged, self);
            libvlc_event_detach(p_em, libvlc_MediaStateChanged,    HandleMediaStateChanged,    self);
            libvlc_event_detach(p_em, libvlc_MediaSubItemAdded,    HandleMediaSubItemAdded,    self);
        }
        [super release];
    }
}

- (void)dealloc
{
    // Testing to see if the pointer exists is not required, if the pointer is null
    // then the release message is not sent to it.
    delegate = nil;
    [length release];
    [url release];
    [subitems release];
    [metaDictionary release];

    libvlc_media_release( p_md );

    [super dealloc];
}

- (NSString *)description
{
    NSString * result = [metaDictionary objectForKey:VLCMetaInformationTitle];
    return [NSString stringWithFormat:@"<%@ %p> %@", [self className], self, (result ? result : [url absoluteString])];
}

- (NSComparisonResult)compare:(VLCMedia *)media
{
    if (self == media)
        return NSOrderedSame;
    if (!media)
        return NSOrderedDescending;
    return p_md == [media libVLCMediaDescriptor] ? NSOrderedSame : NSOrderedAscending;
}

@synthesize delegate;

- (VLCTime *)length
{
    if (!length)
    {
        // Try figuring out what the length is
        long long duration = libvlc_media_get_duration( p_md, NULL );
        if (duration > -1)
        {
            length = [[VLCTime timeWithNumber:[NSNumber numberWithLongLong:duration]] retain];
            return [[length retain] autorelease];
        }
        return [VLCTime nullTime];
    }
    return [[length retain] autorelease];
}

- (VLCTime *)lengthWaitUntilDate:(NSDate *)aDate
{
    static const long long thread_sleep = 10000;

    if (!length)
    {
        // Force preparsing of this item.
        [self length];

        // wait until we are preparsed
        while (!length && ![self isPreparsed] && [aDate timeIntervalSinceNow] > 0)
        {
            usleep( thread_sleep );
        }

        // So we're done waiting, but sometimes we trap the fact that the parsing
        // was done before the length gets assigned, so lets go ahead and assign
        // it ourselves.
        if (!length)
            return [self length];
    }

    return [[length retain] autorelease];
}

- (BOOL)isPreparsed
{
    return libvlc_media_is_preparsed( p_md );
}

@synthesize url;
@synthesize subitems;
@synthesize metaDictionary;
@synthesize state;

@end

/******************************************************************************
 * Implementation VLCMedia (LibVLCBridging)
 */
@implementation VLCMedia (LibVLCBridging)

+ (id)mediaWithLibVLCMediaDescriptor:(void *)md
{
    return [[[VLCMedia alloc] initWithLibVLCMediaDescriptor:md] autorelease];
}

- (id)initWithLibVLCMediaDescriptor:(void *)md
{
    if (self = [super init])
    {
        libvlc_media_retain( md );
        p_md = md;

        metaDictionary = [[NSMutableDictionary alloc] initWithCapacity:3];
        [self initInternalMediaDescriptor];
    }
    return self;
}

- (void *)libVLCMediaDescriptor
{
    return p_md;
}

+ (id)mediaWithMedia:(VLCMedia *)media andLibVLCOptions:(NSDictionary *)options
{
    libvlc_media_t * p_md;
    p_md = libvlc_media_duplicate( [media libVLCMediaDescriptor] );

    for( NSString * key in [options allKeys] )
    {
        libvlc_media_add_option(p_md, [[NSString stringWithFormat:@"%@=#%@", key, [options objectForKey:key]] UTF8String]);
    }
    return [VLCMedia mediaWithLibVLCMediaDescriptor:p_md];
}

@end

/******************************************************************************
 * Implementation VLCMedia (Private)
 */
@implementation VLCMedia (Private)

+ (libvlc_meta_t)stringToMetaType:(NSString *)string
{
    static NSDictionary * stringToMetaDictionary = nil;
    // TODO: Thread safe-ize
    if( !stringToMetaDictionary )
    {
#define VLCStringToMeta( name ) [NSNumber numberWithInt: libvlc_meta_##name], VLCMetaInformation##name
        stringToMetaDictionary =
            [[NSDictionary dictionaryWithObjectsAndKeys:
                VLCStringToMeta(Title),
                VLCStringToMeta(Artist),
                VLCStringToMeta(Genre),
                VLCStringToMeta(Copyright),
                VLCStringToMeta(Album),
                VLCStringToMeta(TrackNumber),
                VLCStringToMeta(Description),
                VLCStringToMeta(Rating),
                VLCStringToMeta(Date),
                VLCStringToMeta(Setting),
                VLCStringToMeta(URL),
                VLCStringToMeta(Language),
                VLCStringToMeta(NowPlaying),
                VLCStringToMeta(Publisher),
                VLCStringToMeta(ArtworkURL),
                VLCStringToMeta(TrackID),
                nil] retain];
#undef VLCStringToMeta
    }
    NSNumber * number = [stringToMetaDictionary objectForKey:string];
    return number ? [number intValue] : -1;
}

+ (NSString *)metaTypeToString:(libvlc_meta_t)type
{
#define VLCMetaToString( name, type )   if (libvlc_meta_##name == type) return VLCMetaInformation##name;
    VLCMetaToString(Title, type);
    VLCMetaToString(Artist, type);
    VLCMetaToString(Genre, type);
    VLCMetaToString(Copyright, type);
    VLCMetaToString(Album, type);
    VLCMetaToString(TrackNumber, type);
    VLCMetaToString(Description, type);
    VLCMetaToString(Rating, type);
    VLCMetaToString(Date, type);
    VLCMetaToString(Setting, type);
    VLCMetaToString(URL, type);
    VLCMetaToString(Language, type);
    VLCMetaToString(NowPlaying, type);
    VLCMetaToString(Publisher, type);
    VLCMetaToString(ArtworkURL, type);
    VLCMetaToString(TrackID, type);
#undef VLCMetaToString
    return nil;
}

- (void)initInternalMediaDescriptor
{
    char * p_url = libvlc_media_get_mrl( p_md );

    url = [[NSURL URLWithString:[NSString stringWithUTF8String:p_url]] retain];
    if( !url ) /* Attempt to interpret as a file path then */
        url = [[NSURL fileURLWithPath:[NSString stringWithUTF8String:p_url]] retain];
    free( p_url );

    libvlc_media_set_user_data( p_md, (void*)self );

    libvlc_event_manager_t * p_em = libvlc_media_event_manager( p_md );
    libvlc_event_attach(p_em, libvlc_MediaMetaChanged,     HandleMediaMetaChanged,     self);
    libvlc_event_attach(p_em, libvlc_MediaDurationChanged, HandleMediaDurationChanged, self);
    libvlc_event_attach(p_em, libvlc_MediaStateChanged,    HandleMediaStateChanged,    self);
    libvlc_event_attach(p_em, libvlc_MediaSubItemAdded,    HandleMediaSubItemAdded,    self);

    libvlc_media_list_t * p_mlist = libvlc_media_subitems( p_md );

    if (!p_mlist)
        subitems = nil;
    else
    {
        subitems = [[VLCMediaList mediaListWithLibVLCMediaList:p_mlist] retain];
        libvlc_media_list_release( p_mlist );
    }

    state = LibVLCStateToMediaState(libvlc_media_get_state( p_md ));
}

- (void)fetchMetaInformationFromLibVLCWithType:(NSString *)metaType
{
    char * psz_value = libvlc_media_get_meta( p_md, [VLCMedia stringToMetaType:metaType] );
    NSString * newValue = psz_value ? [NSString stringWithUTF8String: psz_value] : nil;
    NSString * oldValue = [metaDictionary valueForKey:metaType];
    free(psz_value);

    if ( newValue != oldValue && !(oldValue && newValue && [oldValue compare:newValue] == NSOrderedSame) )
    {
        // Only fetch the art if needed. (ie, create the NSImage, if it was requested before)
        if (isArtFetched && [metaType isEqualToString:VLCMetaInformationArtworkURL])
        {
            [NSThread detachNewThreadSelector:@selector(fetchMetaInformationForArtWorkWithURL:)
                                         toTarget:self
                                       withObject:newValue];
        }

        [metaDictionary setValue:newValue forKeyPath:metaType];
    }
}

- (void)fetchMetaInformationForArtWorkWithURL:(NSString *)anURL
{
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
    NSImage * art = nil;

    if( anURL )
    {
        // Go ahead and load up the art work
        NSURL * artUrl = [NSURL URLWithString:[anURL stringByAddingPercentEscapesUsingEncoding:NSUTF8StringEncoding]];

        // Don't attempt to fetch artwork from remote. Core will do that alone
        if ([artUrl isFileURL])
            art  = [[[NSImage alloc] initWithContentsOfURL:artUrl] autorelease];
    }

    // If anything was found, lets save it to the meta data dictionary
    [self performSelectorOnMainThread:@selector(setArtwork:) withObject:art waitUntilDone:NO];

    [pool release];
}

- (void)setArtwork:(NSImage *)art
{
    if (!art)
    {
        [metaDictionary removeObjectForKey:@"artwork"];
        return;
    }

    [metaDictionary setObject:art forKey:@"artwork"];
}

- (void)metaChanged:(NSString *)metaType
{
    [self fetchMetaInformationFromLibVLCWithType:metaType];
}

- (void)subItemAdded
{
    if( subitems )
        return; /* Nothing to do */

    libvlc_media_list_t * p_mlist = libvlc_media_subitems( p_md );

    NSAssert( p_mlist, @"The mlist shouldn't be nil, we are receiving a subItemAdded");

    [self willChangeValueForKey:@"subitems"];
    subitems = [[VLCMediaList mediaListWithLibVLCMediaList:p_mlist] retain];
    [self didChangeValueForKey:@"subitems"];
    libvlc_media_list_release( p_mlist );
}

- (void)setStateAsNumber:(NSNumber *)newStateAsNumber
{
    [self setState: [newStateAsNumber intValue]];
}

- (id)valueForKeyPath:(NSString *)keyPath
{
    if( !isArtFetched && [keyPath isEqualToString:@"metaDictionary.artwork"])
    {
        isArtFetched = YES;
        /* Force the retrieval of the artwork now that someone asked for it */
        [self fetchMetaInformationFromLibVLCWithType: VLCMetaInformationArtworkURL];
    }
    else if( !areOthersMetaFetched && [keyPath hasPrefix:@"metaDictionary."])
    {
        areOthersMetaFetched = YES;
        /* Force VLCMetaInformationTitle, that will trigger preparsing
         * And all the other meta will be added through the libvlc event system */
        [self fetchMetaInformationFromLibVLCWithType: VLCMetaInformationTitle];

    }
    else if( !isArtURLFetched && [keyPath hasPrefix:@"metaDictionary.artworkURL"])
    {
        isArtURLFetched = YES;
        /* Force isArtURLFetched, that will trigger artwork download eventually
         * And all the other meta will be added through the libvlc event system */
        [self fetchMetaInformationFromLibVLCWithType: VLCMetaInformationArtworkURL];
    }

    return [super valueForKeyPath:keyPath];
}
@end

/******************************************************************************
 * Implementation VLCMedia (VLCMediaPlayerBridging)
 */

@implementation VLCMedia (VLCMediaPlayerBridging)

- (void)setLength:(VLCTime *)value
{
    if (length && value && [length compare:value] == NSOrderedSame)
        return;

    [length release];
    length = value ? [value retain] : nil;
}

@end
