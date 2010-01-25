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
// A dictionary with ints for keys and objects for values

#import <Foundation/Foundation.h>

struct IntPair {
    UInt32 key;
    id object;
};

struct BucketInfo {
    struct IntPair *bucket;
    UInt32 itemCount;
    UInt32 capacity;
};

// A hash-table which associates an int with a pointer to an object
// The table does not retain the object
// FIXME: This implementation could be a lot smarter:
//   Ignores large numbers of collsions
//   Never shrinks

@interface BNRIntDictionary : NSObject {
    struct BucketInfo *bucketArray;
    UInt32 bucketCount;
}
// bucketCount will be a prime greater than or equal to expectedCount
- (id)initWithExpectedCount:(UInt32)c;
- (void)setObject:(id)obj forInt:(UInt32)c;
- (id)objectForInt:(UInt32)c;
- (void)removeObjectForInt:(UInt32)c;
- (void)makeEveryObjectPerformSelector:(SEL)s;
- (void)logStats;

@end
