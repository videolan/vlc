//
//  PXSourceListDelegateDataSourceProxy.m
//  PXSourceList
//
//  Created by Alex Rozanski on 25/12/2013.
//  Copyright 2009-14 Alex Rozanski http://alexrozanski.com and other contributors.
//  This software is licensed under the New BSD License. Full details can be found in the README.
//

#import "PXSourceListDelegateDataSourceProxy.h"

#import <objc/runtime.h>
#import "PXSourceListPrivateConstants.h"
#import "PXSourceListRuntimeAdditions.h"

// Internal constants.
static NSString * const forwardingMapForwardingMethodNameKey = @"methodName";
static NSString * const forwardingMapForwardedArgumentIndexesKey = @"forwardedArgumentIndexes";

static NSArray * __outlineViewDelegateMethods = nil;
static NSArray * __outlineViewDataSourceMethods = nil;
static NSArray * __requiredOutlineViewDataSourceMethods = nil;

// Cache the PXSourceListDelegate and PXSourceListDataSource method names so that if these methods are invoked on
// us, we can quickly forward them to the delegate and dataSource using -forwardingTargetForSelector: without going
// through -forwardInvocation:.
static NSArray * __fastPathForwardingDelegateMethods = nil;
static NSArray * __fastPathForwardingDataSourceMethods = nil;

// We want to suppress the warnings for protocol methods not being implemented. As a proxy we will forward these
// messages to the actual delegate and data source.
#pragma clang diagnostic ignored "-Wprotocol"
@implementation PXSourceListDelegateDataSourceProxy

+ (void)initialize
{
    __outlineViewDelegateMethods = px_methodNamesForProtocol(@protocol(NSOutlineViewDelegate));
    __outlineViewDataSourceMethods = px_methodNamesForProtocol(@protocol(NSOutlineViewDataSource));
    __fastPathForwardingDelegateMethods = [self fastPathForwardingDelegateMethods];
    __fastPathForwardingDataSourceMethods = px_methodNamesForProtocol(@protocol(PXSourceListDataSource));

    __requiredOutlineViewDataSourceMethods = @[NSStringFromSelector(@selector(outlineView:numberOfChildrenOfItem:)),
                                               NSStringFromSelector(@selector(outlineView:child:ofItem:)),
                                               NSStringFromSelector(@selector(outlineView:isItemExpandable:)),
                                               NSStringFromSelector(@selector(outlineView:objectValueForTableColumn:byItem:))];

    // Add the custom mappings first before we add the 'regular' mappings.
    [self addCustomMethodNameMappings];

    // Now add the 'regular' mappings.
    [self addEntriesToMethodForwardingMap:[self methodNameMappingsForProtocol:@protocol(NSOutlineViewDelegate)]];
    [self addEntriesToMethodForwardingMap:[self methodNameMappingsForProtocol:@protocol(NSOutlineViewDataSource)]];
}

- (id)initWithSourceList:(PXSourceList *)sourceList
{
    _sourceList = sourceList;

    return self;
}

- (void)dealloc
{
    //Unregister the delegate from receiving notifications
	[[NSNotificationCenter defaultCenter] removeObserver:self.delegate name:nil object:self.sourceList];
}

#pragma mark - Accessors

- (void)setDelegate:(id<PXSourceListDelegate>)delegate
{
    if (self.delegate)
        [[NSNotificationCenter defaultCenter] removeObserver:self.delegate name:nil object:self.sourceList];

    _delegate = delegate;

    //Register the new delegate to receive notifications
	[self registerDelegateToReceiveNotification:PXSLSelectionIsChangingNotification
								   withSelector:@selector(sourceListSelectionIsChanging:)];
	[self registerDelegateToReceiveNotification:PXSLSelectionDidChangeNotification
								   withSelector:@selector(sourceListSelectionDidChange:)];
	[self registerDelegateToReceiveNotification:PXSLItemWillExpandNotification
								   withSelector:@selector(sourceListItemWillExpand:)];
	[self registerDelegateToReceiveNotification:PXSLItemDidExpandNotification
								   withSelector:@selector(sourceListItemDidExpand:)];
	[self registerDelegateToReceiveNotification:PXSLItemWillCollapseNotification
								   withSelector:@selector(sourceListItemWillCollapse:)];
	[self registerDelegateToReceiveNotification:PXSLItemDidCollapseNotification
								   withSelector:@selector(sourceListItemDidCollapse:)];
	[self registerDelegateToReceiveNotification:PXSLDeleteKeyPressedOnRowsNotification
								   withSelector:@selector(sourceListDeleteKeyPressedOnRows:)];
}

- (void)setDataSource:(id<PXSourceListDataSource>)dataSource
{
    _dataSource = dataSource;
}

#pragma mark - NSObject Overrides

- (BOOL)respondsToSelector:(SEL)aSelector
{
    NSString *methodName = NSStringFromSelector(aSelector);

    // Only let the source list override NSOutlineView delegate and data source methods.
    if ([self.sourceList respondsToSelector:aSelector] && ([__outlineViewDataSourceMethods containsObject:methodName] || [__outlineViewDelegateMethods containsObject:methodName]))
        return YES;

    if ([__requiredOutlineViewDataSourceMethods containsObject:methodName])
        return YES;

    if ([__fastPathForwardingDelegateMethods containsObject:methodName])
        return [self.delegate respondsToSelector:aSelector];
    if ([__fastPathForwardingDataSourceMethods containsObject:methodName])
        return [self.dataSource respondsToSelector:aSelector];

    id forwardingObject = [self forwardingObjectForSelector:aSelector];
    NSDictionary *forwardingInformation = [[self class] forwardingInformationForSelector:aSelector];

    if(!forwardingObject || !forwardingInformation)
        return NO;

    return [forwardingObject respondsToSelector:NSSelectorFromString([forwardingInformation objectForKey:forwardingMapForwardingMethodNameKey])];
}

- (BOOL)conformsToProtocol:(Protocol *)protocol
{
    return class_conformsToProtocol(object_getClass(self), protocol);
}

// Fast-path delegate and data source methods aren't handled here; they are taken care of in -forwardingTargetForSelector:.
- (NSMethodSignature *)methodSignatureForSelector:(SEL)aSelector
{
    NSString *methodName = NSStringFromSelector(aSelector);

    struct objc_method_description description = {NULL, NULL};

    if ([__outlineViewDelegateMethods containsObject:methodName])
        description = px_methodDescriptionForProtocolMethod(@protocol(NSOutlineViewDelegate), aSelector);
    else if ([__outlineViewDataSourceMethods containsObject:methodName])
        description = px_methodDescriptionForProtocolMethod(@protocol(NSOutlineViewDataSource), aSelector);

    if (description.name == NULL && description.types == NULL)
        return nil;

    return [NSMethodSignature signatureWithObjCTypes:description.types];
}

- (BOOL) isKindOfClass:(Class)aClass
{
    return NO;
}

- (void)forwardInvocation:(NSInvocation *)anInvocation
{
    SEL sourceSelector = anInvocation.selector;

    // Give the Source List a chance to handle the selector first (this is a bit of a hack for the time being
    // and should be changed).
    if ([self.sourceList respondsToSelector:sourceSelector]) {
        [anInvocation invokeWithTarget:self.sourceList];
        return;
    }

    id forwardingObject = [self forwardingObjectForSelector:sourceSelector];
    NSDictionary *forwardingInformation = [[self class] forwardingInformationForSelector:sourceSelector];

    if(!forwardingObject || !forwardingInformation) {
        [super forwardInvocation:anInvocation];
        return;
    }

    SEL forwardingSelector = NSSelectorFromString([forwardingInformation objectForKey:forwardingMapForwardingMethodNameKey]);

    NSArray *forwardedArgumentIndexes = [forwardingInformation objectForKey:forwardingMapForwardedArgumentIndexesKey];
    anInvocation.selector = forwardingSelector;

    NSMethodSignature *methodSignature = [forwardingObject methodSignatureForSelector:forwardingSelector];

    /* Catch the case where we have advertised ourselves as responding to a selector required by NSOutlineView
       for a valid dataSource but the corresponding PXSourceListDataSource method isn't implemented by the dataSource.
     */
    if ([__requiredOutlineViewDataSourceMethods containsObject:NSStringFromSelector(sourceSelector)]
        && ![self.dataSource respondsToSelector:forwardingSelector]) {
        return;
    }

    /* Modify the arguments in the invocation if the source and target selector arguments are different.

       The forwardedArgumentIndexes array contains the indexes of arguments in the original invocation that we want
       to use in our modified invocation. E.g. @[@0, @2] means take the first and third arguments and only use them
       when forwarding. We want to do this when the forwarded selector has a different number of arguments to the
       source selector (see +addCustomMethodNameMappings).
     
       Note that this implementation only works if the arguments in `forwardedArgumentIndexes` are monotonically
       increasing (which is good enough for now).

     */
    if (forwardedArgumentIndexes) {
        // self and _cmd are arguments 0 and 1.
        NSUInteger invocationArgumentIndex = 2;
        for (NSNumber *newArgumentIndex in forwardedArgumentIndexes) {
            NSInteger forwardedArgumentIndex = newArgumentIndex.integerValue;

            // Handle the case where we want to use (for example) the third argument from the original invocation
            // as the second argument of our modified invocation.
            if (invocationArgumentIndex != forwardedArgumentIndex) {
                NSUInteger argumentSize = 0;
                NSGetSizeAndAlignment([methodSignature getArgumentTypeAtIndex:invocationArgumentIndex], &argumentSize, NULL);

                void *argument = malloc(argumentSize);
                [anInvocation getArgument:argument atIndex:forwardedArgumentIndex + 2]; // Take self and _cmd into account again.
                [anInvocation setArgument:argument atIndex:invocationArgumentIndex];
                free(argument);
            }

            invocationArgumentIndex++;
        }
    }

    [anInvocation invokeWithTarget:forwardingObject];
}

- (id)forwardingTargetForSelector:(SEL)aSelector
{
    NSString *methodName = NSStringFromSelector(aSelector);

    if ([__fastPathForwardingDelegateMethods containsObject:methodName])
        return self.delegate;

    if ([__fastPathForwardingDataSourceMethods containsObject:methodName])
        return self.dataSource;

    return nil;
}

#pragma mark - Method Forwarding

+ (NSMutableDictionary *)methodForwardingMap
{
    static NSMutableDictionary *_methodForwardingMap = nil;
    if (!_methodForwardingMap)
        _methodForwardingMap = [[NSMutableDictionary alloc] init];

    return _methodForwardingMap;
}

+ (void)addEntriesToMethodForwardingMap:(NSDictionary *)entries
{
    NSArray *methodForwardingBlacklist = [self methodForwardingBlacklist];
    NSMutableDictionary *methodForwardingMap = [self methodForwardingMap];

    for (NSString *key in entries) {
        if (![methodForwardingBlacklist containsObject:key] && ![methodForwardingMap objectForKey:key])
            [methodForwardingMap setObject:[entries objectForKey:key] forKey:key];
    }
}

+ (NSDictionary *)methodNameMappingsForProtocol:(Protocol *)protocol
{
    NSMutableDictionary *methodNameMappings = [[NSMutableDictionary alloc] init];
    NSArray *protocolMethods = px_allProtocolMethods(protocol);
    NSString *protocolName = NSStringFromProtocol(protocol);

    for (NSDictionary *methodInfo in protocolMethods) {
        NSString *methodName = [methodInfo objectForKey:px_protocolMethodNameKey];
        NSString *mappedMethodName = [self mappedMethodNameForMethodName:methodName];
        if (!mappedMethodName) {
            NSLog(@"PXSourceList: couldn't map method %@ from %@", methodName, protocolName);
            continue;
        }

        [methodNameMappings setObject:@{forwardingMapForwardingMethodNameKey: mappedMethodName}
                               forKey:methodName];
    }

    return methodNameMappings;
}

+ (NSString *)mappedMethodNameForMethodName:(NSString *)methodName
{
    NSString *outlineViewSearchString = @"outlineView";
    NSUInteger letterVOffset = [outlineViewSearchString rangeOfString:@"V"].location;
    NSCharacterSet *uppercaseLetterCharacterSet = [NSCharacterSet uppercaseLetterCharacterSet];

    NSRange outlineViewStringRange = [methodName rangeOfString:outlineViewSearchString options:NSCaseInsensitiveSearch];

    // If for some reason we can't map the method name, try to fail gracefully.
    if (outlineViewStringRange.location == NSNotFound)
        return nil;

    BOOL isOCapitalized = [uppercaseLetterCharacterSet characterIsMember:[methodName characterAtIndex:outlineViewStringRange.location]];
    BOOL isVCapitalized = [uppercaseLetterCharacterSet characterIsMember:[methodName characterAtIndex:outlineViewStringRange.location + letterVOffset]];
    return [methodName stringByReplacingCharactersInRange:outlineViewStringRange
                                               withString:[NSString stringWithFormat:@"%@ource%@ist", isOCapitalized ? @"S" : @"s", isVCapitalized ? @"L" : @"l"]];

}

- (id)forwardingObjectForSelector:(SEL)selector
{
    if ([__outlineViewDataSourceMethods containsObject:NSStringFromSelector(selector)])
        return self.dataSource;

    if ([__outlineViewDelegateMethods containsObject:NSStringFromSelector(selector)])
        return self.delegate;

    return nil;
}

+ (NSDictionary *)forwardingInformationForSelector:(SEL)selector
{
    return [[self methodForwardingMap] objectForKey:NSStringFromSelector(selector)];
}

// These methods won't have mappings created for them.
+ (NSArray *)methodForwardingBlacklist
{
    return @[NSStringFromSelector(@selector(outlineView:shouldSelectTableColumn:)),
             NSStringFromSelector(@selector(outlineView:shouldReorderColumn:toColumn:)),
             NSStringFromSelector(@selector(outlineView:mouseDownInHeaderOfTableColumn:)),
             NSStringFromSelector(@selector(outlineView:didClickTableColumn:)),
             NSStringFromSelector(@selector(outlineView:didDragTableColumn:)),
             NSStringFromSelector(@selector(outlineView:sizeToFitWidthOfColumn:)),
             NSStringFromSelector(@selector(outlineView:shouldReorderColumn:toColumn:)),
             NSStringFromSelector(@selector(outlineViewColumnDidMove:)),
             NSStringFromSelector(@selector(outlineViewColumnDidResize:)),
             NSStringFromSelector(@selector(outlineView:isGroupItem:))];
}

/* Add custom mappings for method names which can't have "outlineView" simply replaced with "sourceList".
 
   For example, -outlineView:objectValueForTableColumn:byItem: should be forwarded to -sourceList:objectValueForItem:. We also only want to
   forward the 1st and 3rd arguments when invoking this second selector.

 */
+ (void)addCustomMethodNameMappings
{
    [self addCustomMethodNameMappingFromSelector:@selector(outlineView:objectValueForTableColumn:byItem:)
                                      toSelector:@selector(sourceList:objectValueForItem:)
                        forwardedArgumentIndexes:@[@0, @2]];
    [self addCustomMethodNameMappingFromSelector:@selector(outlineView:setObjectValue:forTableColumn:byItem:)
                                      toSelector:@selector(sourceList:setObjectValue:forItem:)
                        forwardedArgumentIndexes:@[@0, @1, @3]];
    [self addCustomMethodNameMappingFromSelector:@selector(outlineView:viewForTableColumn:item:)
                                      toSelector:@selector(sourceList:viewForItem:)
                        forwardedArgumentIndexes:@[@0, @2]];
    [self addCustomMethodNameMappingFromSelector:@selector(outlineView:willDisplayCell:forTableColumn:item:)
                                      toSelector:@selector(sourceList:willDisplayCell:forItem:)
                        forwardedArgumentIndexes:@[@0, @1, @3]];
    [self addCustomMethodNameMappingFromSelector:@selector(outlineView:shouldEditTableColumn:item:)
                                      toSelector:@selector(sourceList:shouldEditItem:)
                        forwardedArgumentIndexes:@[@0, @2]];
    [self addCustomMethodNameMappingFromSelector:@selector(outlineView:toolTipForCell:rect:tableColumn:item:mouseLocation:)
                                      toSelector:@selector(sourceList:toolTipForCell:rect:item:mouseLocation:)
                        forwardedArgumentIndexes:@[@0, @1, @2, @4, @5]];
    [self addCustomMethodNameMappingFromSelector:@selector(outlineView:typeSelectStringForTableColumn:item:)
                                      toSelector:@selector(sourceList:typeSelectStringForItem:)
                        forwardedArgumentIndexes:@[@0, @2]];
    [self addCustomMethodNameMappingFromSelector:@selector(outlineView:shouldShowCellExpansionForTableColumn:item:)
                                      toSelector:@selector(sourceList:shouldShowCellExpansionForItem:)
                        forwardedArgumentIndexes:@[@0, @2]];
    [self addCustomMethodNameMappingFromSelector:@selector(outlineView:shouldTrackCell:forTableColumn:item:)
                                      toSelector:@selector(sourceList:shouldTrackCell:forItem:)
                        forwardedArgumentIndexes:@[@0, @1, @3]];
    [self addCustomMethodNameMappingFromSelector:@selector(outlineView:dataCellForTableColumn:item:)
                                      toSelector:@selector(sourceList:dataCellForItem:)
                        forwardedArgumentIndexes:@[@0, @2]];
}

+ (void)addCustomMethodNameMappingFromSelector:(SEL)fromSelector toSelector:(SEL)toSelector forwardedArgumentIndexes:(NSArray *)argumentIndexes
{
    [[self methodForwardingMap] setObject:@{forwardingMapForwardingMethodNameKey: NSStringFromSelector(toSelector),
                                            forwardingMapForwardedArgumentIndexesKey: argumentIndexes}
                                   forKey:NSStringFromSelector(fromSelector)];
}

- (BOOL)getForwardingObject:(id*)outObject andForwardingSelector:(SEL*)outSelector forSelector:(SEL)selector
{
    NSDictionary *methodForwardingMap = [[self class] methodForwardingMap];
    NSString *originalMethodName = NSStringFromSelector(selector);
    NSDictionary *forwardingInfo = [methodForwardingMap objectForKey:originalMethodName];
    if (!forwardingInfo)
        return NO;

    id forwardingObject;
    if ([__outlineViewDelegateMethods containsObject:originalMethodName])
        forwardingObject = self.delegate;
    else if ([__outlineViewDataSourceMethods containsObject:originalMethodName])
        forwardingObject = self.dataSource;

    if (!forwardingObject)
        return NO;

    if (outObject)
        *outObject = forwardingObject;

    if (outSelector)
        *outSelector = NSSelectorFromString([forwardingInfo objectForKey:forwardingMapForwardingMethodNameKey]);
    
    return YES;
}

+ (NSArray *)fastPathForwardingDelegateMethods
{
    NSMutableArray *methods = [px_methodNamesForProtocol(@protocol(PXSourceListDelegate)) mutableCopy];

    // Add the NSControl delegate methods manually (unfortunately these aren't part of a formal protocol).
    [methods addObject:px_methodNameForSelector(@selector(controlTextDidEndEditing:))];
    [methods addObject:px_methodNameForSelector(@selector(controlTextDidBeginEditing:))];
    [methods addObject:px_methodNameForSelector(@selector(controlTextDidChange:))];

    return methods;
}

#pragma mark - Notifications

- (void)registerDelegateToReceiveNotification:(NSString*)notification withSelector:(SEL)selector
{
	NSNotificationCenter *defaultCenter = [NSNotificationCenter defaultCenter];

	//Set the delegate as a receiver of the notification if it implements the notification method
	if([self.delegate respondsToSelector:selector]) {
		[defaultCenter addObserver:self.delegate
						  selector:selector
							  name:notification
							object:self.sourceList];
	}
}

/* Notification wrappers */
- (void)outlineViewSelectionIsChanging:(NSNotification *)notification
{
	[[NSNotificationCenter defaultCenter] postNotificationName:PXSLSelectionIsChangingNotification object:self.sourceList];
}


- (void)outlineViewSelectionDidChange:(NSNotification *)notification
{
	[[NSNotificationCenter defaultCenter] postNotificationName:PXSLSelectionDidChangeNotification object:self.sourceList];
}

- (void)outlineViewItemWillExpand:(NSNotification *)notification
{
	[[NSNotificationCenter defaultCenter] postNotificationName:PXSLItemWillExpandNotification
														object:self.sourceList
													  userInfo:[notification userInfo]];
}

- (void)outlineViewItemDidExpand:(NSNotification *)notification
{
	[[NSNotificationCenter defaultCenter] postNotificationName:PXSLItemDidExpandNotification
														object:self.sourceList
													  userInfo:[notification userInfo]];
}

- (void)outlineViewItemWillCollapse:(NSNotification *)notification
{
	[[NSNotificationCenter defaultCenter] postNotificationName:PXSLItemWillCollapseNotification
														object:self.sourceList
													  userInfo:[notification userInfo]];
}

- (void)outlineViewItemDidCollapse:(NSNotification *)notification
{
	[[NSNotificationCenter defaultCenter] postNotificationName:PXSLItemDidCollapseNotification
														object:self.sourceList
													  userInfo:[notification userInfo]];
}

@end
