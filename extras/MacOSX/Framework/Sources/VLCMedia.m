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

/* libvlc event callback */
static void HandleMediaMetaChanged(const libvlc_event_t *event, void *self)
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    
    [[VLCEventManager sharedManager] callOnMainThreadObject:self
                                                 withMethod:@selector(metaChanged:)
                                       withArgumentAsObject:[NSNumber numberWithInt:(int)event->u.media_descriptor_meta_changed.meta_type]];
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


// TODO: Documentation
@interface VLCMedia (Private)
/* Statics */
+ (libvlc_meta_t)stringToMetaType:(NSString *)string;
+ (NSString *)metaTypeToString:(libvlc_meta_t)type;

/* Initializers */
- (void)initInternalMediaDescriptor;

/* Operations */
- (BOOL)setMetaValue:(char *)value forKey:(NSString *)key;
- (void)fetchMetaInformation;
- (void)fetchMetaInformationForArtWorkWithURL:(NSString *)anURL;
- (void)notifyChangeForKey:(NSString *)key withOldValue:(id)oldValue;

/* Callback Methods */
- (void)metaChanged:(NSNumber *)metaType;
@end

@implementation VLCMedia
+ (id)mediaWithPath:(NSString *)aPath;
{
    return [[[VLCMedia alloc] initWithPath:(id)aPath] autorelease];
}

+ (id)mediaWithURL:(NSURL *)aURL;
{
    return [[[VLCMedia alloc] initWithPath:(id)[aURL path] autorelease];
}

- (id)initWithPath:(NSString *)aPath
{        
    if (self = [super init])
    {
        
        libvlc_exception_t ex;
        libvlc_exception_init(&ex);
        
        p_md = libvlc_media_descriptor_new([VLCLibrary sharedInstance],
                                           [aURL cString],
                                           &ex);
        quit_on_exception(&ex);
        
        url = [aURL copy];
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
    NSString *result = nil;
    if (metaDictionary)
        result = [metaDictionary objectForKey:VLCMetaInformationTitle];
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
#define CLOCK_FREQ      1000000
#define THREAD_SLEEP    ((long long)(0.010*CLOCK_FREQ))

    if (![url hasPrefix:@"file://"])
        return [self length];
    else if (!length)
    {
        while (!length && ![self isPreparsed] && [aDate timeIntervalSinceNow] > 0)
        {
            usleep( THREAD_SLEEP );
        }
        
        // So we're done waiting, but sometimes we trap the fact that the parsing
        // was done before the length gets assigned, so lets go ahead and assign
        // it ourselves.
        if (!length)
            return [self length];
    }
#undef CLOCK_FREQ
#undef THREAD_SLEEP
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
        
        [self initInternalMediaDescriptor];
    }
    return self;
}

- (void *)libVLCMediaDescriptor
{
    return p_md;
}
@end

@implementation VLCMedia (Private)
+ (libvlc_meta_t)stringToMetaType:(NSString *)string
{
#define VLCStringToMeta( name, string ) if ([VLCMetaInformation##name compare:string] == NSOrderedSame) return libvlc_meta_##name;
    VLCStringToMeta(Title, string);
    VLCStringToMeta(Artist, string);
    VLCStringToMeta(Genre, string);
    VLCStringToMeta(Copyright, string);
    VLCStringToMeta(Album, string);
    VLCStringToMeta(TrackNumber, string);
    VLCStringToMeta(Description, string);
    VLCStringToMeta(Rating, string);
    VLCStringToMeta(Date, string);
    VLCStringToMeta(Setting, string);
    VLCStringToMeta(URL, string);
    VLCStringToMeta(Language, string);
    VLCStringToMeta(NowPlaying, string);
    VLCStringToMeta(Publisher, string);
    VLCStringToMeta(ArtworkURL, string);
    VLCStringToMeta(TrackID, string);
#undef VLCStringToMeta
    return -1;
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
        [subitems release];
        subitems = [[VLCMediaList mediaListWithLibVLCMediaList:p_mlist] retain];
        libvlc_media_list_release( p_mlist );
    }
    [self fetchMetaInformation];
}

- (BOOL)setMetaValue:(char *)value forKey:(NSString *)key
{
    BOOL result = NO;
    
    NSString *oldValue = [metaDictionary valueForKey:key];
    if ((!oldValue && value) || (oldValue && !value) || (oldValue && value && [oldValue compare:[NSString stringWithCString:value]] != NSOrderedSame))
    {
        if (!metaDictionary)
            metaDictionary = [[NSMutableDictionary alloc] initWithCapacity:3];

        if (value)
            [metaDictionary setValue:[NSString stringWithCString:value] forKeyPath:key];
        else
            [metaDictionary setValue:nil forKeyPath:key];
        
        if ([key compare:VLCMetaInformationArtworkURL] == NSOrderedSame)
        {
            if ([metaDictionary valueForKey:VLCMetaInformationArtworkURL])
            {
                // Initialize a new thread
                [NSThread detachNewThreadSelector:@selector(fetchMetaInformationForArtWorkWithURL:) 
                                         toTarget:self
                                       withObject:[metaDictionary valueForKey:VLCMetaInformationArtworkURL]];
            }
        }
        result = YES;
    }
    free( value );
    return result;
}

- (void)fetchMetaInformation
{
    // TODO: Only fetch meta data that has been requested.  Just don't fetch
    // it, just because.
    
    [self setMetaValue:libvlc_media_descriptor_get_meta( p_md, libvlc_meta_Title,      NULL ) forKey:[VLCMedia metaTypeToString:libvlc_meta_Title]];
    [self setMetaValue:libvlc_media_descriptor_get_meta( p_md, libvlc_meta_Artist,     NULL ) forKey:[VLCMedia metaTypeToString:libvlc_meta_Artist]];
    [self setMetaValue:libvlc_media_descriptor_get_meta( p_md, libvlc_meta_ArtworkURL, NULL ) forKey:[VLCMedia metaTypeToString:libvlc_meta_ArtworkURL]];
}

- (void)fetchMetaInformationForArtWorkWithURL:(NSString *)anURL
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    @try
    {
        // Go ahead and load up the art work
        NSURL *artUrl = [NSURL URLWithString:[anURL stringByAddingPercentEscapesUsingEncoding:NSUTF8StringEncoding]];
        NSImage *art  = [[[NSImage alloc] initWithContentsOfURL:artUrl] autorelease]; 
        
        // If anything was found, lets save it to the meta data dictionary
        if (art)
        {
            @synchronized(metaDictionary)
            {
                [metaDictionary setObject:art forKey:VLCMetaInformationArtwork];
            }
            
            // Let the world know that there is new art work available
            [self notifyChangeForKey:VLCMetaInformationArtwork withOldValue:nil];
        }
    }
    @finally 
    {
        [pool release];
    }
}

- (void)notifyChangeForKey:(NSString *)key withOldValue:(id)oldValue
{
    // Send out a formal notification
    [[NSNotificationCenter defaultCenter] postNotificationName:VLCMediaMetaChanged
                                                        object:self
                                                      userInfo:[NSDictionary dictionaryWithObjectsAndKeys:
                                                          key, @"key",
                                                          oldValue, @"oldValue", 
                                                          nil]];
    
    // Now notify the delegate
    if (delegate && [delegate respondsToSelector:@selector(media:metaValueChangedFrom:forKey:)])
        [delegate media:self metaValueChangedFrom:oldValue forKey:key];
}

- (void)metaChanged:(NSNumber *)metaType
{
    // TODO: Only retrieve the meta that was changed
    // Can we figure out what piece was changed instead of retreiving everything?
    NSString *key = [VLCMedia metaTypeToString:[metaType intValue]];
    id oldValue = (metaDictionary ? [metaDictionary valueForKey:key] : nil);

    // Update the meta data
    if ([self setMetaValue:libvlc_media_descriptor_get_meta(p_md, [metaType intValue], NULL) forKey:key])
        // There was actually a change, send out the notifications
        [self notifyChangeForKey:key withOldValue:oldValue];
}
@end

@implementation VLCMedia (VLCMediaPlayerBridging)

- (void)setLength:(VLCTime *)value
{
    if (length != value)
    {
        if (length && value && [length compare:value] == NSOrderedSame)
            return;
        
        [self willChangeValueForKey:@"length"];

        if (length) {
            [length release];
            length = nil;
        }
        
        if (value)
            length = [value retain];

        [self didChangeValueForKey:@"length"];
    }
}
@end
