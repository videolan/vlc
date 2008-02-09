/* Controller */

#import <Cocoa/Cocoa.h>
#import <VLCKit/VLCKit.h>

@interface Controller : NSObject
{
    IBOutlet id window;
    IBOutlet id playlistOutline;
    IBOutlet id videoHolderView;

    VLCVideoView * videoView;
    VLCMediaList *playlist;
    VLCMediaPlayer *player;
    int mediaIndex;
}
- (void)awakeFromNib;

- (void)setMediaIndex:(int)value;
- (void)play:(id)sender;
- (void)pause:(id)sender;

@end
