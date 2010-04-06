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


// This databuffer is designed for easy reading and writing in
// BNRStoredObjects.

#import "BNRDataBuffer.h"
#import "BNRStore.h"
#import "BNRStoredObject.h"
#import <CoreFoundation/CFByteOrder.h>

@implementation BNRDataBuffer
- (id)initWithCapacity:(NSUInteger)c
{
    [super init];
    buffer = (unsigned char *)malloc(c);
    cursor = buffer;
    capacity = c;
    return self;
}

- (id)initWithData:(void *)v
            length:(unsigned)size
{
    [super init];
    [self setData:v length:size];
    return self;
}

- (void)setData:(void *)v
         length:(unsigned)size
{
    if (buffer) {
        free(buffer);
    }
    buffer = v;
    cursor = buffer;
    length = size;
    capacity = size;    
}

- (void)dealloc
{
    free(buffer);
    [super dealloc];
}


- (void)grow
{
    NSLog(@"growing");
    unsigned newCapacity;

    if (capacity < 256) {
        newCapacity = 512;
    } else {
        newCapacity = capacity * 2;
    }
    ptrdiff_t offset = cursor - buffer;
    unsigned char *newBuffer = (unsigned char *)malloc(newCapacity);
    memcpy(newBuffer,buffer,length);
    cursor = newBuffer + offset;
    capacity = newCapacity;
    free(buffer);
    buffer = newBuffer;
}

- (void)checkForSpaceFor:(unsigned int)bytesComing
{
    while (capacity < cursor - buffer + bytesComing) {
        [self grow];
    }
}

- (void)resetCursor
{
    cursor = buffer;
}
- (void)clearBuffer
{
    length = 0;
    cursor = buffer;
}

- (Float32)readFloat32
{
    CFSwappedFloat32 result;
    memcpy(&result, cursor, sizeof(CFSwappedFloat32));
    cursor += sizeof(CFSwappedFloat32);
    return CFConvertFloatSwappedToHost(result);
}

- (void)writeFloat32:(Float32)f
{
    [self checkForSpaceFor:sizeof(CFSwappedFloat32)];
    CFSwappedFloat32 s;
    s = CFConvertFloatHostToSwapped(f);
    memcpy(cursor, &s, sizeof(CFSwappedFloat32));
    cursor += sizeof(CFSwappedFloat32);   
    length += sizeof(CFSwappedFloat32);
}

- (Float64)readFloat64
{
    CFSwappedFloat64 result;
    memcpy(&result, cursor, sizeof(CFSwappedFloat64));
    cursor += sizeof(CFSwappedFloat64);
    return CFConvertDoubleSwappedToHost(result);
}

- (void)writeFloat64:(Float64)f
{
    [self checkForSpaceFor:sizeof(CFSwappedFloat64)];
    CFSwappedFloat64 s;
    s = CFConvertDoubleHostToSwapped(f);
    memcpy(cursor, &s, sizeof(CFSwappedFloat64));
    cursor += sizeof(CFSwappedFloat64);   
    length += sizeof(CFSwappedFloat64);
}


- (UInt32)readUInt32
{
    UInt32 result;
    memcpy(&result, cursor, sizeof(UInt32));
    cursor += sizeof(UInt32);
    return CFSwapInt32LittleToHost(result);
}
- (void)writeUInt32:(UInt32)x
{
    [self checkForSpaceFor:sizeof(UInt32)];

    UInt32 swapped = CFSwapInt32HostToLittle(x);
    memcpy(cursor, &swapped, sizeof(UInt32));
    cursor += sizeof(UInt32);   
    length += sizeof(UInt32);
}

- (UInt8)readUInt8
{
    UInt8 result;
    memcpy(&result, cursor, sizeof(UInt8));
    cursor += sizeof(UInt8);
    return result;
}

- (void)writeUInt8:(UInt8)x
{
    [self checkForSpaceFor:sizeof(UInt32)];
    memcpy(cursor, &x, sizeof(UInt8));
    cursor += sizeof(UInt8);   
    length += sizeof(UInt8);
}

- (unsigned char *)buffer
{
    return buffer;
}
- (unsigned)length
{
    return length;
}

- (void)setLength:(unsigned)r
{
    length = r;
}

- (void)copyFrom:(const void *)d
          length:(size_t)byteCount
{
    [self checkForSpaceFor:byteCount];

    memcpy(cursor, d, byteCount);
    cursor += byteCount;
    length += byteCount;

}

- (BNRStoredObject *)readObjectReferenceOfClass:(Class)c
                                     usingStore:(BNRStore *)s
{
    UInt32 rowID = [self readUInt32];
    if (rowID == 0) {
        NSLog(@"reading nil object reference");
        return nil;
    }
    BNRStoredObject *obj = [s objectForClass:c
                                       rowID:rowID
                                fetchContent:NO];
    return obj;
}

- (void)writeObjectReference:(BNRStoredObject *)obj
{
    UInt32 rowID = [obj rowID];
    [self writeUInt32:rowID];
}

- (BNRStoredObject *)readObjectReferenceOfUnknownClassUsingStore:(BNRStore *)s
{
    unsigned char classID = [self readUInt8];
    Class c = [s classForClassID:classID];
    return [self readObjectReferenceOfClass:c
                                 usingStore:s];
}

- (void)writeObjectReferenceOfUnknownClass:(BNRStoredObject *)obj usingStore:(BNRStore *)s
{
    Class c = [obj class];
    unsigned char classID = [s classIDForClass:c];
    [self writeUInt8:classID];
    [self writeObjectReference:obj];
}

- (NSMutableArray *)readArrayOfClass:(Class)c
                          usingStore:(BNRStore *)s
{
    // FIXME: I suspect that this could also be made faster with 
    // clever multithreading
    UInt32 len = [self readUInt32];
    NSMutableArray *result = [[NSMutableArray alloc] initWithCapacity:len];
    [result autorelease];
    int i;
    for (i = 0; i < len; i++) {
        BNRStoredObject *obj = [self readObjectReferenceOfClass:c
                                                     usingStore:s];
        [result addObject:obj];
    }
    return result;
}
- (void)writeArray:(NSArray *)a
           ofClass:(Class)c
{
    UInt32 len = [a count];
    [self writeUInt32:len];
    int i;
    for (i = 0; i < len; i++) {
        BNRStoredObject *obj = [a objectAtIndex:i];
        [self writeObjectReference:obj];
    }
}

- (NSMutableArray *)readHeteroArrayUsingStore:(BNRStore *)s
{
    UInt32 len = [self readUInt32];
    NSMutableArray *result = [[NSMutableArray alloc] initWithCapacity:len];
    [result autorelease];
    int i;
    for (i = 0; i < len; i++) {
        BNRStoredObject *obj = [self readObjectReferenceOfUnknownClassUsingStore:s];
        [result addObject:obj];
    }
    return result;
}

- (void)writeHeteroArray:(NSArray *)a usingStore:(BNRStore *)s
{
    UInt32 len = [a count];
    [self writeUInt32:len];
    int i;
    for (i = 0; i < len; i++) {
        BNRStoredObject *obj = [a objectAtIndex:i];
        [self writeObjectReferenceOfUnknownClass:obj
                                      usingStore:s];
    }
}
- (NSData *)readData
{
    unsigned dLen = [self readUInt32];
    if (dLen == 0) {
        return nil;
    }
    NSData *d = [NSData dataWithBytes:cursor
                               length:dLen];
    cursor += dLen;
    return d;
}

- (void)writeData:(NSData *)d
{
    unsigned dLen = [d length];
    [self writeUInt32:dLen];
    if (dLen != 0) {
        [self copyFrom:[d bytes]
                length:dLen];
    }

}

- (NSDate *)readDate
{
    Float64 timeInterval = [self readFloat64];
    return [NSDate dateWithTimeIntervalSinceReferenceDate:timeInterval];
}

- (void)writeDate:(NSDate *)d
{
    Float64 timeInterval = [d timeIntervalSinceReferenceDate];
    [self writeFloat64:timeInterval];
}

- (id)readArchiveableObject
{
    NSData *d = [self readData];
    id result = [NSKeyedUnarchiver unarchiveObjectWithData:d];
    return result;
}

- (void)writeArchiveableObject:(id)obj
{
    NSData *d = [NSKeyedArchiver archivedDataWithRootObject:obj];
    [self writeData:d];
}


- (NSString *)readString
{
    unsigned dLen = [self readUInt32];
    if (dLen == 0) {
        return nil;
    }
    NSString *d = [[NSString alloc] initWithBytes:cursor length:dLen encoding:NSUTF8StringEncoding];
    cursor += dLen;
    return [d autorelease];
}

- (void)writeString:(NSString *)s
{
    UInt32 dLen = [s lengthOfBytesUsingEncoding:NSUTF8StringEncoding];
    [self writeUInt32:dLen];

    if (dLen != 0) {
        [self checkForSpaceFor:dLen];
        BOOL success = [s getBytes:cursor
                         maxLength:dLen
                        usedLength:NULL
                          encoding:NSUTF8StringEncoding
                           options:0
                             range:NSMakeRange(0, [s length])
                    remainingRange:NULL];
        if (!success) {
            NSLog(@"failed to write any characters");
        }
        
        cursor += dLen;
        length += dLen;
    }
}

- (void)consumeVersion
{
    versionOfData = [self readUInt8];
}

- (void)writeVersionForObject:(BNRStoredObject *)obj
{
    versionOfData = [obj writeVersion];
    [self writeUInt8:versionOfData];
}

- (UInt8)versionOfData
{
    return versionOfData;
}

- (unsigned)capacity
{
    return capacity;
}

- (NSString *)description
{
    NSMutableString *result = [NSMutableString stringWithFormat:@"<BNRDataBuffer %d bytes:",
        [self length]];
    unsigned char *cptr = buffer;
    while (cptr - buffer < length) {
        [result appendFormat:@"%x.", *cptr];
        cptr++;
    }
    return result;
}


@end
