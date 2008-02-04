#import "Controller.h"


/**********************************************************
 * First off, some value transformer to easily play with
 * bindings
 */
@interface VLCFloat10000FoldTransformer : NSObject
@end

@implementation VLCFloat10000FoldTransformer

+ (Class)transformedValueClass
{
    return [NSNumber class];
}

+ (BOOL)allowsReverseTransformation
{
    return YES;
}

- (id)transformedValue:(id)value
{
    if( !value ) return nil;
 
    if(![value respondsToSelector: @selector(floatValue)])
    {
        [NSException raise: NSInternalInconsistencyException
                    format: @"Value (%@) does not respond to -floatValue.",
        [value class]];
        return nil;
    }
 
    return [NSNumber numberWithFloat: [value floatValue]*10000.];
}

- (id)reverseTransformedValue:(id)value
{
    if( !value ) return nil;
 
    if(![value respondsToSelector: @selector(floatValue)])
    {
        [NSException raise: NSInternalInconsistencyException
                    format: @"Value (%@) does not respond to -floatValue.",
        [value class]];
        return nil;
    }
 
    return [NSNumber numberWithFloat: [value floatValue]/10000.];
}
@end


/**********************************************************
 * @implementation Controller
 */
@interface Controller ()
@property (retain,readwrite) NSString * outputFolderPath;
@end

@implementation Controller
- (id)init
{
    if(self = [super init])
    {
        [self bind:@"outputFolderPath" toObject:[NSUserDefaultsController sharedUserDefaultsController]
              withKeyPath:@"values.outputFolderPath" options:nil]; 
        [[[NSUserDefaultsController sharedUserDefaultsController] values] bind:@"outputFolderPath" toObject:self
              withKeyPath:@"outputFolderPath" options:nil]; 
        VLCFloat10000FoldTransformer *float100fold;
        float100fold = [[[VLCFloat10000FoldTransformer alloc] init] autorelease];
        [NSValueTransformer setValueTransformer:(id)float100fold forName:@"Float10000FoldTransformer"];
        self.media = nil;
        self.streamSession = nil;
        selectedStreamOutput = [[NSNumber alloc] initWithInt:0];
        self.remoteURLAsString = [NSString stringWithString:@"http://youtube.com/watch?v=IXpx2OEWBdA&feature=bz303"];
        outputFilePath = nil;
        if( !self.outputFolderPath || [self.outputFolderPath isKindOfClass:[NSNull class]])
            self.outputFolderPath = [@"~/Movies/Flash Video Converted" stringByExpandingTildeInPath];
    }
    return self;
}

- (void)dealloc
{
    [outputFilePath release];
    [remoteURLAsString release];
    [streamSession release];
    [media release];
    [super dealloc];
}

@synthesize streamSession;
@synthesize selectedStreamOutput;
@synthesize media;
@synthesize outputFolderPath;

- (void)awakeFromNib
{
    [window setShowsResizeIndicator:NO];
    [NSApp setDelegate: self];
}

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
    [VLCLibrary sharedLibrary];
}

- (NSString *)remoteURLAsString
{
    return remoteURLAsString;
}

- (void)setRemoteURLAsString:(NSString *)newURLAsString
{
    [remoteURLAsString release];
    remoteURLAsString = [[newURLAsString copy] retain];
    media = [[newURLAsString copy] retain];
    [self setMedia:[VLCMedia mediaWithPath:newURLAsString]];
}

+ (NSSet *)keyPathsForValuesAffectingOutputFilePath
{
    return [NSSet setWithObjects:@"media.metaDictionary.title", nil];
}

- (void)freezeOutputFilePath
{
    [outputFilePath release];
    outputFilePath = nil;
    outputFilePath = [self outputFilePath];
    [outputFilePath retain];
}

- (NSString *)outputFilePath
{
    if(outputFilePath)
        return [outputFilePath copy];
    VLCMedia * aMedia = self.streamSession ? self.streamSession.media ? self.streamSession.media : self.media : self.media;
    NSString * name = [[[aMedia metaDictionary] objectForKey:@"title"] lastPathComponent];
    NSString * extension = [selectedStreamOutput intValue] == 2 ? @"mpeg" : @"mp4";
    NSString * path = [NSString stringWithFormat:@"%@/%@.%@", self.outputFolderPath, name, extension ];
    int i;
    for( i = 0; [[NSFileManager defaultManager] fileExistsAtPath:path]; i ++)
    {
        path = [NSString stringWithFormat:@"%@/%@ %d.%@", self.outputFolderPath, name, i, extension ];
        if( i > 256 )
        {
            /* Don't got too far */
            /* FIXME: Be nicer with the user and give him a choice for the new name */
            NSRunAlertPanelRelativeToWindow(@"File already exists",
                [NSString stringWithFormat:
                    @"File '%@', already exists. The old one will be deleted when the OK button will be pressed", path],
                @"OK", nil, nil, window);
            break;
        }
    }
    return path;
}


- (IBAction)convert:(id)sender
{
    VLCStreamOutput * streamOutput;
    [self.streamSession removeObserver:self forKeyPath:@"isComplete"];

    self.streamSession = [VLCStreamSession streamSession];
    [self freezeOutputFilePath];

    if([selectedStreamOutput intValue] == 2)
    {
        streamOutput = [VLCStreamOutput mpeg2StreamOutputWithFilePath:[self outputFilePath]];
    }
    else if([selectedStreamOutput intValue] == 1)
    {
        streamOutput = [VLCStreamOutput mpeg4StreamOutputWithFilePath:[self outputFilePath]];
    }
    else
        streamOutput = [VLCStreamOutput ipodStreamOutputWithFilePath:[self outputFilePath]];

    /* Make sure we are exporting to a well known directory */
    [[NSFileManager defaultManager] createDirectoryAtPath:self.outputFolderPath attributes:nil];

    [self.streamSession setStreamOutput:streamOutput];
    [self.streamSession setMedia:self.media];
    [self.streamSession startStreaming];

    [self.streamSession addObserver:self forKeyPath:@"isComplete" options:NSKeyValueObservingOptionNew context:nil];

    /* Show the new view */
    [[window contentView] addSubview:workingView];
    NSRect frame = [workingView frame];
    frame.origin.y -= NSHeight([window contentRectForFrameRect:[window frame]]) + 20.f;
    [workingView setFrame:frame];
    [[window animator] setFrame:NSMakeRect([window frame].origin.x, [window frame].origin.y-NSHeight([workingView frame]), NSWidth([window frame]), NSHeight([window frame])+NSHeight([workingView frame])) display:YES];
}

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context
{
    if([keyPath isEqualToString:@"isComplete"])
    {
        if([self.streamSession isComplete])
        {
            /* Notify the user */
            [[NSSound soundNamed:@"Glass"] play];

            /* Set the icon */
            [openConvertedFileButton setImage:[[NSWorkspace sharedWorkspace] iconForFile:[self outputFilePath]]];
            
            /* Rename the link with a nicer name */
            NSString * oldPath = [self outputFilePath];
            [self freezeOutputFilePath];
            [[NSFileManager defaultManager] moveItemAtPath:oldPath toPath:[self outputFilePath] error:NULL];
        }
        return;
    }
    [super observeValueForKeyPath:keyPath ofObject:object change:change context:context];
}

- (IBAction)openConvertedFile:(id)sender
{
    [[NSWorkspace sharedWorkspace] openFile:[self outputFilePath]];
}

- (IBAction)pickOutputFolderPath:(id)sender;
{
    NSOpenPanel * panel = [NSOpenPanel openPanel];
    [panel setCanChooseFiles:NO];
    [panel setCanChooseDirectories:YES];
    [panel setAllowsMultipleSelection:NO];
    [panel beginSheetForDirectory:self.outputFolderPath file:nil types:nil modalForWindow:[sender window] modalDelegate:self didEndSelector:@selector(openPanelDidEnd:returnCode:contextInfo:) contextInfo:nil];
}

- (void)openPanelDidEnd:(NSOpenPanel *)panel returnCode:(int)returnCode  contextInfo:(void  *)contextInfo
{
    if(returnCode != NSOKButton || ![[panel filenames] count])
        return;
    self.outputFolderPath = [[panel filenames] objectAtIndex:0];
}

- (IBAction)openConvertedEnclosingFolder:(id)sender
{
    [[NSWorkspace sharedWorkspace] selectFile:[self outputFilePath] inFileViewerRootedAtPath:[[self outputFilePath] stringByDeletingLastPathComponent]];
}

- (IBAction)cancel:(id)sender
{
    [self.streamSession stop];
}

@end
