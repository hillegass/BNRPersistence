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

#import "BNRStore.h"
#import "BNRStoreBackend.h"
#import "BNRStoredObject.h"
#import "BNRClassDictionary.h"
#import "BNRBackendCursor.h"
#import "BNRDataBuffer.h"
#import "BNRClassMetaData.h"
#import "BNRIndexManager.h"
#import "BNRDataBuffer+Encryption.h"

#if kUseBNRResizableUniquingTable
	#import "BNRResizableUniquingTable.h"
#else
	#import "BNRUniquingTable.h"
#endif

#ifndef PAGE_SIZE
#define PAGE_SIZE (4096)
#endif

@interface BNRStoredObject (BNRStoreFriend)

- (void)setHasContent:(BOOL)yn;
- (id)initWithStore:(BNRStore *)s rowID:(UInt32)n buffer:(BNRDataBuffer *)buffer;

@end


@interface BNRStore (FilePresenter)
// iCloud - currently disabled; full support requires iCloudBNRStoreSupportEnabled and more testing
- (void)registerAsFilePresenter;
- (void)unregisterAsFilePresenter;
- (BOOL)isRegisteredAsFilePresenter; 
@end


@implementation BNRStore
@synthesize undoManager, indexManager, delegate, usesPerInstanceVersioning, encryptionKey;

- (id)init
{
    self = [super init];
    if (self) {
#if kUseBNRResizableUniquingTable	// see size/speed tradeoffs in BNRResizableUniquingTable. kUseBNRResizableUniquingTable will be required for ARC.
        uniquingTable = [[BNRResizableUniquingTable alloc] init];
#else
		uniquingTable = [[BNRUniquingTable alloc] init];
#endif
		if (!uniquingTable) {
			return nil;
		}
		toBeInserted = [[NSMutableSet alloc] init];
		toBeDeleted = [[NSMutableSet alloc] init];
		toBeUpdated = [[NSMutableSet alloc] init];
		classMetaData = [[BNRClassDictionary alloc] init];
		usesPerInstanceVersioning = YES; // Adds an 8-bit number to every record, but enables versioning...
    }
    return self;
}

- (void)setDelegate:(id <BNRStoreDelegate>)obj
{
    delegate = obj;
}

- (void)setUndoManager:(NSUndoManager *)ud
{
    [ud retain];
    [undoManager release];
    undoManager = ud;
}

- (void)makeEveryStoredObjectPerformSelector:(SEL)s
{
    [uniquingTable makeAllObjectsPerformSelector:s];
}

- (void)dissolveAllRelationships
{
    [self makeEveryStoredObjectPerformSelector:@selector(dissolveAllRelationships)];
}

- (void)dealloc
{
	[self unregisterAsFilePresenter];
	
    [uniquingTable release];
    [toBeInserted release];
    [toBeDeleted release];
    [toBeUpdated release];
    
    [indexManager close];
    [indexManager release];
    
    [backend release];
    [classMetaData release];
	
	[undoManager release];		// added BMonk 4/2/11
	[encryptionKey release];	// added BMonk 4/2/11
	
    [super dealloc];
}
    
- (void)addClass:(Class)c
{    
    // Put it in the first empty slot
    int classCount = 0;
    while (classes[classCount] != NULL) {
        classCount++;
        
        if (classCount >= 255) {
            [NSException raise:@"BNRStore classes array is full"
                        format:@"Class %@ was not added to %@", NSStringFromClass(c), self];
        }
    }
    classes[classCount] = c;
}

- (BOOL)decryptBuffer:(BNRDataBuffer *)buffer ofClass:(Class)c rowID:(UInt32)rowID
{
    if (buffer == nil)
        return YES;
    
    BNRClassMetaData *metaData = [self metaDataForClass:c];
    
    UInt32 salt[2];
    memcpy(salt, [metaData encryptionKeySalt], 8);
    salt[1] = salt[1] ^ rowID;
    
    return [buffer decryptWithKey:encryptionKey salt:salt];
}
- (void)encryptBuffer:(BNRDataBuffer *)buffer ofClass:(Class)c rowID:(UInt32)rowID
{
    UInt32 salt[2];
    memcpy(salt, [[self metaDataForClass:c] encryptionKeySalt], 8);
    salt[1] = salt[1] ^ rowID;
    [buffer encryptWithKey:encryptionKey salt:salt]; // does not encrypt if encryptionKey is empty.
}

#pragma mark Fetching

- (BNRStoredObject *)objectForClass:(Class)c 
                              rowID:(UInt32)n 
                       fetchContent:(BOOL)mustFetch
{
    
    // Try to find it in the uniquing table
    BNRStoredObject *obj = [uniquingTable objectForClass:c rowID:n];

    if (obj) {
        if (mustFetch && ![obj hasContent]) {
            BNRDataBuffer *const d = [backend dataForClass:c rowID:n];
            [self decryptBuffer:d ofClass:c rowID:n];
            if (usesPerInstanceVersioning) {
                [d consumeVersion];
            }
            [obj readContentFromBuffer:d];
            [obj setHasContent:YES];
        }
    } else {
        BNRDataBuffer *const d = mustFetch? [backend dataForClass:c rowID:n] : nil;
        [self decryptBuffer:d ofClass:c rowID:n];
        if (usesPerInstanceVersioning) {
            [d consumeVersion];
        }
        obj = [[[c alloc] initWithStore:self rowID:n buffer:d] autorelease];
        [uniquingTable setObject:obj forClass:c rowID:n];
    }
    return obj;
}

- (NSMutableArray *)allObjectsForClass:(Class)c
{
    // Fetch!
    BNRBackendCursor *const cursor = [backend cursorForClass:c];
    if (!cursor) {
        NSLog(@"No database for %@", NSStringFromClass(c));
        return nil;
    }
    NSMutableArray *const allObjects = [NSMutableArray array];
    BNRDataBuffer *const buffer = [[[BNRDataBuffer alloc]
                                    initWithCapacity:(UINT16_MAX + 1)]
                                   autorelease];

    UInt32 rowID;
    while ((rowID = [cursor nextBuffer:buffer]) != 0)
    {
        if (kBNRMetadataRowID == rowID) continue;  // skip metadata

        // Get the next object.
        BNRStoredObject *storedObject = [self objectForClass:c
                                                       rowID:rowID
                                                fetchContent:NO];
        [allObjects addObject:storedObject];
        // Possibly read in its stored data.
        const BOOL hasUnsavedData = [toBeUpdated containsObject:storedObject];
        if (!hasUnsavedData) {
            [self decryptBuffer:buffer ofClass:c rowID:rowID];
            if (usesPerInstanceVersioning) {
                [buffer consumeVersion];
            }
            [storedObject readContentFromBuffer:buffer];
            [storedObject setHasContent:YES];
        }
     }
    return allObjects;
}

#if NS_BLOCKS_AVAILABLE
- (void)enumerateAllObjectsForClass:(Class)c usingBlock:(BNRStoredObjectIterBlock)iterBlock
{
    // Fetch!
    BNRBackendCursor *const cursor = [backend cursorForClass:c];
    if (!cursor) {
        NSLog(@"No database for %@", NSStringFromClass(c));
        return;
    }

    BNRDataBuffer *const buffer = [[[BNRDataBuffer alloc]
                                    initWithCapacity:(UINT16_MAX + 1)]
                                   autorelease];
    
    UInt32 rowID;
    while ((rowID = [cursor nextBuffer:buffer]) != 0)
    {
        if (kBNRMetadataRowID == rowID) continue;  // skip metadata
        
        // Prevent our usage from building up while iterating over the objects:
        NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
        
        // Get the next object.
        BNRStoredObject *storedObject = [self objectForClass:c
                                                       rowID:rowID
                                                fetchContent:NO];
        // Possibly read in its stored data.
        const BOOL hasUnsavedData = [toBeUpdated containsObject:storedObject];
        if (!hasUnsavedData) {
            [self decryptBuffer:buffer ofClass:c rowID:rowID];
            if (usesPerInstanceVersioning) {
                [buffer consumeVersion];
            }
            [storedObject readContentFromBuffer:buffer];
            [storedObject setHasContent:YES];
        }
        
        BOOL stop = NO;
        iterBlock(rowID, storedObject, &stop);
        
        [pool drain];
        
        if (stop)
            break;
    }
}
#endif

- (NSMutableArray *)objectsForClass:(Class)c
                       matchingText:(NSString *)toMatch
                             forKey:(NSString *)key
{
    if (!indexManager) {
        NSLog(@"No fulltext search without an index manager");
        return nil;
    }
    
    UInt32 *indexResult;
    
    UInt32 rowCount = [indexManager countOfRowsInClass:c 
                                          matchingText:toMatch
                                                forKey:key
                                                  list:&indexResult];
    
    NSMutableArray *result = [NSMutableArray array];
    for (UInt32 i = 0; i < rowCount; i++) {
        UInt32 rowID = indexResult[i];
        BNRStoredObject *obj = [self objectForClass:c 
                                              rowID:rowID
                                       fetchContent:NO];
        [result addObject:obj];
    }
    if (rowCount > 0) {
        free(indexResult);
    }
    return result;
}

#pragma mark Insert, update, delete

- (BOOL)hasUnsavedChanges
{
    return [toBeDeleted count] || [toBeInserted count] || [toBeUpdated count];
}

- (void)insertObject:(BNRStoredObject *)obj
{
    [obj setStore:self];
    
    Class c = [obj class];
    UInt32 rowID = [obj rowID];
    
    // Should I really be giving this object
    // a row ID?
    if (rowID == 0) {
        rowID = [self nextRowIDForClass:c];
        [obj setRowID:rowID];
    }
    
    // Put it in the uniquing table
    [uniquingTable setObject:obj forClass:c rowID:rowID];

    if (undoManager) {
        [(BNRStore *)[undoManager prepareWithInvocationTarget:self] deleteObject:obj];
    }
    
    [self willChangeValueForKey:@"hasUnsavedChanges"];
    [toBeInserted addObject:obj];
    [toBeUpdated removeObject:obj];
    [toBeDeleted removeObject:obj];
    [self didChangeValueForKey:@"hasUnsavedChanges"];
    
    if (delegate) {
        [delegate store:self willInsertObject:obj];
    }
}

// This insert is used when undoing a delete
- (void)insertWithRowID:(unsigned)rowID
                  class:(Class)c
               snapshot:(BNRDataBuffer *)snap
{
    BNRStoredObject *obj = [self objectForClass:c
                                          rowID:rowID
                                   fetchContent:NO];
    if (usesPerInstanceVersioning) {
        [snap consumeVersion];
    }
    
    [obj readContentFromBuffer:snap];
    [self insertObject:obj];
   // [obj release];              // XCode4 analyzer complains incorrect decrement; object is already autoreleased - BMonk 7/30/11
}

- (void)deleteObject:(BNRStoredObject *)obj
{
    // Prevents infinite recursion (see prepareForDelete)
    if ([toBeDeleted containsObject:obj]) {
        return;
    }
    
    [self willChangeValueForKey:@"hasUnsavedChanges"];
    
    if ([toBeInserted containsObject:obj]){
        [toBeInserted removeObject:obj];
    } else {
        [toBeDeleted addObject:obj];
    }
    
    // No need to insert or update deleted objects
    [toBeUpdated removeObject:obj];

    [self didChangeValueForKey:@"hasUnsavedChanges"];
    
    // Store away current values
    if (undoManager) {
        BNRDataBuffer *snapshot = [[BNRDataBuffer alloc]
                                   initWithCapacity:PAGE_SIZE];
        if (usesPerInstanceVersioning) {
            [snapshot writeVersionForObject:obj];
        }
        
        [obj writeContentToBuffer:snapshot];
        [snapshot resetCursor];
        //NSLog(@"snapshot for undo = %@", snapshot);

        unsigned rowID = [obj rowID];
        Class c = [obj class];
        [[undoManager prepareWithInvocationTarget:self] insertWithRowID:rowID
                                                                  class:c
                                                               snapshot:snapshot];
        [snapshot release];
    }
    
    // objects implement their own delete rules - cascade, whatever
    [obj prepareForDelete];
    
    if (delegate) {
        [delegate store:self willDeleteObject:obj];
    }
}

- (void)updateObject:(BNRStoredObject *)obj withSnapshot:(BNRDataBuffer *)b
{
    // Store away current values
    if (undoManager) {
        BNRDataBuffer *snapshot = [[BNRDataBuffer alloc]
                                   initWithCapacity:PAGE_SIZE];
        if (usesPerInstanceVersioning) {
            [snapshot writeVersionForObject:obj];
        }
        
        [obj writeContentToBuffer:snapshot];
        [snapshot resetCursor];
        [[undoManager prepareWithInvocationTarget:self] updateObject:obj 
                                                        withSnapshot:snapshot];
        [snapshot release];
    }
    
    if (usesPerInstanceVersioning) {
        [b consumeVersion];
    }
    
    [obj readContentFromBuffer:b];
    if (delegate) {
        [delegate store:self didChangeObject:obj];
    }
    
    [toBeUpdated addObject:obj];
    if (delegate) {
        [delegate store:self willUpdateObject:obj];
    }
    
}

- (void)willUpdateObject:(BNRStoredObject *)obj
{
    if (undoManager) {
        BNRDataBuffer *snapshot = [[BNRDataBuffer alloc]
                                   initWithCapacity:PAGE_SIZE];
        if (usesPerInstanceVersioning) {
            [snapshot writeVersionForObject:obj];
        }
        
        [obj writeContentToBuffer:snapshot];
        [snapshot resetCursor];

        [[undoManager prepareWithInvocationTarget:self] updateObject:obj 
                                                        withSnapshot:snapshot];
        [snapshot release];
    }
    
    if (delegate) {
        [delegate store:self willUpdateObject:obj];
    }
    
    // No need to insert and update
    if (![toBeInserted containsObject:obj]) {
        [self willChangeValueForKey:@"hasUnsavedChanges"];
        [toBeUpdated addObject:obj];
        [self didChangeValueForKey:@"hasUnsavedChanges"];
    }
}

//MARK: Transaction support

- (NSSet *)classesInvolvedInSave
{
	NSMutableSet *classesInvolvedInSave = [NSMutableSet set];
			
	for (BNRStoredObject *obj in toBeInserted) {
        Class c = [obj class];
		[classesInvolvedInSave addObject:c];
	}
	
	for (BNRStoredObject *obj in toBeUpdated) {
        Class c = [obj class];
		[classesInvolvedInSave addObject:c];
	}
	
	for (BNRStoredObject *obj in toBeDeleted) {
        Class c = [obj class];
		[classesInvolvedInSave addObject:c];
	}
	
	// Instead of the above, could use 
	// [classesInvolvedInSave addObjectsFromArray:[toBeInserted valueForKey:@"class"]];
	// [classesInvolvedInSave addObjectsFromArray:[toBeUpdated valueForKey:@"class"]];
	// [classesInvolvedInSave addObjectsFromArray:[toBeDeleted valueForKey:@"class"]];
	// but I suppose it's no faster, and is more brittle.
	
	// Another approach: create a union of the BNRStoredObjects in toBe<Inserted,Updated,Deleted>
	// and then use a single loop to extract the Classes, but once again the speed
	// difference is likely negligible: most saves only involve a few stored objects.

	return [[classesInvolvedInSave copy] autorelease];
}

- (NSSet *)beginTransaction
{
#if iCloudBNRStoreSupportEnabled
	if (NSClassFromString(@"NSFileCoordinator")) {
		coordinator = [[NSClassFromString(@"NSFileCoordinator") alloc] performSelector:@selector(initWithFilePresenter:) withObject:self];
	}
#endif
	
	NSSet *affectedClasses = [self classesInvolvedInSave];
	[backend beginTransactionForClasses:affectedClasses];
	return affectedClasses;
}

- (BOOL)commitTransaction
{
	BOOL committedOK = [backend commitTransaction];
	
#if iCloudBNRStoreSupportEnabled
	if (committedOK) {
		[coordinator release];
	}
#endif
	
	return committedOK;
}

- (BOOL)abortTransaction
{
	BOOL result = [backend abortTransaction];;
	
#if iCloudBNRStoreSupportEnabled
	[coordinator release];
#endif
	
	return result;
}

// MARK: Saving

- (BOOL)saveChanges:(NSError **)errorPtr
{
    [self willChangeValueForKey:@"hasUnsavedChanges"];

    BNRDataBuffer *buffer = [[BNRDataBuffer alloc] initWithCapacity:65536];
    NSSet *affectedClasses = [self beginTransaction];
    
	// Inserts
    //NSLog(@"inserting %d objects", [toBeInserted count]);
    for (BNRStoredObject *obj in toBeInserted) {
        Class c = [obj class];
        UInt32 rowID = [obj rowID];
        
        if (usesPerInstanceVersioning) {
            [buffer writeVersionForObject:obj];
        }        
        
        [obj writeContentToBuffer:buffer];
        
        [self encryptBuffer:buffer ofClass:c rowID:rowID]; // does not encrypt if encryptionKey is empty.
        
        [backend insertData:buffer
                   forClass:c
                      rowID:rowID];
        [buffer clearBuffer];
        
        if (indexManager) {
            [indexManager insertObjectInIndexes:obj];
        }
        
    }
    
    // Updates
    //NSLog(@"updating %d objects", [toBeUpdated count]);
    for (BNRStoredObject *obj in toBeUpdated) {
        Class c = [obj class];
        UInt32 rowID = [obj rowID];
        if (usesPerInstanceVersioning) {
            [buffer writeVersionForObject:obj];
        }
        
        [obj writeContentToBuffer:buffer];
        
        [self encryptBuffer:buffer ofClass:c rowID:rowID]; // does not encrypt if encryptionKey is empty.
        
        [backend updateData:buffer
                   forClass:c
                      rowID:rowID];
        [buffer clearBuffer];
        
        // FIXME: updating all indexes is inefficient
        if (indexManager) {
            [indexManager updateObjectInIndexes:obj];
        }
        
    }
    
    // Deletes
    //NSLog(@"deleting %d objects", [toBeDeleted count]);
    for (BNRStoredObject *obj in toBeDeleted) {
        Class c = [obj class];
        UInt32 rowID = [obj rowID];
        
        // Take it out of the uniquing table:
        // Should I remove it from the uniquingTable in deleteObject?
        [uniquingTable removeObjectForClass:c rowID:rowID];
        [obj setStore:nil];

        [backend deleteDataForClass:c
                              rowID:rowID];
        
        if (indexManager) {
            [indexManager deleteObjectFromIndexes:obj];
        }
        
    }
    
	
	// Update metadata
	for (Class c in affectedClasses) {
        BNRClassMetaData *d = [classMetaData objectForClass:c];
        if (d) {
            [d writeContentToBuffer:buffer];
            //NSLog(@"Inserting %d bytes of meta data for %@", [buffer length], NSStringFromClass(c));

            [backend updateData:buffer
                       forClass:c
                          rowID:1];
            [buffer clearBuffer];
        }
    }
	
    [buffer release];
    
    BOOL successful = [self commitTransaction];
    if (successful) {
        [toBeInserted removeAllObjects];
        [toBeUpdated removeAllObjects];
        [toBeDeleted removeAllObjects];
    } else {
        NSLog(@"Error: save was not successful");
        [self abortTransaction];
    }
    
    [self didChangeValueForKey:@"hasUnsavedChanges"];        
    return successful;
}

#pragma mark Backend

- (BNRStoreBackend *)backend
{
    return backend;
}
- (void)setBackend:(BNRStoreBackend *)be
{
	[self unregisterAsFilePresenter];
    [be retain];
    [backend release];
    backend = be;
	[self registerAsFilePresenter];
}

#pragma mark Class meta data

- (BNRClassMetaData *)metaDataForClass:(Class)c
{
    BNRClassMetaData *md = [classMetaData objectForClass:c];
    if (!md) {
        md = [[BNRClassMetaData alloc] init];
        BNRDataBuffer *b;
        b = [backend dataForClass:c 
                            rowID:1];
        
        // Did I find meta data in the database?
        if (b) {
            
            // Note: meta data data buffer is *not* prepended with a version #
            //NSLog(@"Read %d bytes of meta data for %@", [b length], NSStringFromClass(c));
            [md readContentFromBuffer:b];
            unsigned char classID = [md classID];
            classes[classID] = c;
        } else {
            unsigned char classID = 0;
            while (classes[classID] != c) {
                classID++;
                if (classID == 255) {
                    [NSException raise:@"Class not in classes"
                                format:@"Class %@ was not added to %@", NSStringFromClass(c), self];
                }
            }
            [md setClassID:classID];
        }
        [classMetaData setObject:md forClass:c];
        [md release];
    }
    return md;
}

- (unsigned)nextRowIDForClass:(Class)c
{
    BNRClassMetaData *md = [self metaDataForClass:c];
    return [md nextPrimaryKey];
}
- (unsigned char)versionForClass:(Class)c
{
    BNRClassMetaData *md = [self metaDataForClass:c];
    return [md versionNumber];
}
- (Class)classForClassID:(unsigned char)c
{
    return classes[c];
}
- (unsigned char)classIDForClass:(Class)c
{
    BNRClassMetaData *md = [self metaDataForClass:c];
    return [md classID];
}

- (NSString *)description
{
    return [NSString stringWithFormat:@"<BNRStore-%@ to insert:%lu, to update %lu, to delete %lu>",
        backend, (unsigned long)[toBeInserted count], (unsigned long)[toBeUpdated count], (unsigned long)[toBeDeleted count]];
}

//MARK: Bulk upgrade

// alternate approach to BNRStoredObject's -upgradeInstancesinStore to achieve the same result
// Call example: [myBNRStore upgradeBNRStoredObjectClass:[MyBNRStoredObject class]];
- (BOOL)upgradeAllBNRStoredObjectsOfClass:(Class)classToUpgrade {
	
	// 1. Load each object according to how its -readContentFromBuffer behaves for its pre-upgraded -versionNumber
	
	// For huge numbers of objects, this could become an issue. May need to load in chunks.
	NSArray *allObjectsInClass = [self allObjectsForClass:classToUpgrade];
	for (id obj in allObjectsInClass) {
		[obj checkForContent];			
		[self willUpdateObject:obj];
	}
	
	// 2. Upgrade class metadata -versionNumber and class -version to match class's -currentClassVersionNumber
	unsigned char currentVersion = [classToUpgrade currentClassVersionNumber];
	
	BNRClassMetaData *metaData = [self metaDataForClass:classToUpgrade];
	[metaData setVersionNumber:currentVersion];
	[classToUpgrade setVersion:currentVersion];
	
	// 3. Write each object back out, according to how its -writeContentToBuffer behaves for its new, post-upgrade -versionNumber.
	//
	// Note if this gets called on a class whose implmentation hasn't actually changed, 
	// (i.e. its version == its currentVersionNumber), nothing bad happens. The only effect of the "upgrade" 
	// is the inefficiency of reading and writing each object, along with updated file mod dates on disk.
	
	NSError *error = nil;
	if (![self saveChanges:&error]) {
		// FIXME: Needs error sheet

		NSLog( @"Failed to upgrade class %@ instances to version:%d, error:%@", NSStringFromClass([self class]), currentVersion, [error localizedDescription] );
		return NO;
	}
	return YES;
}

- (void)upgradeAllObjectsForAnyClassesNeedingUpgrade
{
    int classCount = 0;
    while (classes[classCount] != NULL) {
		
		Class c = classes[classCount];
		unsigned char oldVersion = [self versionForClass:c];
		if (oldVersion < [c currentClassVersionNumber]) 
		{
			BOOL success = [self upgradeAllBNRStoredObjectsOfClass:c];
			if (!success) {
				NSString *failureReason = [NSString stringWithFormat:@"Schema of class \"%@\" needs upgrade; metaData versionNumber:%d currentClassVersionNumber:%c", 
								NSStringFromClass(c),
								oldVersion, 
								[c currentClassVersionNumber] 
								];
				NSLog(@"%@", failureReason);
				@throw [NSException exceptionWithName:@"DB Schema Error" 
											   reason:failureReason
											 userInfo:nil];
			}
		}
		
		classCount++;
	}
}

#if 0 // NS_BLOCKS_AVAILABLE
- (void)upgradeAllObjectsForClassesNeedingUpgradeUsingBlock:(BNRStoredObjectIterBlock)iterBlock
{
	
}
#endif


//MARK: logging utils

- (void)logAllObjects
{
    [self makeEveryStoredObjectPerformSelector:@selector(logDescription)];
}

- (void)logUniquingTable 
{
	[uniquingTable logTable];
}


//MARK: iCloud suport

-(void)registerAsFilePresenter {
#if iCloudBNRStoreSupportEnabled
	if ([backend respondsToSelector:@selector(path)] && ![self isRegisteredAsFilePresenter] && NSClassFromString(@"NSFileCoordinator")) {
		[NSClassFromString(@"NSFileCoordinator") performSelector:@selector(addFilePresenter:) withObject:self];
	}
#endif
}

-(void)unregisterAsFilePresenter {
#if iCloudBNRStoreSupportEnabled
	if ([self isRegisteredAsFilePresenter] && NSClassFromString(@"NSFileCoordinator")) {
		[NSClassFromString(@"NSFileCoordinator") performSelector:@selector(removeFilePresenter:) withObject:self];
	}
#endif
}

 - (BOOL)isRegisteredAsFilePresenter {
#if iCloudBNRStoreSupportEnabled
	 if (NSClassFromString(@"NSFileCoordinator")) {
		 NSArray *presenters = [NSClassFromString(@"NSFileCoordinator") performSelector:@selector(filePresenters)];
		 for (id<NSFilePresenter>obj in presenters) {
			 if (obj == self) {
				 return YES;
			 }
		 }
	 }
#endif
	 return NO;
 }

// MARK: NSFilePresenter protocol methods
#if iCloudBNRStoreSupportEnabled
		 
- (NSURL *)presentedItemURL {
	NSLog(@"%@", @"presentedItemURL");
	if ([self backend] respondsToSelector:@selector(path)) {
		NSString *path = [(BNRTCBackend *)[self backend] path];
		return [NSURL fileURLWithPath:path isDirectory:YES];;
	}
	return nil;
}

- (NSOperationQueue *)presentedItemOperationQueue {
	NSLog(@"%@", @"presentedItemOperationQueue");
	return [NSOperationQueue mainQueue];
}

- (void)accommodatePresentedItemDeletionWithCompletionHandler:(void (^)(NSError *errorOrNil))completionHandler
{
	NSLog(@"%@", @"accommodatePresentedItemDeletionWithCompletionHandler");
	NSError *error = [NSError errorWithDomain:@"BNRStoreDomain" code:-1 userInfo:[NSDictionary dictionaryWithObject:@"BNRStore not yet ready to accomodate coordinated deletion" forKey:NSLocalizedDescriptionKey]];
	// pass nil to indicate success
	completionHandler(error);
}

- (void)accommodatePresentedSubitemDeletionAtURL:(NSURL *)url completionHandler:(void (^)(NSError *errorOrNil))completionHandler
{
	NSLog(@"%@ at URL:%@", @"accommodatePresentedSubitemDeletionAtURL", [url description]);
	NSError *error = [NSError errorWithDomain:@"BNRStoreDomain" code:-2 userInfo:[NSDictionary dictionaryWithObject:@"BNRStore not yet ready to accomodate coordinated subitem deletion" forKey:NSLocalizedDescriptionKey]];
	// pass nil to indicate success
	completionHandler(error);
}

- (void)presentedItemDidChange {
	NSLog(@"%@", @"presentedItemDidChange");
}

- (void)presentedItemDidGainVersion:(NSFileVersion *)version {
	NSLog(@"presentedItemDidGainVersion:%@", [version description]);
}

- (void)presentedItemDidLoseVersion:(NSFileVersion *)version {
	NSLog(@"presentedItemDidLoseVersion:%@", [version description]);
}

- (void)presentedItemDidMoveToURL:(NSURL *)newURL {
	NSLog(@"presentedItemDidMoveToURL:%@", [newURL description]);
}

- (void)presentedItemDidResolveConflictVersion:(NSFileVersion *)version {
	NSLog(@"presentedItemDidResolveConflictVersion:%@", [version description]);
}

- (void)presentedSubitemAtURL:(NSURL *)url didGainVersion:(NSFileVersion *)version {
	NSLog(@"presentedSubitemAtURL:%@ didGainVersion:%@", [url description], [version description]);
}

- (void)presentedSubitemAtURL:(NSURL *)url didLoseVersion:(NSFileVersion *)version {
	NSLog(@"presentedSubitemAtURL:%@ didLoseVersion:%@", [url description], [version description]);
}

- (void)presentedSubitemAtURL:(NSURL *)oldURL didMoveToURL:(NSURL *)newURL {
	NSLog(@"presentedSubitemAtURL:%@ didMoveToURL:%@", [oldURL description], [newURL description]);
}

- (void)presentedSubitemAtURL:(NSURL *)url didResolveConflictVersion:(NSFileVersion *)version {
	NSLog(@"presentedSubitemAtURL:%@ didResolveConflictVersion:%@", [url description], [version description]);
}

- (void)presentedSubitemDidAppearAtURL:(NSURL *)url {
	NSLog(@"presentedSubitemDidAppearAtURL:%@", [url description]);
}

- (void)presentedSubitemDidChangeAtURL:(NSURL *)url {
	NSLog(@"presentedSubitemDidChangeAtURL:%@", [url description]);
}

- (void)relinquishPresentedItemToReader:((void (^)(void (^reacquirer)(void))))reader {
	NSLog(@"%@", @"relinquishPresentedItemToReader");
    // Prepare for another object to read the file.
	//self.fileIsWritable = NO;
	
	// Now let the reader know that it can have the file.
	// But pass a reacquisition block so that this object
	// can update itself when the reader is done.
	reader(^{
		NSLog(@"%@", @"read reacquirer called");
		//self.fileIsWritable = YES;
	});
}

- (void)relinquishPresentedItemToWriter:((void (^)(void (^reacquirer)(void))))writer {
	NSLog(@"%@", @"relinquishPresentedItemToWriter");
	// Prepare for another object to write to the file.
	//self.fileIsWritable = NO;
	
	// Now let the writer know that it can have the file.
	// But pass a reacquisition block so that this object
	// can update itself when the writer is done.
	writer(^{
		NSLog(@"%@", @"write reacquirer called");
		//self.fileIsWritable = YES;
	});
}

- (void)savePresentedItemChangesWithCompletionHandler:(void (^)(NSError *errorOrNil))completionHandler {
	NSLog(@"%@", @"savePresentedItemChangesWithCompletionHandler");
	
	NSError *error = [NSError errorWithDomain:@"BNRStoreDomain" code:-3 userInfo:[NSDictionary dictionaryWithObject:@"BNRStore not yet ready for savePresentedItemChangesWithCompletionHandler" forKey:NSLocalizedDescriptionKey]];
	// pass nil to indicate success
	completionHandler(error);
}

#endif // iCloudBNRStoreSupportEnabled


@end
