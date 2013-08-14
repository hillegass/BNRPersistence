//
//  BNRResizableUniquingTable.m
//  BNRPersistence
//
//  Created by Bill Monk on 8/1/11.
//  Copyright 2011 Big Nerd Ranch. All rights reserved.
//

#import "BNRResizableUniquingTable.h"
#import "BNRStoredObject.h"

@interface BNRStoredObject (ClearingStore)
- (void)clearStore;
@end

// Issue: No NSMapTable on iOS

@implementation BNRResizableUniquingTable


//#define kHashKeyModulus 786433	// 0xC0001  // in ComplexInsertText, 0xC0001 (the value used in BNRUniquingTable) causes 100,000 buckets to be created for 100,000 songs, using ~52MB (heapdiff). (original BNRUniquingTable used ~6MB, using 21MB for 100,000 entries)
#define kHashKeyModulus 0xC01					// in ComplexInsertText, xC01 causes 3,000 buckets to be created for 100,000 songs, using 2MB, total memory footprint 15MB (heapdiff) 
												// (original BNRUniquing table used ~6MB, total memory footprint 21MB for 100,000 entries)
												// insertion time with new uniquing table for 100,000 objects (MBAir) ranged from 1131ms-1605ms over ten looped runs with a new empty file; old uniquing table ranged 980ms to 1242ms over ten looped runs, new empty file)

#define kCreateMapTableViaCInterface (1)
#define kHashValueOfZeroIsOK (kCreateMapTableViaCInterface)

- (id)init
{
    self = [super init];
    if (self) {
		// create NSMapTable with weak non-object keys and strong object values
		
		
#if kCreateMapTableViaCInterface
		// "The table’s size is dependent on (but generally not equal to) capacity. If capacity is 0, a small map table is created."
		NSUInteger tableSize = 0;

		// The C API for creating NSMapTable allows key values of 0. The Obj-C API  works, but  disallows key values of 0.
		table = NSCreateMapTable(NSIntegerMapKeyCallBacks, NSObjectMapValueCallBacks, tableSize);
#else
		// Obj-C API also works, but disallows key values of 0, so hashValueFor() needs to special case that, then it all still works.
		// But NSCreateMapTable with NSIntegerMapKeyCallBacks above is cleaner.
		NSPointerFunctionsOptions keyOptions = NSPointerFunctionsOpaqueMemory |
											   NSPointerFunctionsIntegerPersonality;
		
		NSPointerFunctionsOptions valueOptions = NSPointerFunctionsStrongMemory |
												 NSPointerFunctionsObjectPersonality;
		table = [[NSMapTable mapTableWithKeyOptions:keyOptions valueOptions:valueOptions] retain];
#endif
    }
    return self;
}
- (void)dealloc
{	
	if (table) {
		NSArray *allBuckets = NSAllMapTableValues(table);
		for (NSPointerArray *bucket in allBuckets) {
			for (BNRStoredObject *currentObject in bucket) {
				[currentObject clearStore];
			}
		}
		NSFreeMapTable(table);
		//[table release];
	}
	
    [super dealloc];
}

static NSUInteger hashValueFor(Class c, UInt32 row)
{
#define kTestUInt64WarningFix 1  
// test fix for compiler complaints (Release config only) about cast to UInt64: "Cast from pointer to integer of different size"
// but is NSUInteger the correct cast?
#if kTestUInt64WarningFix 
    NSUInteger bucketKey = (row + (NSUInteger)c) % kHashKeyModulus;
#else
    UInt32 bucketKey = (row + (UInt64)(*(char *)&c)) % kHashKeyModulus;
#endif
	
#if !kHashValueOfZeroIsOK
	// NSMapTable created via the C interface with NSCreateMapTable(NSIntegerMapKeyCallBacks...
	// can use key values of 0. 
	// NSMapTable created with the objC API, even with NSPointerFunctionsIntegerPersonality
	// cannot. 
	// -init above uses the former so 0 hash values are OK. 
	// Query:perhaps it'd be better to just always disallow a 0 value here? 	
	if (bucketKey == 0) {
		bucketKey = 1; 
	}
#endif
	
#define kLogHashValueInfo 0
#if kLogHashValueInfo // log bucket info
	NSLog(@"\r=================hash value calc================\r      kHashKeyModulus :0x%9x\r                  row :0x%9x\r                    c :%p\r (row + (NSUInteger)c):0x%9x\r((row + (NSUInteger)c) mod kHashKeyModulus) == 0x%x\r================================================\r", kHashKeyModulus, row, c, (row + (NSUInteger)c), bucketKey);
	NSLog(@"hasValue for Class:%@ (%p), rowID:%u, bucketKey:%d (0x%x)", NSStringFromClass(c), c, row, bucketKey, bucketKey);
#endif
	
	return bucketKey;
}

- (void)setObject:(BNRStoredObject *)obj forClass:(Class)c rowID:(UInt32)row
{
    NSUInteger bucketKey = hashValueFor(c, row);
	
	BNRStoredObject *objectToReplace = nil;
	NSPointerArray *bucket = (NSPointerArray *)NSMapGet(table, (void *)bucketKey);
    if (bucket) {
		NSUInteger index = 0;
        for (BNRStoredObject *currentObject in bucket) {
			if (currentObject && [currentObject rowID] == row && [currentObject class] == c) {
				objectToReplace = currentObject;
				break;
			}
			++index;
		}
		if (objectToReplace) {
			[bucket replacePointerAtIndex:index withPointer:obj];
		}
		else {
			[bucket addPointer:obj];
		}
    } 
	else {
		NSPointerArray *newBucket = [NSPointerArray pointerArrayWithWeakObjects];
		[newBucket addPointer:obj];
		
#define kCheckMapTableInserts 0
#if kCheckMapTableInserts
		// NSMapInsert doesn't report success/failure, so this can be used for stress testing.
		// Only problem found was that keys of value zero don't work unless map table is
		// created with the C API, NSCreateMapTable(NSIntegerMapKeyCallBacks...; see -init
		NSUInteger oldCount = NSCountMapTable(table);
#endif
		
		NSMapInsertKnownAbsent(table, (const void *)bucketKey, newBucket);
		
#if kCheckMapTableInserts
		// NSMapTable created via the C interface with NSCreateMapTable(NSIntegerMapKeyCallBacks...
		// can use key values of 0. 
		// NSMapTable created with the ObjC API, even with NSPointerFunctionsIntegerPersonality
		// cannot. 
		// -init above uses the former so 0 hash values are OK. 
		// Query:perhaps it'd be better to just always disallow a 0 value here? 	
		NSUInteger newCount = NSCountMapTable(table);
		if (newCount <= oldCount) {
			NSLog(@"NSMapInsert failed with bucketKey:%lu for Class:%@ rowID:%d", bucketKey, NSStringFromClass(c), row);
		}
#endif
	}
	
}

- (BNRStoredObject *)objectForClass:(Class)c rowID:(UInt32)row;
{
    NSUInteger bucketKey = hashValueFor(c, row);
		
	NSPointerArray *bucket = (NSPointerArray *)NSMapGet(table, (const void *)bucketKey);
    if (bucket) {
        for (BNRStoredObject *currentObject in bucket) {
			if (currentObject && [currentObject rowID] == row && [currentObject class] == c) {
				return currentObject;
			}
		}
    }
    return nil;
}

- (void)removeObjectForClass:(Class)c rowID:(UInt32)row
{
    NSUInteger bucketKey = hashValueFor(c, row);
	
	NSPointerArray *bucket = (NSPointerArray *)NSMapGet(table, (const void *)bucketKey);
    if (bucket) {
		NSUInteger index = 0;
        for (BNRStoredObject *currentObject in bucket) {
			if (currentObject && [currentObject rowID] == row && [currentObject class] == c) {
				break;
			}
			++index;
        }
		[bucket removePointerAtIndex:index];
    }
	
	// may be more efficient to just leave empty buckets, in case they are needed again
	//[bucket compact];
}

- (void)makeAllObjectsPerformSelector:(SEL)s
{
	NSArray *allBuckets = NSAllMapTableValues(table);
	for (NSPointerArray *bucket in allBuckets) {
        for (BNRStoredObject *currentObject in bucket) {
            [currentObject performSelector:s];
        }
    }
}

- (void)logTable
{
	NSString *tableDescription = NSStringFromMapTable(table);
	NSLog(@"tableDescription:%@", tableDescription);
		
	// BNRUniquingTable has a better logTable method, but it has not been ported to BNRResizableUniquingTable
}

@end
