/*
 * Spotify.h
 */

#import <AppKit/AppKit.h>
#import <ScriptingBridge/ScriptingBridge.h>

OSType const kSpotifyPlayerStateStopped = 'kPSS';
OSType const kSpotifyPlayerStatePlaying = 'kPSP';
OSType const kSpotifyPlayerStatePaused  = 'kPSp';

@interface SpotifyApplication : SBApplication
@property (readonly) OSType playerState;  // is Spotify stopped, paused, or playing?
- (void)play;
- (void)pause;
@end
