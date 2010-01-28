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

#import "BNRIntDictionary.h"
#import <inttypes.h>

static UInt32 goodPrimes[] = {
    769, 1543, 3079, 6151, 12289, 24593, 49157, 98317, 196613, 393241, 786433, 
    1572869, 3145739,6291469,12582917,25165843,50331653,100663319,201326611,
    402653189,805306457,1610612741, 0
};


@implementation BNRIntDictionary

- (id)initWithExpectedCount:(UInt32)c
{
    [super init];
    int i = 0;
    while ((bucketCount = goodPrimes[i]) < c) {
        i++;
        if (bucketCount == 0) {
            bucketCount = goodPrimes[i-1];
            break;
        }
    }
    
    bucketArray = (struct BucketInfo *)calloc(bucketCount, sizeof(struct BucketInfo));
    return self;
}

- (void)dealloc
{
    for (int i = 0; i < bucketCount; i++) {
        struct BucketInfo *ptr = bucketArray + i;
        if (ptr->bucket) {
            free(ptr->bucket);
        }
    }
    free(bucketArray);
    [super dealloc];
}

- (struct IntPair *)intPairForInt:(UInt32)c 
                createIfNecessary:(BOOL)create
{
    UInt32 bucketIndex = c % bucketCount;
    struct BucketInfo *bi = bucketArray + bucketIndex;
    
    // Is the bucket uncreated?
    if (bi->capacity == 0) {
        if (!create) {
            return NULL;
        } else {
            bi->capacity = 4;
            bi->itemCount = 1;
            bi->bucket = (struct IntPair *)malloc(bi->capacity * sizeof(struct IntPair));
            bi->bucket[0].key = c;
            return bi->bucket;
        }
    }
    
    // Step though the bucket looking for the right key
    for (UInt32 i = 0; i < bi->itemCount; i++) {
        
        // Found it?
        if (bi->bucket[i].key == c) {
            return bi->bucket + i;
        }
    }
    
    // Not found in bucket
    if (!create) {
        return NULL;
    }
    
    // Is the bucket full?
    if (bi->capacity == bi->itemCount) {
        const size_t newCapacity = bi->capacity * 2;
        NSLog(@"resizing bucket %"PRIu32" to %"PRIuMAX,
              bucketIndex, (uintmax_t)newCapacity);

        const size_t newSize = newCapacity * sizeof(struct IntPair);
        void *const oldBucket = bi->bucket;
        struct IntPair *const newBucket = (struct IntPair *)realloc(oldBucket,
                                                                    newSize);
        if (!newBucket && oldBucket) free(oldBucket);
        bi->bucket = newBucket;
    }

    struct IntPair *const result = bi->bucket + bi->itemCount;
    result->key = c;
    bi->itemCount += 1;
    return result;
}


- (void)setObject:(id)obj forInt:(UInt32)c;
{
    //NSLog(@"%@: setObj:%@ forInt:%d", self, obj, c);
    struct IntPair *pptr = [self intPairForInt:c
                             createIfNecessary:YES];
    pptr->object = obj;
}

- (id)objectForInt:(UInt32)c;
{
    struct IntPair *pptr = [self intPairForInt:c
                             createIfNecessary:NO];
    if (!pptr) {
        return nil;
    } else {
        return pptr->object;
    }
}

- (void)removeObjectForInt:(UInt32)c
{
    UInt32 bucketIndex = c % bucketCount;
    struct BucketInfo *bi = bucketArray + bucketIndex;
    
    UInt32 i;
    for (i = 0; i < bi->itemCount; i++) {
        struct IntPair *current = bi->bucket + i;
        if (current->key == c) {
            break;
        }
    }
    
    // Did I get to the end without finding it?
    if (i==bi->itemCount) {
        NSLog(@"didn't find pair for %u", c);
        return;
    }
    bi->itemCount--;
    while (i < bi->itemCount) {
        struct IntPair *current = bi->bucket + i;
        struct IntPair *next = current + 1;
        *current = *next;
        i++;
    }
}

- (void)makeEveryObjectPerformSelector:(SEL)s
{
    for (int i = 0; i < bucketCount; i++) {
        struct BucketInfo *bi = bucketArray + i;
        UInt32 count = bi->itemCount;
        for (int j = 0; j < count ; j++) {
            id object = ((bi->bucket) + j)->object;
            [object performSelector:s];
        }
    }
}

- (void)logStats
{
    Class objClass = NULL;
    UInt32 sum = 0;
    for (int i = 0; i < bucketCount; i++) {
        struct BucketInfo *bi = bucketArray + i;
        UInt32 count = bi->itemCount;
        for (int j = 0; j < count ; j++) {
            sum++;
            id object = ((bi->bucket) + j)->object;
            objClass = [object class];
        }
    }
    NSLog(@"Instances of %@ in uniquing table = %u", NSStringFromClass(objClass), sum);
}


@end
