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

@implementation Controller
- (id)init
{
    if(self = [super init])
    {
        VLCFloat10000FoldTransformer *float100fold;
        float100fold = [[[VLCFloat10000FoldTransformer alloc] init] autorelease];
        [NSValueTransformer setValueTransformer:(id)float100fold forName:@"Float10000FoldTransformer"];
        self.media = nil;
        self.streamSession = nil;
        selectedStreamOutput = [[NSNumber alloc] initWithInt:0];
    }
    return self;
}

@synthesize streamSession;
@synthesize selectedStreamOutput;

- (void)awakeFromNib
{
    [window setShowsResizeIndicator:NO];
    [NSApp setDelegate: self];
}

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
    [VLCLibrary sharedLibrary];
}

- (VLCMedia *)media
{
    return media;
}

- (void)setMedia:(VLCMedia *)newMedia
{
    [media release];
    media = [newMedia retain];
    NSRect newFrame = [window frameRectForContentRect:[conversionView frame]];
    [[window animator] setFrame:NSMakeRect([window frame].origin.x, [window frame].origin.y+NSHeight([window frame])-NSHeight(newFrame), NSWidth(newFrame), NSHeight(newFrame)) display:YES];
    [[window animator] setContentView:conversionView];
    [window setMinSize:newFrame.size];
    [window setMaxSize:NSMakeSize(10000., NSHeight(newFrame))];
    [window setShowsResizeIndicator:YES];
}

+ (NSSet *)keyPathsForValuesAffectingOutputFilePath
{
    return [NSSet setWithObjects:@"media.metaDictionary.title", nil];
}
- (NSString *)outputFilePath
{
    return [NSString stringWithFormat:[@"~/Movies/iPod Converted/%@.mp4" stringByExpandingTildeInPath],
                            [[self.media metaDictionary] objectForKey:@"title"]];
}

- (IBAction)convert:(id)sender
{
    self.streamSession = [VLCStreamSession streamSession];
    if([selectedStreamOutput intValue] == 0)
    {
        [self.streamSession setStreamOutput:
                                    [VLCStreamOutput ipodStreamOutputWithFilePath:
                                        [self outputFilePath]
                                    ]];
    }
    else
    {
        /* This doesn't really is useful for the iPod/iPhone...
         * But one day we'll figure that out */
        NSRunAlertPanelRelativeToWindow(@"Warning", @"We can't really stream to the iPod/iPhone for now...\n\nSo we're just streaming using the RTP protocol, and annoucing it via SAP.\n\n(Launch the SAP VLC service discovery module to see it).", @"OK", nil, nil, window);
        [self.streamSession setStreamOutput:
                                    [VLCStreamOutput rtpBroadcastStreamOutput]];
    }


    NSLog(@"Using %@", self.streamSession.streamOutput );

    [self.streamSession setMedia:self.media];
    [self.streamSession startStreaming];

    [openConvertedFileButton setImage:[[NSWorkspace sharedWorkspace] iconForFile:[self outputFilePath]]];
}

- (IBAction)openConvertedFile:(id)sender
{
    [[NSWorkspace sharedWorkspace] openFile:[self outputFilePath]];
}
- (IBAction)openConvertedEnclosingFolder:(id)sender
{
    [[NSWorkspace sharedWorkspace] openFile:[[self outputFilePath] stringByDeletingLastPathComponent]];

}
@end
