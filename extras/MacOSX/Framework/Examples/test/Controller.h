/* Controller */

#import <Cocoa/Cocoa.h>
#import <VLC/VLC.h>

@interface Controller : NSObject
{
    IBOutlet id window;
    IBOutlet id playlistOutline;
    IBOutlet id videoHolderView;
    
    VLCVideoView * videoView;
    VLCPlaylist  * playlist;
}
- (void)awakeFromNib;

- (void)play:(id)sender;
@end
