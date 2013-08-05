//
//  BNRUniquingTable.m
//  BNRPersistence
//
//  Created by Aaron Hillegass on 1/28/10.
//  Copyright 2010 Big Nerd Ranch. All rights reserved.
//

#import "BNRUniquingTable.h"
#import "BNRStoredObject.h"

#define kTestUInt64WarningFix 1  // test fix for compiler complaints (Release config only) about cast to UInt64: "Cast from pointer to integer of different size"

@interface BNRStoredObject (ClearingStore)
- (void)clearStore;
@end

@implementation BNRUniquingTable

- (id)init
{
    self = [super init];
    if (self) {
        numBuckets = 786433;		// 0xC0001  
        //numBuckets = 1572869;		// 0x180005
		
		// Under 32bit, numBuckets == 786433 uses about 3MB of RAM. Under 64bit, this doubles
		// to about 6MB for the same number of UniquingListNode ptrs; see BigTime 32bit vs 64bit heapdiff report.
		// Size of each UniquingListNode also doubles from 8 to 16; with many nodes this could become significant also.
		// Might be able to greatly reduce this with suggestions from 64-bit Transition Guide "Use Fewer Pointers" at:
		// http://developer.apple.com/library/mac/#documentation/Cocoa/Conceptual/Cocoa64BitGuide/OptimizingMemoryPerformance/OptimizingMemoryPerformance
		// Or perhaps use NSHashTable; might have to do this for ARC anyway...
		// See implementation in NSResizableUniquingTable
		
        table = (UniquingListNode **)calloc(numBuckets, sizeof(UniquingListNode *));
	}
    return self;
}
- (void)dealloc
{
    for (UInt32 i = 0; i < numBuckets; i++) {
		UniquingListNode *ptr = table[i];
		UniquingListNode *nextNode = NULL;
        while (ptr != NULL) {
            BNRStoredObject *currentObject = ptr->storedObject;
            nextNode = ptr->next;
            [currentObject clearStore];
            free(ptr);
            ptr = nextNode;
        }
    }
    free(table);
    [super dealloc];
}

- (void)setObject:(BNRStoredObject *)obj forClass:(Class)c rowID:(UInt32)row
{
#if kTestUInt64WarningFix // compiler in Build Config complains "Cast from pointer to integer of different size", but is NSUInteger the correct cast?
    UInt32 bucket = (row + (NSUInteger)c) % numBuckets;
#else
    UInt32 bucket = (row + (UInt64)c) % numBuckets; 
#endif
	//NSLog(@"setObject:forClass:%@ (%p) rowID:%d == bucket:%d", NSStringFromClass(c), c, row, bucket);

	UniquingListNode *ptr = table[bucket];
	UniquingListNode *lastPtr = NULL;
    while (ptr != NULL) {
        BNRStoredObject *currentObject = ptr->storedObject;
        if ([currentObject rowID] == row && [currentObject class] == c) {
            break;
        }
        lastPtr = ptr;
        ptr = ptr->next;
    }
    if (ptr) {
        ptr->storedObject = obj;
    } else {
		UniquingListNode *newNode = (UniquingListNode *)malloc(sizeof(UniquingListNode));
        newNode->storedObject = obj;
        newNode->next = NULL;
        if (lastPtr) {
            lastPtr->next = newNode;
        } else {
            table[bucket] = newNode;
        }
    }
}

- (BNRStoredObject *)objectForClass:(Class)c rowID:(UInt32)row;
{
#if kTestUInt64WarningFix // compiler in Build Config complains "Cast from pointer to integer of different size", but is NSUInteger the correct cast?
    UInt32 bucket = (row + (NSUInteger)c) % numBuckets;
#else
    UInt32 bucket = (row + (UInt64)c) % numBuckets;
#endif
	//NSLog(@"\r=================hash value calc================\r           numBuckets :    0x%x\r                  row :      0x%x\r                    c :%p\r (row + (NSUInteger)c):    0x%x\r((row + (NSUInteger)c) mod numBuckets) == 0x%x\r================================================\r", numBuckets, row, c, (row + (NSUInteger)c), bucket);
	//NSLog(@"objectForClass:%@ (%p), rowID:%u, bucket:%d (0x%X)", NSStringFromClass(c), c, row, bucket, bucket);
	
	UniquingListNode *ptr = table[bucket];
    while (ptr != NULL) {
        BNRStoredObject *currentObject = ptr->storedObject;
        if ([currentObject rowID] == row && [currentObject class] == c) {
            return currentObject;
        }
        ptr = ptr->next;
    }
    return nil;
}

- (void)removeObjectForClass:(Class)c rowID:(UInt32)row
{
#if kTestUInt64WarningFix // compiler in Build Config complains "Cast from pointer to integer of different size", but is NSUInteger the correct cast?
    UInt32 bucket = (row + (NSUInteger)c) % numBuckets;
#else
    UInt32 bucket = (row + (UInt64)c) % numBuckets;
#endif
	
	UniquingListNode *ptr = table[bucket];
	UniquingListNode *previousPtr = NULL;
    while (ptr != NULL) {
        BNRStoredObject *currentObject = ptr->storedObject;
        if ([currentObject rowID] == row && [currentObject class] == c) {
            break;
        }
        previousPtr = ptr;
        ptr = ptr->next;
    }
    if (ptr) {
		UniquingListNode *nextPtr = ptr->next;
        if (previousPtr) {
            previousPtr->next = nextPtr;
        } else {
            table[bucket] = nextPtr;
        }
        free(ptr);
    }    
}

- (void)makeAllObjectsPerformSelector:(SEL)s
{
    for (UInt32 i = 0; i < numBuckets; i++) {
		UniquingListNode *ptr = table[i];
        while (ptr != NULL) {
            BNRStoredObject *currentObject = ptr->storedObject;
            [currentObject performSelector:s];
            ptr = ptr->next;
        }
    }
}

- (void)logTable
{
	NSUInteger totalObjects = 0L;
	NSUInteger numBucketsInUse = 0L;
	
    for (UInt32 i = 0; i < numBuckets; i++) {
		UniquingListNode *ptr = table[i];
		if (ptr) {
			++numBucketsInUse;
		}
		
		NSUInteger currentObjectCount = 0L;
        while (ptr != NULL) {
            BNRStoredObject *currentObject = ptr->storedObject;
			if (currentObject) {
				++currentObjectCount;
			}
            ptr = ptr->next;
        }
		totalObjects += currentObjectCount;
		
#define kLogEveryBucketMap 0
#if kLogEveryBucketMap
		if (currentObjectCount) {
				NSLog(@"bucket:%lu maps to %lu BNRStoredObjects", i, currentObjectCount);
		}
#endif
    }

	NSLog(@"numBucketsInTable:%d", (unsigned int)numBuckets);
	NSLog(@"bytes used by table (numBuckets * sizeof(UniquingListNode *)):%lu", numBuckets * sizeof(UniquingListNode *));
	
	NSLog(@"sizeof(UniquingListNode):%lu", sizeof(UniquingListNode));
	NSUInteger totalNodeSize = (numBucketsInUse * sizeof(UniquingListNode));
	NSLog(@"bytes used by all UniquingListNodes in all buckets:%lu", (unsigned long)totalNodeSize);
	
	NSUInteger bytesUsed = (numBuckets * sizeof(UniquingListNode *)) + totalNodeSize;
	NSLog(@"%lu BNRStoredObjects mapped in %lu buckets, total bytes used:%lu", (unsigned long)totalObjects, (unsigned long)numBucketsInUse, (unsigned long)bytesUsed);
}

@end
