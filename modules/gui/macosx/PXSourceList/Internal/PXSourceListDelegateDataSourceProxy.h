//
//  PXSourceListDelegateDataSourceProxy.h
//  PXSourceList
//
//  Created by Alex Rozanski on 25/12/2013.
//  Copyright 2009-14 Alex Rozanski http://alexrozanski.com and other contributors.
//  This software is licensed under the New BSD License. Full details can be found in the README.
//

#import <Foundation/Foundation.h>
#import "../PXSourceList.h"

@interface PXSourceListDelegateDataSourceProxy : NSProxy <NSOutlineViewDelegate, NSOutlineViewDataSource, PXSourceListDelegate, PXSourceListDataSource>

@property (weak, nonatomic) PXSourceList *sourceList;
@property (unsafe_unretained, nonatomic) id <PXSourceListDelegate> delegate;
@property (unsafe_unretained, nonatomic) id <PXSourceListDataSource> dataSource;

- (id)initWithSourceList:(PXSourceList *)sourceList;

@end
