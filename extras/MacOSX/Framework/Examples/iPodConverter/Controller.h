/* Controller */

#import <Cocoa/Cocoa.h>
#import <VLCKit/VLCKit.h>
#import <VLCKit/VLCMediaPlayer.h>

@interface Controller : NSObject
{
    IBOutlet NSView * conversionView;
    IBOutlet NSWindow * window;
    IBOutlet NSButton * openConvertedFileButton;

    NSNumber * selectedStreamOutput;

    VLCMedia * media;
    VLCStreamSession * streamSession;
}

- (void)awakeFromNib;

@property (retain) VLCMedia * media;
@property (retain) VLCStreamSession * streamSession;
@property (assign) NSNumber * selectedStreamOutput;
@property (retain,readonly) NSString * outputFilePath;

- (IBAction)convert:(id)sender;
- (IBAction)openConvertedFile:(id)sender;
- (IBAction)openConvertedEnclosingFolder:(id)sender;
@end
