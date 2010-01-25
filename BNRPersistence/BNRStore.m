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
#import "BNRIntDictionary.h"
#import "BNRBackendCursor.h"
#import "BNRDataBuffer.h"
#import "BNRClassMetaData.h"

@interface BNRStoredObject (BNRStoreFriend)

- (void)setHasContent:(BOOL)yn;

@end


@implementation BNRStore

@synthesize savesDataForSync;

- (id)init
{
    [super init];
    uniquingTable = [[BNRClassDictionary alloc] init];
    toBeInserted = [[NSMutableSet alloc] init];
    toBeDeleted = [[NSMutableSet alloc] init];
    toBeUpdated = [[NSMutableSet alloc] init];
    classMetaData = [[BNRClassDictionary alloc] init];
    savesDataForSync = NO;
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
    NSEnumerator *e = [uniquingTable objectEnumerator];
    BNRIntDictionary *currentTable;
    while (currentTable = [e nextObject]){
        [currentTable makeEveryObjectPerformSelector:s];
    }
}

- (void)dissolveAllRelationships
{
    [self makeEveryStoredObjectPerformSelector:@selector(dissolveAllRelationships)];
}

- (void)dealloc
{
    [self makeEveryStoredObjectPerformSelector:@selector(clearStore)];
    [uniquingTable release];
    [toBeInserted release];
    [toBeDeleted release];
    [toBeUpdated release];
    [backend release];
    [classMetaData release];
    [super dealloc];
}
    
- (void)addClass:(Class)c expectedCount:(UInt32)eCount
{
    // If zero, do something reasonable
    if (eCount == 0) {
        eCount = 50000;
    }
    BNRIntDictionary *u = [[BNRIntDictionary alloc] initWithExpectedCount:eCount];
    [uniquingTable setObject:u forClass:c];
    [u release];
    
    // Put it in the first empty slot
    int classCount = 0;
    while (classes[classCount] != NULL) {
        classCount++;
    }
    classes[classCount] = c;
}

#pragma mark Fetching

- (BNRStoredObject *)objectForClass:(Class)c 
                              rowID:(UInt32)n 
                       fetchContent:(BOOL)mustFetch
{
    
    // Try to find it in the uniquing table
    BNRIntDictionary *u = [uniquingTable objectForClass:c];
    NSAssert(u != nil, @"No uniquing table for %@", NSStringFromClass(c));
    BNRStoredObject *obj = [u objectForInt:n];

    if (!obj) {
        obj = [[c alloc] init];
        [obj setRowID:n];
        [u setObject:obj forInt:n];
        [obj setStore:self];
        [obj setHasContent:NO];
        [obj autorelease];
    }
    
    // Can I skip the actual fetch?
    if ((mustFetch == NO) || ([obj hasContent])) {
        return obj;
    }
    
    // Fetch!
    BNRDataBuffer *d = [backend dataForClass:c 
                                       rowID:n];
    [obj readContentFromBuffer:d];
    [obj setHasContent:YES];
    return obj;
}

- (NSMutableArray *)allObjectsForClass:(Class)c
{
    // Fetch!
    BNRBackendCursor *cursor = [backend cursorForClass:c];
    NSMutableArray *result = [NSMutableArray array];
    BNRDataBuffer *d = [[BNRDataBuffer alloc] initWithCapacity:65536];

    // FIXME: With clever use of threads, the work of this
    // loop could be done at least twice as fast.
    // Put nextBuffer: in one thread 
    // Put objectForClass:rowID:fetchContent: in another
    // Put readContentFromBuffer in another
    UInt32 rowID;
    while (rowID = [cursor nextBuffer:d]) 
    {
        // Is this the metadata record?
        if (rowID == 1) {
            continue;
        }
        BNRStoredObject *obj = [self objectForClass:c 
                                              rowID:rowID
                                       fetchContent:NO];
        // Don't overwrite unsaved data in a dirty object
        if (![toBeUpdated containsObject:obj]) {
            [obj readContentFromBuffer:d];
            [obj setHasContent:YES];
        }
        [result addObject:obj];
     }
    [d release];
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
    BNRIntDictionary *u = [uniquingTable objectForClass:c];
    [u setObject:obj forInt:rowID];

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
    [obj readContentFromBuffer:snap];
    [self insertObject:obj];
    [obj release];
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
        
        BNRDataBuffer *snapshot = [[BNRDataBuffer alloc] initWithCapacity:4096];
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
        BNRDataBuffer *snapshot = [[BNRDataBuffer alloc] initWithCapacity:4096];
        [obj writeContentToBuffer:snapshot];
        [snapshot resetCursor];
        [[undoManager prepareWithInvocationTarget:self] updateObject:obj 
                                                        withSnapshot:snapshot];
        [snapshot release];
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
        BNRDataBuffer *snapshot = [[BNRDataBuffer alloc] initWithCapacity:4096];
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

- (BOOL)saveChanges:(NSError **)errorPtr
{
    [self willChangeValueForKey:@"hasUnsavedChanges"];

    NSEnumerator *e; 
    BNRStoredObject *obj;
    BNRDataBuffer *buffer = [[BNRDataBuffer alloc] initWithCapacity:65536];
    [backend beginTransaction];
    
    //NSLog(@"inserting %d objects", [toBeInserted count]);
     
    e = [toBeInserted objectEnumerator];
    while (obj = [e nextObject]) {
        Class c = [obj class];
        UInt32 rowID = [obj rowID];
        
        [obj writeContentToBuffer:buffer];
        [backend insertData:buffer
                   forClass:c
                      rowID:rowID];
        [buffer clearBuffer];
    }
    
    //NSLog(@"updating %d objects", [toBeUpdated count]);

    // Updates
    NSMutableSet *tentativelyUpdated = [[NSMutableSet alloc] init];

    while (obj = [toBeUpdated anyObject]) {
        Class c = [obj class];
        UInt32 rowID = [obj rowID];
        
        [obj writeContentToBuffer:buffer];
        
        [backend updateData:buffer
                   forClass:c
                      rowID:rowID];
        [buffer clearBuffer];
        [tentativelyUpdated addObject:obj];
        [toBeUpdated removeObject:obj];
    }
    // Deletes
    
    //NSLog(@"deleting %d objects", [toBeDeleted count]);
    NSMutableSet *tentativelyDeleted = [[NSMutableSet alloc] init];
    while (obj = [toBeDeleted anyObject]) {
        if ([tentativelyDeleted containsObject:obj]) {
            continue;
        }
        [tentativelyDeleted addObject:obj];

        Class c = [obj class];
        UInt32 rowID = [obj rowID];
        
        // Take it out of the uniquing table:
        // Should I remove it from the uniquingTable in deleteObject?
        BNRIntDictionary *u = [uniquingTable objectForClass:c];
        [u removeObjectForInt:rowID];
        [obj setStore:nil];

        [backend deleteDataForClass:c
                              rowID:rowID];
        
        if (savesDataForSync) {
            //UInt8 classID = [self classIDForClass:c];
            // FIXME: change status of this object to deleted in log
        }
        
        [toBeDeleted removeObject:obj];

    }
    
    // Write out class meta data
    // FIXME: things will be faster if you only 
    // save ones that have been changed
    int i = 0;
    Class c;
    while (c = classes[i]) {
        BNRClassMetaData *d = [classMetaData objectForClass:c];
        if (d) {
            [d writeContentToBuffer:buffer];
            //NSLog(@"Inserting %d bytes of meta data for %@", [buffer length], NSStringFromClass(c));

            [backend updateData:buffer
                       forClass:c
                          rowID:1];
            [buffer clearBuffer];
        }
        i++;
    }
    [buffer release];
    
    BOOL successful = [backend commitTransaction];
    if (successful) {
        [toBeInserted removeAllObjects];
    } else {
        [toBeUpdated unionSet:tentativelyUpdated];
        [toBeDeleted unionSet:tentativelyDeleted];
        [backend abortTransaction];
    }
    [tentativelyUpdated release];
    [tentativelyDeleted release];
    
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
    [be retain];
    [backend release];
    backend = be;
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
    return [NSString stringWithFormat:@"<BNRStore-%@ to insert:%d, to update %d, to delete %d>",
        backend, [toBeInserted count], [toBeUpdated count], [toBeDeleted count]];
}
@end
