//
//  intf_qdview.c
//  vlc
//
//  Created by fgp on Mon Oct 29 2001.
//  Copyright (c) 2001 __MyCompanyName__. All rights reserved.
//

#import "intf_qdview.h"

NSString *VlcQuickDrawViewDidResize = @"VlcQuickDrawViewDidDraw" ;

@implementation VlcQuickDrawView

- (id)initWithFrame:(NSRect)frame {
    self = [super initWithFrame:frame];
    return self;
}

- (void)drawRect:(NSRect)rect {
    [super drawRect:rect] ;
    [[NSNotificationCenter defaultCenter] postNotificationName:VlcQuickDrawViewDidResize object:self] ;
}

@end
