//
//  VLCMediaLayer.m
//  VLC
//
//  Created by Pierre d'Herbemont on 1/14/08.
//  Copyright 2008 __MyCompanyName__. All rights reserved.
//

#import "VLCMediaLayer.h"

/*****************************************************************************
 * Some configuration first. Defines the size of the artwork layer.
 */
static CGSize kArtworkSize = { 256., 256. };

/*****************************************************************************
 * @interface VLCMediaLayer (Private)
 */

@interface VLCMediaLayer (Private)
- (void)updateSublayers;
@end

/*****************************************************************************
 * @interface VLCMediaLayer ()
 */
@interface VLCMediaLayer ()
@property (retain,readwrite) VLCMedia * media;
@property (retain,readwrite) CATextLayer * titleLayer;
@property (retain,readwrite) CATextLayer * artistLayer;
@property (retain,readwrite) CATextLayer * genreLayer;
@property (retain,readwrite) CALayer * artworkLayer;
@end

/*****************************************************************************
 * @implementation VLCMediaLayer
 */
@implementation VLCMediaLayer
@synthesize displayFullInformation;
@synthesize media;
@synthesize titleLayer;
@synthesize genreLayer;
@synthesize artistLayer;
@synthesize artworkLayer;

+ (id)layer
{
    return [self layerWithMedia:[VLCMedia mediaAsNodeWithName:@"Empty Media"]];
}
+ (id)layerWithMedia:(VLCMedia *)aMedia
{
    VLCMediaLayer * me = [super layer];

    if(!me) return nil;

    me.media = aMedia;
    me.displayFullInformation = YES;

    /* Set the default layout */
    me.titleLayer = [CATextLayer layer];
    me.artistLayer = [CATextLayer layer];
    me.genreLayer = [CATextLayer layer];
    CALayer * textLayer = [CALayer layer];
    NSDictionary * textStyle = [NSDictionary dictionaryWithObjectsAndKeys:
                                        [NSNumber numberWithInteger:12], @"cornerRadius",
                                        [NSValue valueWithSize:NSMakeSize(5, 0)], @"margin",
                                        @"Lucida-Bold", @"font",
                                        CGColorCreateGenericGray(0.5, 1.),@"foregroundColor",
                                        [NSNumber numberWithInteger:18], @"fontSize",
                                        [NSNumber numberWithFloat: .8], @"shadowOpacity",
                                        [NSNumber numberWithFloat: 1.], @"shadowRadius",
                                        kCAAlignmentLeft, @"alignmentMode",
                                        nil];
    NSDictionary * textTitleStyle = [NSDictionary dictionaryWithObjectsAndKeys:
                                        [NSNumber numberWithInteger:12], @"cornerRadius",
                                        [NSValue valueWithSize:NSMakeSize(5, 0)], @"margin",
                                        @"Lucida", @"font",
                                        [NSNumber numberWithInteger:26], @"fontSize",
                                        [NSNumber numberWithFloat: .7], @"shadowOpacity",
                                        [NSNumber numberWithFloat: 3.], @"shadowRadius",
                                        kCAAlignmentLeft, @"alignmentMode",
                                        nil];
    /* First off, text */
	me.titleLayer.style = textTitleStyle;
    me.titleLayer.string = @"Title";
    me.titleLayer.name = @"title";
	[me.titleLayer addConstraint:[CAConstraint constraintWithAttribute:kCAConstraintMinX relativeTo:@"superlayer" attribute:kCAConstraintMinX offset:0.]];
	[me.titleLayer addConstraint:[CAConstraint constraintWithAttribute:kCAConstraintMaxX relativeTo:@"superlayer" attribute:kCAConstraintMaxX offset:0.]];
	[me.titleLayer addConstraint:[CAConstraint constraintWithAttribute:kCAConstraintMinY relativeTo:@"artist" attribute:kCAConstraintMaxY offset:10.]];
	me.artistLayer.style = textStyle;
    me.artistLayer.string = @"Artist";
    me.artistLayer.name = @"artist";
	[me.artistLayer addConstraint:[CAConstraint constraintWithAttribute:kCAConstraintMinX relativeTo:@"title" attribute:kCAConstraintMinX]];
	[me.artistLayer addConstraint:[CAConstraint constraintWithAttribute:kCAConstraintMaxX relativeTo:@"title" attribute:kCAConstraintMaxX]];
	[me.artistLayer addConstraint:[CAConstraint constraintWithAttribute:kCAConstraintMidY relativeTo:@"superlayer" attribute:kCAConstraintMidY]];
	me.genreLayer.style = textStyle;
    me.genreLayer.string = @"Genre";
    me.genreLayer.name = @"genre";
	[me.genreLayer addConstraint:[CAConstraint constraintWithAttribute:kCAConstraintMinX relativeTo:@"title" attribute:kCAConstraintMinX]];
	[me.genreLayer addConstraint:[CAConstraint constraintWithAttribute:kCAConstraintMaxX relativeTo:@"title" attribute:kCAConstraintMaxX]];
	[me.genreLayer addConstraint:[CAConstraint constraintWithAttribute:kCAConstraintMaxY relativeTo:@"artist" attribute:kCAConstraintMinY offset:-10.]];


    [textLayer addSublayer:me.titleLayer];
    [textLayer addSublayer:me.artistLayer];
    [textLayer addSublayer:me.genreLayer];
    textLayer.contentsGravity = kCAGravityCenter;
    textLayer.layoutManager = [CAConstraintLayoutManager layoutManager];

    /* Empty layer for picture */
    me.artworkLayer = [CALayer layer];
    me.artworkLayer.backgroundColor = CGColorCreateGenericGray(0.5, 0.4);
    me.artworkLayer.borderColor = CGColorCreateGenericRGB(1., 1., 1., .8);
    me.artworkLayer.borderWidth = 3.0;

   // me.artworkLayer.frame = CGRectMake(0.,0., kArtworkSize.width, kArtworkSize.height);
    textLayer.frame = CGRectMake(0.,0., kArtworkSize.width, kArtworkSize.height);

    /* Position the text and the artwork layer */
    CALayer * container = [CALayer layer];
    me.artworkLayer.name = @"artworkLayer";
    textLayer.name = @"textLayer";
    container.name = @"artContainer";
    container.layoutManager = [CAConstraintLayoutManager layoutManager];

    [container addSublayer:me.artworkLayer];
	[container addConstraint:[CAConstraint constraintWithAttribute:kCAConstraintMinX relativeTo:@"superlayer" attribute:kCAConstraintMinX offset:60.]];
	[container addConstraint:[CAConstraint constraintWithAttribute:kCAConstraintMidY relativeTo:@"superlayer" attribute:kCAConstraintMidY]];
	[container addConstraint:[CAConstraint constraintWithAttribute:kCAConstraintHeight relativeTo:@"superlayer" attribute:kCAConstraintHeight scale:.6 offset:0.]];
	[container addConstraint:[CAConstraint constraintWithAttribute:kCAConstraintMaxX relativeTo:@"superlayer" attribute:kCAConstraintMidX]];

	[me.artworkLayer addConstraint:[CAConstraint constraintWithAttribute:kCAConstraintMinX relativeTo:@"superlayer" attribute:kCAConstraintMinX]];
	[me.artworkLayer addConstraint:[CAConstraint constraintWithAttribute:kCAConstraintMinY relativeTo:@"superlayer" attribute:kCAConstraintMinY]];
	[me.artworkLayer addConstraint:[CAConstraint constraintWithAttribute:kCAConstraintMaxY relativeTo:@"superlayer" attribute:kCAConstraintMaxY]];
	[me.artworkLayer addConstraint:[CAConstraint constraintWithAttribute:kCAConstraintMaxX relativeTo:@"superlayer" attribute:kCAConstraintMaxX]];
        
	[textLayer addConstraint:[CAConstraint constraintWithAttribute:kCAConstraintMinX relativeTo:@"artContainer" attribute:kCAConstraintMaxX]];
	[textLayer addConstraint:[CAConstraint constraintWithAttribute:kCAConstraintMaxX relativeTo:@"superlayer" attribute:kCAConstraintMaxX ]];
	[textLayer addConstraint:[CAConstraint constraintWithAttribute:kCAConstraintMidY relativeTo:@"artContainer" attribute:kCAConstraintMidY]];

    me.artworkLayer.zPosition = -30.f;

    me.artworkLayer.shadowOpacity = .3;
    me.artworkLayer.shadowRadius = 10.;
    static CATransform3D rot, projection;
    static BOOL transformInited = NO;
    if( !transformInited )
    {
        rot = CATransform3DMakeRotation(.1
        , 0., 1., 0.);
            projection = CATransform3DIdentity; 
        projection.m34 = 1. / -80.;
        transformInited = YES;
    }
    me.artworkLayer.transform = rot;
    container.sublayerTransform = projection;

    me.layoutManager = [CAConstraintLayoutManager layoutManager];
    [me addSublayer:textLayer];
    [me addSublayer:container];

    [me updateSublayers];

    /* The following will trigger -observeValueForKeyPath: ofObject: change: context: */
    [me.media addObserver:me forKeyPath:@"metaDictionary.title" options:NSKeyValueObservingOptionNew context:nil];
    [me.media addObserver:me forKeyPath:@"metaDictionary.genre" options:NSKeyValueObservingOptionNew context:nil];
    [me.media addObserver:me forKeyPath:@"metaDictionary.artist" options:NSKeyValueObservingOptionNew context:nil];
    [me.media addObserver:me forKeyPath:@"metaDictionary.artwork" options:NSKeyValueObservingOptionNew context:nil];

    return me;
}

- (void)dealloc
{
    /* Previously registered in +layerWithMediaArrayController: +layerWithMedia:*/
    [self.media removeObserver:self forKeyPath:@"metaDictionary.title"];
    [self.media removeObserver:self forKeyPath:@"metaDictionary.genre"];
    [self.media removeObserver:self forKeyPath:@"metaDictionary.artist"];
    [self.media removeObserver:self forKeyPath:@"metaDictionary.artwork"];

    [super dealloc];
}
@end

/*****************************************************************************
 * @implementation VLCMediaLayer (Private)
 */
@implementation VLCMediaLayer (Private)

- (void)updateSublayers
{
    [CATransaction begin];
    self.titleLayer.string = [self.media.metaDictionary objectForKey:@"title"];
    NSString * artist = [self.media.metaDictionary objectForKey:@"artist"];
    self.artistLayer.string = artist ? artist : @"No Artist";
    NSString * genre = [self.media.metaDictionary objectForKey:@"genre"];
    self.genreLayer.string = genre ? genre : @"No Genre";
    if( [self.media.metaDictionary objectForKey:@"artwork"] )
    {
        self.artworkLayer.contents = (id)[[self.media.metaDictionary objectForKey:@"artwork"] CGImage];
        self.artworkLayer.contentsGravity = kCAGravityResizeAspect;
        self.artworkLayer.borderWidth = 0.;
        self.artworkLayer.backgroundColor = nil;
    }
    [CATransaction commit];
}

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context
{
    if( [keyPath hasPrefix:@"metaDictionary"] )
    {
        [self updateSublayers];
        return;
    }
    [super observeValueForKeyPath:keyPath ofObject:object change:change context:context];
}
@end
