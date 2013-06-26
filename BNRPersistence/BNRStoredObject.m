// The MIT License
//
// Copyright (c) 2008 Big Nerd Ranch, Inc.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#import "BNRStoredObject.h"
#import "BNRStore.h"
#import "BNRStoreBackend.h"
#import "BNRUniquingTable.h"
#import "BNRResizableUniquingTable.h"
#import "BNRDataBuffer.h"
#import "BNRDataBuffer+Encryption.h"
#import "BNRClassMetaData.h"

@interface BNRStore (StoredObjectIsFriend)

#if kUseBNRResizableUniquingTable
- (BNRResizableUniquingTable *)uniquingTable;
#else
- (BNRUniquingTable *)uniquingTable;
#endif

@end


@implementation BNRStore (StoredObjectIsFriend)

#if kUseBNRResizableUniquingTable
- (BNRResizableUniquingTable *)uniquingTable
#else
- (BNRUniquingTable *)uniquingTable
#endif
{
    return uniquingTable;
}
@end

@interface BNRStoredObject ()
- (void)setHasContent:(BOOL)yn;
@end

@implementation BNRStoredObject

+ (unsigned char)currentClassVersionNumber {
	// If a subclass is upgraded with additional BNRStoredObject properties after the subclass has been
	// added to the store, the subclass should override this to return an appropriately-incremented version number.
	// This provides a single point of truth for the current version, and will ensure that as instances are
	// loaded, per-instance versioning will seamlessly update any added ivars.
	//
	// For more info see comments at -upgradeAllInstancesinStore, -upgradeAllBNRStoredObjectsOfClass
	// and -upgradeAllObjectsForAnyClassesNeedingUpgrade.
	
	return [[self class] version];
}

// ** Added BMonk
// Client apps, instead of calling -addClass: for all their BNRStoredObject subclasses
// can call +configureInStore to make versioning "just work".
// This could alternatively be done just as well with a method on BNRStore,
// something like [aBNRStore configureClass:[myBNRStoredObject class]];
// Hard to say which is better...
+ (void)configureInStore:(BNRStore *)s
{
	[s addClass:[self class]];
	
	// BNRStoredObject subclasses which have been upgraded may override +currentClassVersionNumber 
	// to provide a single source for their version number. Doing so will ensure versioning just works.
	[[self class] setVersion:[[self class] currentClassVersionNumber]];
}

// ** Added BMonk
+ (BOOL)upgradeAllInstancesinStore:(BNRStore *)storeToUpgrade {
	
	//
	// 1. Load each object according to how its -readContentFromBuffer behaves for its pre-upgraded -versionNumber
	//
	
	// Loading all objects could be an issue with huge stores - might need to load and upgrade in chunks.
	NSArray *allObjectsInClass = [storeToUpgrade allObjectsForClass:[self class]];
	for (id obj in allObjectsInClass) {
		[obj checkForContent];			
		[storeToUpgrade willUpdateObject:obj];
	}
	
	// 2. Upgrade class metadata -versionNumber and class -version to match -currentClassVersionNumber
	unsigned char currentVersion = [[self class] currentClassVersionNumber];
	
	BNRClassMetaData *metaData = [storeToUpgrade metaDataForClass:[self class]];
	[metaData setVersionNumber:currentVersion];
	[[self class] setVersion:currentVersion];
	
	// 3. Write each object back out according to how its -writeContentToBuffer behaves for its new post-upgrade -versionNumber.
	//
	// Note if this gets called on a class whose implmentation hasn't actually changed 
	// (i.e. [aClass version] == [aClass currentVersionNumber]), nothing bad happens. The only effect of the "upgrade" 
	// is the inefficiency of reading and writing every object plus the updated file mod dates.
	
	NSError *error = nil;
	if (![storeToUpgrade saveChanges:&error]) {
		// FIXME: Needs error sheet?

		NSLog( @"Failed to upgrade class %@ instances to version:%d, error:%@", NSStringFromClass([self class]), currentVersion, [error localizedDescription] );
		return NO;
	}
	return YES;
}

// NOT the designated initializer, just a convenience for BNRStore.
- (id)initWithStore:(BNRStore *)s rowID:(UInt32)n buffer:(BNRDataBuffer *)buffer {
    self = [self init];
    if (self) {
		store = s;
		rowID = n;
		if (nil != buffer) {
			
			if ([s usesPerInstanceVersioning]) {
				[buffer consumeVersion];
			}
			
			[self readContentFromBuffer:buffer];
			// Retain count of 1 + 1 for hasContent
			status = 3;
		} else {
			// Retain count of 1 
			status = 2;
		}		
	}
    return self;
}

- (id)init
{
    self = [super init];
    if (self) {
    	// Retain count of 1 + hasContent
    	status = 3;
    }
    return self;
}

- (void)setStore:(BNRStore *)s
{
    store = s;
}

- (BNRStore *)store
{
    return store;
}

#pragma mark Getting data in and out
- (void)readContentFromBuffer:(BNRDataBuffer *)d
{
    // NOOP, must be overridden by subclass
}
- (void)writeContentToBuffer:(BNRDataBuffer *)d
{
    // NOOP, must be overridden by subclass
}

- (UInt8)writeVersion
{
    return (UInt8)[[self class] version];
}

- (void)dissolveAllRelationships
{
    // NOOP, may be overridden by subclass
}
- (void)prepareForDelete
{
    // NOOP, may be overridden by subclass 
}

+ (NSSet *)textIndexedAttributes
{
    return nil;
}

- (UInt32)rowID
{
    return rowID;
}
- (void)setRowID:(UInt32)n
{
    rowID = n;
}

- (BOOL)hasContent
{
    return status % 2;
}
- (void)setHasContent:(BOOL)yn
{
    // Do I currently have content?
    if ([self hasContent]) {
        if (yn == NO) status--;  // just lost content
    } else if (yn == YES) status++;  // just gained content
}
- (void)fetchContent
{
    if (0U == rowID) return;

    BNRStore *s = [self store];
    BNRStoreBackend *backend = [s backend];
    BNRDataBuffer *d = [backend dataForClass:[self class]
                                       rowID:[self rowID]];
    if (!d) return;
    
    BNRClassMetaData *metaData = [s metaDataForClass:[self class]];
    [d decryptWithKey:[s encryptionKey] salt:[metaData encryptionKeySalt]];

    if ([s usesPerInstanceVersioning]) {
        [d consumeVersion];
    }
    [self readContentFromBuffer:d];
    [self setHasContent:YES];
}

- (void)checkForContent
{
    if (![self hasContent]) [self fetchContent];
}

- (NSUInteger)retainCount
{
    return status / 2;
}

- (id)retain
{
    status += 2;
    return self;
}

// Called if the store is deallocated before the stored object
- (void)clearStore
{
    [self setStore:nil];
}

- (void)dealloc
{
#if kUseBNRResizableUniquingTable
    BNRResizableUniquingTable *uniquingTable = [store uniquingTable];
#else
    BNRUniquingTable *uniquingTable = [store uniquingTable];
#endif
    [uniquingTable removeObjectForClass:[self class] rowID:[self rowID]];
    [super dealloc];
}

- (NSString *)description
{
    return [NSString stringWithFormat:@"<%@: rowID = %u>", 
            NSStringFromClass([self class]), (unsigned int)[self rowID]];
}

- (void)logDescription
{
	NSLog(@"\n%@\n=========================================", [self description]);
}

- (oneway void)release
{
    status -= 2;
    if (status < 2) [self dealloc];
}

@end
