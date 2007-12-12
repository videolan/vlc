/*****************************************************************************
 * VLCMedia.m: VLC.framework VLCMedia implementation
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
NSString *VLCMetaInformationTitle       = @"title";
NSString *VLCMetaInformationArtist      = @"artist";
NSString *VLCMetaInformationGenre       = @"genre";
NSString *VLCMetaInformationCopyright   = @"copyright";
NSString *VLCMetaInformationAlbum       = @"album";
NSString *VLCMetaInformationTrackNumber = @"trackNumber";
NSString *VLCMetaInformationDescription = @"description";
NSString *VLCMetaInformationRating      = @"rating";
NSString *VLCMetaInformationDate        = @"date";
NSString *VLCMetaInformationSetting     = @"setting";
NSString *VLCMetaInformationURL         = @"url";
NSString *VLCMetaInformationLanguage    = @"language";
NSString *VLCMetaInformationNowPlaying  = @"nowPlaying";
NSString *VLCMetaInformationPublisher   = @"publisher";
NSString *VLCMetaInformationEncodedBy   = @"encodedBy";
NSString *VLCMetaInformationArtworkURL  = @"artworkURL";
NSString *VLCMetaInformationArtwork     = @"artwork";
NSString *VLCMetaInformationTrackID     = @"trackID";

/* Notification Messages */
NSString *VLCMediaMetaChanged           = @"VLCMediaMetaChanged";

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

/* Callback Methods */
- (void)metaChanged:(NSString *)metaType;
@end


/******************************************************************************
 * LibVLC Event Callback
 */
static void HandleMediaMetaChanged(const libvlc_event_t *event, void *self)
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    [[VLCEventManager sharedManager] callOnMainThreadObject:self
                                                 withMethod:@selector(metaChanged:)
                                       withArgumentAsObject:[VLCMedia metaTypeToString:event->u.media_descriptor_meta_changed.meta_type]];
    [pool release];
}

static void HandleMediaDurationChanged(const libvlc_event_t *event, void *self)
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    
    //[[VLCEventManager sharedManager] callOnMainThreadObject:self
//                                                 withMethod:@selector(setLength:)
//                                       withArgumentAsObject:[VLCTime timeWithNumber:
//                                           [NSNumber numberWithLongLong:event->u.media_descriptor_duration_changed.new_duration]]];
    [pool release];
}

/******************************************************************************
 * Implementation
 */
@implementation VLCMedia
+ (id)mediaWithPath:(NSString *)aPath;
{
    return [[[VLCMedia alloc] initWithPath:(id)aPath] autorelease];
}

+ (id)mediaWithURL:(NSURL *)aURL;
{
    return [[[VLCMedia alloc] initWithPath:(id)[aURL path]] autorelease];
}

- (id)initWithPath:(NSString *)aPath
{        
    if (self = [super init])
    {
        
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);
        
        p_md = libvlc_media_descriptor_new([VLCLibrary sharedInstance],
                                           [aPath UTF8String],
                                           &ex);
        quit_on_exception(&ex);
        
        url = [aPath copy];
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
            libvlc_event_manager_t *p_em = libvlc_media_descriptor_event_manager(p_md, NULL);
            libvlc_event_detach(p_em, libvlc_MediaDescriptorMetaChanged,     HandleMediaMetaChanged,     self, NULL);
            libvlc_event_detach(p_em, libvlc_MediaDescriptorDurationChanged, HandleMediaDurationChanged, self, NULL);
        }
        [super release];
    }
}

- (void)dealloc
{
    // Testing to see if the pointer exists is not required, if the pointer is null
    // then the release message is not sent to it.
    [self setDelegate:nil];
    [self setLength:nil];

    [url release];
    [subitems release];
    [metaDictionary release];

    libvlc_media_descriptor_release( p_md );

    [super dealloc];
}

- (NSString *)description
{
    NSString *result = [metaDictionary objectForKey:VLCMetaInformationTitle];
    return (result ? result : url);
}

- (NSComparisonResult)compare:(VLCMedia *)media
{
    if (self == media)
        return NSOrderedSame;
    else if (!media)
        return NSOrderedDescending;
    else
        return [[self url] compare:[media url]];
}

- (NSString *)url
{
    return [[url copy] autorelease];
}

- (VLCMediaList *)subitems
{
    return subitems;
}

- (VLCTime *)length
{
    if (!length) 
    {
        // Try figuring out what the length is
        long long duration = libvlc_media_descriptor_get_duration( p_md, NULL );
        if (duration > -1) 
        {
            [self setLength:[VLCTime timeWithNumber:[NSNumber numberWithLongLong:duration]]];
            return [[length retain] autorelease];
        } 
    }
    return [VLCTime nullTime];
}

- (VLCTime *)lengthWaitUntilDate:(NSDate *)aDate
{
    static const long long thread_sleep = 10000;

    if (![url hasPrefix:@"file://"])
        return [self length];
    else if (!length)
    {
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
    return libvlc_media_descriptor_is_preparsed( p_md, NULL );
}

- (NSDictionary *)metaDictionary
{
    return metaDictionary;
}

- (void)setDelegate:(id)value
{
    delegate = value;
}

- (id)delegate
{
    return delegate;
}
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
        libvlc_exception_t ex;
        libvlc_exception_init( &ex );
        char * p_url;
        
        p_url = libvlc_media_descriptor_get_mrl( md, &ex );
        quit_on_exception( &ex );
        
        url = [[NSString stringWithCString:p_url] retain];
        
        libvlc_media_descriptor_retain( md );
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
    libvlc_exception_t ex;
    libvlc_exception_init( &ex );

    libvlc_media_descriptor_set_user_data( p_md, (void*)self, &ex );
    quit_on_exception( &ex );

    libvlc_event_manager_t *p_em = libvlc_media_descriptor_event_manager( p_md, &ex );
    libvlc_event_attach(p_em, libvlc_MediaDescriptorMetaChanged,     HandleMediaMetaChanged,     self, &ex);
    libvlc_event_attach(p_em, libvlc_MediaDescriptorDurationChanged, HandleMediaDurationChanged, self, &ex);
    quit_on_exception( &ex );
    
    libvlc_media_list_t *p_mlist = libvlc_media_descriptor_subitems( p_md, NULL );
    if (!p_mlist)
        subitems = nil;
    else
    {
        subitems = [[VLCMediaList mediaListWithLibVLCMediaList:p_mlist] retain];
        libvlc_media_list_release( p_mlist );
    }
    
    /* Force VLCMetaInformationTitle, that will trigger preparsing
     * And all the other meta will be added through the libvlc event system */
    [self fetchMetaInformationFromLibVLCWithType: VLCMetaInformationTitle];
}

- (void)fetchMetaInformationFromLibVLCWithType:(NSString *)metaType
{
    char * psz_value = libvlc_media_descriptor_get_meta( p_md, [VLCMedia stringToMetaType:metaType], NULL);
    NSString *newValue = psz_value ? [NSString stringWithUTF8String: psz_value] : nil;
    NSString *oldValue = [metaDictionary valueForKey:metaType];
    free(psz_value);

    if ( !(newValue && oldValue && [oldValue compare:newValue] == NSOrderedSame) )
    {
        if ([metaType isEqualToString:VLCMetaInformationArtworkURL])
        {
            [NSThread detachNewThreadSelector:@selector(fetchMetaInformationForArtWorkWithURL:) 
                                         toTarget:self
                                       withObject:newValue];
            return;
        }

        [metaDictionary setValue:newValue forKeyPath:metaType];
    }
}


- (void)fetchMetaInformationForArtWorkWithURL:(NSString *)anURL
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    
    // Go ahead and load up the art work
    NSURL * artUrl = [NSURL URLWithString:[anURL stringByAddingPercentEscapesUsingEncoding:NSUTF8StringEncoding]];
    NSImage * art  = [[[NSImage alloc] initWithContentsOfURL:artUrl] autorelease]; 
        
    // If anything was found, lets save it to the meta data dictionary
    if (art)
    {
        @synchronized(metaDictionary) {
            [metaDictionary setObject:art forKey:VLCMetaInformationArtwork];
        }
    }

    [pool release];
}

- (void)metaChanged:(NSString *)metaType
{
    [self fetchMetaInformationFromLibVLCWithType:metaType];
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
        
    [self willChangeValueForKey:@"length"];
    [length release];       
    length = value ? [value retain] : nil;
    [self didChangeValueForKey:@"length"];
}
@end
