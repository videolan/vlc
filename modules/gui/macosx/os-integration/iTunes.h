/*****************************************************************************
 * iTunes.h
 *****************************************************************************
 * Copyright (C) 2013-2014 VLC authors and VideoLAN
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

#import <AppKit/AppKit.h>
#import <ScriptingBridge/ScriptingBridge.h>


@class iTunesPrintSettings, iTunesApplication, iTunesItem, iTunesArtwork, iTunesEncoder, iTunesEQPreset, iTunesPlaylist, iTunesAudioCDPlaylist, iTunesLibraryPlaylist, iTunesRadioTunerPlaylist, iTunesSource, iTunesTrack, iTunesAudioCDTrack, iTunesFileTrack, iTunesSharedTrack, iTunesURLTrack, iTunesUserPlaylist, iTunesFolderPlaylist, iTunesVisual, iTunesWindow, iTunesBrowserWindow, iTunesEQWindow, iTunesPlaylistWindow;

enum iTunesEKnd {
	iTunesEKndTrackListing = 'kTrk' /* a basic listing of tracks within a playlist */,
	iTunesEKndAlbumListing = 'kAlb' /* a listing of a playlist grouped by album */,
	iTunesEKndCdInsert = 'kCDi' /* a printout of the playlist for jewel case inserts */
};
typedef enum iTunesEKnd iTunesEKnd;

enum iTunesEnum {
	iTunesEnumStandard = 'lwst' /* Standard PostScript error handling */,
	iTunesEnumDetailed = 'lwdt' /* print a detailed report of PostScript errors */
};
typedef enum iTunesEnum iTunesEnum;

enum iTunesEPlS {
	iTunesEPlSStopped = 'kPSS',
	iTunesEPlSPlaying = 'kPSP',
	iTunesEPlSPaused = 'kPSp',
	iTunesEPlSFastForwarding = 'kPSF',
	iTunesEPlSRewinding = 'kPSR'
};
typedef enum iTunesEPlS iTunesEPlS;

enum iTunesERpt {
	iTunesERptOff = 'kRpO',
	iTunesERptOne = 'kRp1',
	iTunesERptAll = 'kAll'
};
typedef enum iTunesERpt iTunesERpt;

enum iTunesEVSz {
	iTunesEVSzSmall = 'kVSS',
	iTunesEVSzMedium = 'kVSM',
	iTunesEVSzLarge = 'kVSL'
};
typedef enum iTunesEVSz iTunesEVSz;

enum iTunesESrc {
	iTunesESrcLibrary = 'kLib',
	iTunesESrcIPod = 'kPod',
	iTunesESrcAudioCD = 'kACD',
	iTunesESrcMP3CD = 'kMCD',
	iTunesESrcRadioTuner = 'kTun',
	iTunesESrcSharedLibrary = 'kShd',
	iTunesESrcUnknown = 'kUnk'
};
typedef enum iTunesESrc iTunesESrc;

enum iTunesESrA {
	iTunesESrAAlbums = 'kSrL' /* albums only */,
	iTunesESrAAll = 'kAll' /* all text fields */,
	iTunesESrAArtists = 'kSrR' /* artists only */,
	iTunesESrAComposers = 'kSrC' /* composers only */,
	iTunesESrADisplayed = 'kSrV' /* visible text fields */,
	iTunesESrASongs = 'kSrS' /* song names only */
};
typedef enum iTunesESrA iTunesESrA;

enum iTunesESpK {
	iTunesESpKNone = 'kNon',
	iTunesESpKBooks = 'kSpA',
	iTunesESpKFolder = 'kSpF',
	iTunesESpKGenius = 'kSpG',
	iTunesESpKITunesU = 'kSpU',
	iTunesESpKLibrary = 'kSpL',
	iTunesESpKMovies = 'kSpI',
	iTunesESpKMusic = 'kSpZ',
	iTunesESpKPodcasts = 'kSpP',
	iTunesESpKPurchasedMusic = 'kSpM',
	iTunesESpKTVShows = 'kSpT'
};
typedef enum iTunesESpK iTunesESpK;

enum iTunesEVdK {
	iTunesEVdKNone = 'kNon' /* not a video or unknown video kind */,
	iTunesEVdKMovie = 'kVdM' /* movie track */,
	iTunesEVdKMusicVideo = 'kVdV' /* music video track */,
	iTunesEVdKTVShow = 'kVdT' /* TV show track */
};
typedef enum iTunesEVdK iTunesEVdK;

enum iTunesERtK {
	iTunesERtKUser = 'kRtU' /* user-specified rating */,
	iTunesERtKComputed = 'kRtC' /* iTunes-computed rating */
};
typedef enum iTunesERtK iTunesERtK;



/*
 * Standard Suite
 */

@interface iTunesPrintSettings : SBObject

@property (readonly) NSInteger copies;  // the number of copies of a document to be printed
@property (readonly) BOOL collating;  // Should printed copies be collated?
@property (readonly) NSInteger startingPage;  // the first page of the document to be printed
@property (readonly) NSInteger endingPage;  // the last page of the document to be printed
@property (readonly) NSInteger pagesAcross;  // number of logical pages laid across a physical page
@property (readonly) NSInteger pagesDown;  // number of logical pages laid out down a physical page
@property (readonly) iTunesEnum errorHandling;  // how errors are handled
@property (copy, readonly) NSDate *requestedPrintTime;  // the time at which the desktop printer should print the document
@property (copy, readonly) NSArray *printerFeatures;  // printer specific options
@property (copy, readonly) NSString *faxNumber;  // for fax number
@property (copy, readonly) NSString *targetPrinter;  // for target printer

- (void) printPrintDialog:(BOOL)printDialog withProperties:(iTunesPrintSettings *)withProperties kind:(iTunesEKnd)kind theme:(NSString *)theme;  // Print the specified object(s)
- (void) close;  // Close an object
- (void) delete;  // Delete an element from an object
- (SBObject *) duplicateTo:(SBObject *)to;  // Duplicate one or more object(s)
- (BOOL) exists;  // Verify if an object exists
- (void) open;  // open the specified object(s)
- (void) playOnce:(BOOL)once;  // play the current track or the specified track or file.

@end



/*
 * iTunes Suite
 */

// The application program
@interface iTunesApplication : SBApplication

- (SBElementArray *) browserWindows;
- (SBElementArray *) encoders;
- (SBElementArray *) EQPresets;
- (SBElementArray *) EQWindows;
- (SBElementArray *) playlistWindows;
- (SBElementArray *) sources;
- (SBElementArray *) visuals;
- (SBElementArray *) windows;

@property (copy) iTunesEncoder *currentEncoder;  // the currently selected encoder (MP3, AIFF, WAV, etc.)
@property (copy) iTunesEQPreset *currentEQPreset;  // the currently selected equalizer preset
@property (copy, readonly) iTunesPlaylist *currentPlaylist;  // the playlist containing the currently targeted track
@property (copy, readonly) NSString *currentStreamTitle;  // the name of the current song in the playing stream (provided by streaming server)
@property (copy, readonly) NSString *currentStreamURL;  // the URL of the playing stream or streaming web site (provided by streaming server)
@property (copy, readonly) iTunesTrack *currentTrack;  // the current targeted track
@property (copy) iTunesVisual *currentVisual;  //  the currently selected visual plug-in
@property BOOL EQEnabled;  // is the equalizer enabled?
@property BOOL fixedIndexing;  // true if all AppleScript track indices should be independent of the play order of the owning playlist.
@property BOOL frontmost;  // is iTunes the frontmost application?
@property BOOL fullScreen;  // are visuals displayed using the entire screen?
@property (copy, readonly) NSString *name;  // the name of the application
@property BOOL mute;  // has the sound output been muted?
@property NSInteger playerPosition;  // the player’s position within the currently playing track in seconds.
@property (readonly) iTunesEPlS playerState;  // is iTunes stopped, paused, or playing?
@property (copy, readonly) SBObject *selection;  // the selection visible to the user
@property NSInteger soundVolume;  // the sound output volume (0 = minimum, 100 = maximum)
@property (copy, readonly) NSString *version;  // the version of iTunes
@property BOOL visualsEnabled;  // are visuals currently being displayed?
@property iTunesEVSz visualSize;  // the size of the displayed visual

- (void) printPrintDialog:(BOOL)printDialog withProperties:(iTunesPrintSettings *)withProperties kind:(iTunesEKnd)kind theme:(NSString *)theme;  // Print the specified object(s)
- (void) run;  // run iTunes
- (void) quit;  // quit iTunes
- (iTunesTrack *) add:(NSArray *)x to:(SBObject *)to;  // add one or more files to a playlist
- (void) backTrack;  // reposition to beginning of current track or go to previous track if already at start of current track
- (iTunesTrack *) convert:(NSArray *)x;  // convert one or more files or tracks
- (void) fastForward;  // skip forward in a playing track
- (void) nextTrack;  // advance to the next track in the current playlist
- (void) pause;  // pause playback
- (void) playOnce:(BOOL)once;  // play the current track or the specified track or file.
- (void) playpause;  // toggle the playing/paused state of the current track
- (void) previousTrack;  // return to the previous track in the current playlist
- (void) resume;  // disable fast forward/rewind and resume playback, if playing.
- (void) rewind;  // skip backwards in a playing track
- (void) stop;  // stop playback
- (void) update;  // update the specified iPod
- (void) eject;  // eject the specified iPod
- (void) subscribe:(NSString *)x;  // subscribe to a podcast feed
- (void) updateAllPodcasts;  // update all subscribed podcast feeds
- (void) updatePodcast;  // update podcast feed
- (void) openLocation:(NSString *)x;  // Opens a Music Store or audio stream URL

@end

// an item
@interface iTunesItem : SBObject

@property (copy, readonly) SBObject *container;  // the container of the item
- (NSInteger) id;  // the id of the item
@property (readonly) NSInteger index;  // The index of the item in internal application order.
@property (copy) NSString *name;  // the name of the item
@property (copy, readonly) NSString *persistentID;  // the id of the item as a hexadecimal string. This id does not change over time.

- (void) printPrintDialog:(BOOL)printDialog withProperties:(iTunesPrintSettings *)withProperties kind:(iTunesEKnd)kind theme:(NSString *)theme;  // Print the specified object(s)
- (void) close;  // Close an object
- (void) delete;  // Delete an element from an object
- (SBObject *) duplicateTo:(SBObject *)to;  // Duplicate one or more object(s)
- (BOOL) exists;  // Verify if an object exists
- (void) open;  // open the specified object(s)
- (void) playOnce:(BOOL)once;  // play the current track or the specified track or file.
- (void) reveal;  // reveal and select a track or playlist

@end

// a piece of art within a track
@interface iTunesArtwork : iTunesItem

@property (copy) NSImage *data;  // data for this artwork, in the form of a picture
@property (copy) NSString *objectDescription;  // description of artwork as a string
@property (readonly) BOOL downloaded;  // was this artwork downloaded by iTunes?
@property (copy, readonly) NSNumber *format;  // the data format for this piece of artwork
@property NSInteger kind;  // kind or purpose of this piece of artwork
@property (copy) NSData *rawData;  // data for this artwork, in original format


@end

// converts a track to a specific file format
@interface iTunesEncoder : iTunesItem

@property (copy, readonly) NSString *format;  // the data format created by the encoder


@end

// equalizer preset configuration
@interface iTunesEQPreset : iTunesItem

@property double band1;  // the equalizer 32 Hz band level (-12.0 dB to +12.0 dB)
@property double band2;  // the equalizer 64 Hz band level (-12.0 dB to +12.0 dB)
@property double band3;  // the equalizer 125 Hz band level (-12.0 dB to +12.0 dB)
@property double band4;  // the equalizer 250 Hz band level (-12.0 dB to +12.0 dB)
@property double band5;  // the equalizer 500 Hz band level (-12.0 dB to +12.0 dB)
@property double band6;  // the equalizer 1 kHz band level (-12.0 dB to +12.0 dB)
@property double band7;  // the equalizer 2 kHz band level (-12.0 dB to +12.0 dB)
@property double band8;  // the equalizer 4 kHz band level (-12.0 dB to +12.0 dB)
@property double band9;  // the equalizer 8 kHz band level (-12.0 dB to +12.0 dB)
@property double band10;  // the equalizer 16 kHz band level (-12.0 dB to +12.0 dB)
@property (readonly) BOOL modifiable;  // can this preset be modified?
@property double preamp;  // the equalizer preamp level (-12.0 dB to +12.0 dB)
@property BOOL updateTracks;  // should tracks which refer to this preset be updated when the preset is renamed or deleted?


@end

// a list of songs/streams
@interface iTunesPlaylist : iTunesItem

- (SBElementArray *) tracks;

@property (readonly) NSInteger duration;  // the total length of all songs (in seconds)
@property (copy) NSString *name;  // the name of the playlist
@property (copy, readonly) iTunesPlaylist *parent;  // folder which contains this playlist (if any)
@property BOOL shuffle;  // play the songs in this playlist in random order?
@property (readonly) long long size;  // the total size of all songs (in bytes)
@property iTunesERpt songRepeat;  // playback repeat mode
@property (readonly) iTunesESpK specialKind;  // special playlist kind
@property (copy, readonly) NSString *time;  // the length of all songs in MM:SS format
@property (readonly) BOOL visible;  // is this playlist visible in the Source list?

- (void) moveTo:(SBObject *)to;  // Move playlist(s) to a new location
- (iTunesTrack *) searchFor:(NSString *)for_ only:(iTunesESrA)only;  // search a playlist for tracks matching the search string. Identical to entering search text in the Search field in iTunes.

@end

// a playlist representing an audio CD
@interface iTunesAudioCDPlaylist : iTunesPlaylist

- (SBElementArray *) audioCDTracks;

@property (copy) NSString *artist;  // the artist of the CD
@property BOOL compilation;  // is this CD a compilation album?
@property (copy) NSString *composer;  // the composer of the CD
@property NSInteger discCount;  // the total number of discs in this CD’s album
@property NSInteger discNumber;  // the index of this CD disc in the source album
@property (copy) NSString *genre;  // the genre of the CD
@property NSInteger year;  // the year the album was recorded/released


@end

// the master music library playlist
@interface iTunesLibraryPlaylist : iTunesPlaylist

- (SBElementArray *) fileTracks;
- (SBElementArray *) URLTracks;
- (SBElementArray *) sharedTracks;


@end

// the radio tuner playlist
@interface iTunesRadioTunerPlaylist : iTunesPlaylist

- (SBElementArray *) URLTracks;


@end

// a music source (music library, CD, device, etc.)
@interface iTunesSource : iTunesItem

- (SBElementArray *) audioCDPlaylists;
- (SBElementArray *) libraryPlaylists;
- (SBElementArray *) playlists;
- (SBElementArray *) radioTunerPlaylists;
- (SBElementArray *) userPlaylists;

@property (readonly) long long capacity;  // the total size of the source if it has a fixed size
@property (readonly) long long freeSpace;  // the free space on the source if it has a fixed size
@property (readonly) iTunesESrc kind;

- (void) update;  // update the specified iPod
- (void) eject;  // eject the specified iPod

@end

// playable audio source
@interface iTunesTrack : iTunesItem

- (SBElementArray *) artworks;

@property (copy) NSString *album;  // the album name of the track
@property (copy) NSString *albumArtist;  // the album artist of the track
@property NSInteger albumRating;  // the rating of the album for this track (0 to 100)
@property (readonly) iTunesERtK albumRatingKind;  // the rating kind of the album rating for this track
@property (copy) NSString *artist;  // the artist/source of the track
@property (readonly) NSInteger bitRate;  // the bit rate of the track (in kbps)
@property double bookmark;  // the bookmark time of the track in seconds
@property BOOL bookmarkable;  // is the playback position for this track remembered?
@property NSInteger bpm;  // the tempo of this track in beats per minute
@property (copy) NSString *category;  // the category of the track
@property (copy) NSString *comment;  // freeform notes about the track
@property BOOL compilation;  // is this track from a compilation album?
@property (copy) NSString *composer;  // the composer of the track
@property (readonly) NSInteger databaseID;  // the common, unique ID for this track. If two tracks in different playlists have the same database ID, they are sharing the same data.
@property (copy, readonly) NSDate *dateAdded;  // the date the track was added to the playlist
@property (copy) NSString *objectDescription;  // the description of the track
@property NSInteger discCount;  // the total number of discs in the source album
@property NSInteger discNumber;  // the index of the disc containing this track on the source album
@property (readonly) double duration;  // the length of the track in seconds
@property BOOL enabled;  // is this track checked for playback?
@property (copy) NSString *episodeID;  // the episode ID of the track
@property NSInteger episodeNumber;  // the episode number of the track
@property (copy) NSString *EQ;  // the name of the EQ preset of the track
@property double finish;  // the stop time of the track in seconds
@property BOOL gapless;  // is this track from a gapless album?
@property (copy) NSString *genre;  // the music/audio genre (category) of the track
@property (copy) NSString *grouping;  // the grouping (piece) of the track. Generally used to denote movements within a classical work.
@property (copy, readonly) NSString *kind;  // a text description of the track
@property (copy) NSString *longDescription;
@property (copy) NSString *lyrics;  // the lyrics of the track
@property (copy, readonly) NSDate *modificationDate;  // the modification date of the content of this track
@property NSInteger playedCount;  // number of times this track has been played
@property (copy) NSDate *playedDate;  // the date and time this track was last played
@property (readonly) BOOL podcast;  // is this track a podcast episode?
@property NSInteger rating;  // the rating of this track (0 to 100)
@property (readonly) iTunesERtK ratingKind;  // the rating kind of this track
@property (copy, readonly) NSDate *releaseDate;  // the release date of this track
@property (readonly) NSInteger sampleRate;  // the sample rate of the track (in Hz)
@property NSInteger seasonNumber;  // the season number of the track
@property BOOL shufflable;  // is this track included when shuffling?
@property NSInteger skippedCount;  // number of times this track has been skipped
@property (copy) NSDate *skippedDate;  // the date and time this track was last skipped
@property (copy) NSString *show;  // the show name of the track
@property (copy) NSString *sortAlbum;  // override string to use for the track when sorting by album
@property (copy) NSString *sortArtist;  // override string to use for the track when sorting by artist
@property (copy) NSString *sortAlbumArtist;  // override string to use for the track when sorting by album artist
@property (copy) NSString *sortName;  // override string to use for the track when sorting by name
@property (copy) NSString *sortComposer;  // override string to use for the track when sorting by composer
@property (copy) NSString *sortShow;  // override string to use for the track when sorting by show name
@property (readonly) NSInteger size;  // the size of the track (in bytes)
@property double start;  // the start time of the track in seconds
@property (copy, readonly) NSString *time;  // the length of the track in MM:SS format
@property NSInteger trackCount;  // the total number of tracks on the source album
@property NSInteger trackNumber;  // the index of the track on the source album
@property BOOL unplayed;  // is this track unplayed?
@property iTunesEVdK videoKind;  // kind of video track
@property NSInteger volumeAdjustment;  // relative volume adjustment of the track (-100% to 100%)
@property NSInteger year;  // the year the track was recorded/released


@end

// a track on an audio CD
@interface iTunesAudioCDTrack : iTunesTrack

@property (copy, readonly) NSURL *location;  // the location of the file represented by this track


@end

// a track representing an audio file (MP3, AIFF, etc.)
@interface iTunesFileTrack : iTunesTrack

@property (copy) NSURL *location;  // the location of the file represented by this track

- (void) refresh;  // update file track information from the current information in the track’s file

@end

// a track residing in a shared library
@interface iTunesSharedTrack : iTunesTrack


@end

// a track representing a network stream
@interface iTunesURLTrack : iTunesTrack

@property (copy) NSString *address;  // the URL for this track

- (void) download;  // download podcast episode

@end

// custom playlists created by the user
@interface iTunesUserPlaylist : iTunesPlaylist

- (SBElementArray *) fileTracks;
- (SBElementArray *) URLTracks;
- (SBElementArray *) sharedTracks;

@property BOOL shared;  // is this playlist shared?
@property (readonly) BOOL smart;  // is this a Smart Playlist?


@end

// a folder that contains other playlists
@interface iTunesFolderPlaylist : iTunesUserPlaylist


@end

// a visual plug-in
@interface iTunesVisual : iTunesItem


@end

// any window
@interface iTunesWindow : iTunesItem

@property NSRect bounds;  // the boundary rectangle for the window
@property (readonly) BOOL closeable;  // does the window have a close box?
@property (readonly) BOOL collapseable;  // does the window have a collapse (windowshade) box?
@property BOOL collapsed;  // is the window collapsed?
@property NSPoint position;  // the upper left position of the window
@property (readonly) BOOL resizable;  // is the window resizable?
@property BOOL visible;  // is the window visible?
@property (readonly) BOOL zoomable;  // is the window zoomable?
@property BOOL zoomed;  // is the window zoomed?


@end

// the main iTunes window
@interface iTunesBrowserWindow : iTunesWindow

@property BOOL minimized;  // is the small player visible?
@property (copy, readonly) SBObject *selection;  // the selected songs
@property (copy) iTunesPlaylist *view;  // the playlist currently displayed in the window


@end

// the iTunes equalizer window
@interface iTunesEQWindow : iTunesWindow

@property BOOL minimized;  // is the small EQ window visible?


@end

// a sub-window showing a single playlist
@interface iTunesPlaylistWindow : iTunesWindow

@property (copy, readonly) SBObject *selection;  // the selected songs
@property (copy, readonly) iTunesPlaylist *view;  // the playlist displayed in the window


@end

