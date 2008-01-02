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

    playlist = [[VLCMediaList alloc] init];
    [playlist setDelegate:self];
    
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
		[player play];
}

- (void)pause:(id)sender
{
    NSLog(@"Sending pause message to media player...");
    [player pause];
}

//
- (void)mediaList:(VLCMediaList *)mediaList mediaAdded:(VLCMedia *)media atIndex:(int)index
{
    [playlistOutline reloadData];
}

- (void)mediaList:(VLCMediaList *)mediaList mediaRemoved:(VLCMedia *)media atIndex:(int)index
{
    [playlistOutline reloadData];
}

// NSTableView Implementation
- (int)numberOfRowsInTableView:(NSTableView *)tableView
{
    return [playlist count];
}

- (id)tableView:(NSTableView *)tableView objectValueForTableColumn:(NSTableColumn *)tableColumn
			row:(int)row
{
    return [[playlist mediaAtIndex:row] description];
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
		NSLog(@"%@ length = %@", media, [media lengthWaitUntilDate:[NSDate dateWithTimeIntervalSinceNow:60]]);
		[playlist addMedia:media];
    }
    return YES;
}    

@end
