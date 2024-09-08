//
//  VLCLibraryDataSource.h
//  VLC
//
//  Created by Claudio Cambra on 9/9/24.
//

#ifndef VLCLibraryDataSource_h
#define VLCLibraryDataSource_h

#import <Foundation/Foundation.h>

@protocol VLCLibraryDataSource <NSObject>

@optional
- (void)connect;
- (void)disconnect;

@end

#endif /* VLCLibraryDataSource_h */
