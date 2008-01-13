/* Controller */

#import <Cocoa/Cocoa.h>
#import <VLCKit/VLCKit.h>

@interface Controller : NSObject
{
    IBOutlet NSView * remoteURLView;
    IBOutlet NSView * workingView;
    IBOutlet NSWindow * window;
    IBOutlet NSButton * openConvertedFileButton;

    NSNumber * selectedStreamOutput;
    NSString * remoteURLAsString;

    VLCMedia * media;
    VLCStreamSession * streamSession;
    
    NSString * outputFilePath;
    NSString * outputFolderPath;
}

- (void)awakeFromNib;

@property (retain) VLCMedia * media;
@property (retain) VLCStreamSession * streamSession;
@property (assign) NSNumber * selectedStreamOutput;
@property (retain,readonly) NSString * outputFilePath;
@property (retain,readonly) NSString * outputFolderPath;
@property (retain) NSString * remoteURLAsString;

- (IBAction)convert:(id)sender;
- (IBAction)openConvertedFile:(id)sender;
- (IBAction)openConvertedEnclosingFolder:(id)sender;
- (IBAction)pickOutputFolderPath:(id)sender;
@end
