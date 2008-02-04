#import "Controller.h"

static void *sleepForMe(void)
{
    while (1) sleep(60);
}

@implementation Controller

- (void)awakeFromNib
{
//    atexit((void*)sleepForMe);    // Only used for memory leak debugging

    [NSApp setDelegate:self];

    // Allocate a VLCVideoView instance and tell it what area to occupy.
    NSRect rect = NSMakeRect(0, 0, 0, 0);
    rect.size = [videoHolderView frame].size;
    
    videoView = [[VLCVideoView alloc] initWithFrame:rect];
    [videoHolderView addSubview:videoView];
    [videoView setAutoresizingMask: NSViewHeightSizable|NSViewWidthSizable];
    videoView.fillScreen = YES;
    
    playlist = [[VLCMediaList alloc] init];
    [playlist addObserver:self forKeyPath:@"media" options:NSKeyValueObservingOptionNew context:nil];
    
    player = [[VLCMediaPlayer alloc] initWithVideoView:videoView];
    mediaIndex = -1;

    [playlistOutline registerForDraggedTypes:[NSArray arrayWithObjects:NSFilenamesPboardType, NSURLPboardType, nil]];
    [playlistOutline setDoubleAction:@selector(changeAndPlay:)];
}

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
}

- (void)applicationWillTerminate:(NSNotification *)aNotification
{
    [playlist removeObserver:self forKeyPath:@"media"];
    
    [player pause];
    [player setMedia:nil];
    [player release];
    [playlist release];
    [videoView release];
}

- (void)changeAndPlay:(id)sender
{
    if ([playlistOutline selectedRow] != mediaIndex)
    {
		[self setMediaIndex:[playlistOutline selectedRow]];
		if (![player isPlaying])
			[player play];
    }
}

- (void)setMediaIndex:(int)value
{
    if ([playlist count] <= 0)
		return;
    
    if (value < 0)
		value = 0;
    if (value > [playlist count] - 1)
		value = [playlist count] - 1;
    
    mediaIndex = value;
    [player setMedia:[playlist mediaAtIndex:mediaIndex]];
}

- (void)play:(id)sender
{
    [self setMediaIndex:mediaIndex+1];
    if (![player isPlaying])
    {
		NSLog(@"%@ length = %@", [playlist mediaAtIndex:mediaIndex], [[playlist mediaAtIndex:mediaIndex] lengthWaitUntilDate:[NSDate dateWithTimeIntervalSinceNow:60]]);
		[player play];
    }
}

- (void)pause:(id)sender
{
    NSLog(@"Sending pause message to media player...");
    [player pause];
}

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context
{
    if ([keyPath isEqualToString:@"media"] && object == playlist) {
        [playlistOutline reloadData];
    }
}

// NSTableView Implementation
- (int)numberOfRowsInTableView:(NSTableView *)tableView
{
    return [playlist count];
}

- (id)tableView:(NSTableView *)tableView objectValueForTableColumn:(NSTableColumn *)tableColumn
			row:(int)row
{
    return [(VLCMedia *)[playlist mediaAtIndex:row].metaDictionary valueForKey:VLCMetaInformationTitle];
}

- (NSDragOperation)tableView:(NSTableView*)tv validateDrop:(id <NSDraggingInfo>)info 
				 proposedRow:(int)row proposedDropOperation:(NSTableViewDropOperation)op
{
    return NSDragOperationEvery; /* This is for now */
}

- (BOOL)tableView:(NSTableView *)aTableView acceptDrop:(id <NSDraggingInfo>)info
			  row:(int)row dropOperation:(NSTableViewDropOperation)operation
{
    int i;
    NSArray *droppedItems = [[info draggingPasteboard] propertyListForType:NSFilenamesPboardType];
    
    for (i = 0; i < [droppedItems count]; i++)
    {
        NSString * filename = [droppedItems objectAtIndex:i];
		VLCMedia * media = [VLCMedia mediaWithURL:[NSURL fileURLWithPath:filename]];
		[playlist addMedia:media];
    }
    return YES;
}    

@end
