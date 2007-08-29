#import "Controller.h"

@implementation Controller
- (void)awakeFromNib
{
    NSRect rect;
    VLCPlaylistDataSource * aDataSource;

    /* Won't be released */
    videoView = [[VLCVideoView alloc] init];

    rect = [videoHolderView frame];
    [[window contentView] replaceSubview: videoHolderView with: videoView];
    [videoView setFrame: rect];
    [videoView setAutoresizingMask: NSViewHeightSizable|NSViewWidthSizable];

    /* Won't be released */
    playlist = [[VLCPlaylist alloc] init];

    /* Won't be released */
    aDataSource = [[VLCPlaylistDataSource alloc] initWithPlaylist:playlist videoView:videoView];

    [playlistOutline setDataSource: aDataSource];
    [playlistOutline registerForDraggedTypes: [NSArray arrayWithObjects:NSFilenamesPboardType, NSURLPboardType, nil]];
}

- (void)play:(id)sender
{
    if(![videoView playlist])
        [videoView setPlaylist: playlist];
    if( [sender isKindOfClass:[NSTableView class]] && [sender selectedRow] >= 0)
        [videoView playItemAtIndex: [sender selectedRow]];
}
@end
