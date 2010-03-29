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

#import <Foundation/Foundation.h>
@class BNRStoredObject;
@class BNRStore;

/*!
 @class BNRDataBuffer
 @abstract used for reading and writing blobs of data that come from the backend
 @discussion Everything is kept in little-endian order for best performance on 
 Intel machines.
*/
@interface BNRDataBuffer : NSObject {
    unsigned char *buffer;
    unsigned capacity;

    unsigned length;    
    unsigned char *cursor;
    UInt8 versionOfData; // Set by consumeVersion, see -[BNRStore usesPerInstanceVersioning];
}
/*!
 @method initWithCapacity:
 @abstract Used to create empty buffers (typically then written to)
*/
- (id)initWithCapacity:(NSUInteger)c;

/*!
 @method initWithData:length:
 @abstract Used to create full buffers (typically then read from)
*/
- (id)initWithData:(void *)v
            length:(unsigned)size;
/*!
 @method setData:length:
 @abstract Used to create full buffers (typically then read from)
 */
- (void)setData:(void *)v
         length:(unsigned)size;

/*!
 @method resetCursor
 @abstract Moves the cursor back to the beginning of the buffer
*/
- (void)resetCursor;

/*!
 @method clearBuffer
 @abstract Moves the cursor back to the beginning of the buffer and
 sets the length to zero
*/
- (void)clearBuffer;

/*!
 @method readUInt32
 @abstract Reads an unsigned 32-bit integer from the buffer (at the cursor) and moves the cursor
 past it.
*/
- (UInt32)readUInt32;

/*!
 @method writeUInt32:
 @abstract Writes an unsigned 32-bit integer to the buffer (at the cursor) and moves the cursor
 past it.
*/
- (void)writeUInt32:(UInt32)x;


/*!
 @method readUInt8
 @abstract Reads an unsigned 8-bit integer from the buffer (at the cursor) and moves the cursor
 past it.
*/
- (UInt8)readUInt8;

/*!
 @method writeUInt8:
 @abstract Writes an unsigned 8-bit integer to the buffer (at the cursor) and moves the cursor
 past it.
*/
- (void)writeUInt8:(UInt8)x;

- (Float32)readFloat32;
- (void)writeFloat32:(Float32)f;

- (Float64)readFloat64;
- (void)writeFloat64:(Float64)f;

- (BNRStoredObject *)readObjectReferenceOfClass:(Class)c
                                     usingStore:(BNRStore *)s;

- (void)writeObjectReference:(BNRStoredObject *)obj;

- (BNRStoredObject *)readObjectReferenceOfUnknownClassUsingStore:(BNRStore *)s;
- (void)writeObjectReferenceOfUnknownClass:(BNRStoredObject *)obj 
                                usingStore:(BNRStore *)s;

- (NSMutableArray *)readArrayOfClass:(Class)c
                          usingStore:(BNRStore *)s;

- (void)writeArray:(NSArray *)a
           ofClass:(Class)c;

- (NSMutableArray *)readHeteroArrayUsingStore:(BNRStore *)s;

- (void)writeHeteroArray:(NSArray *)a usingStore:(BNRStore *)s;

- (id)readArchiveableObject;
- (void)writeArchiveableObject:(id)obj;


- (NSString *)readString;
- (void)writeString:(NSString *)s;
- (NSData *)readData;
- (void)writeData:(NSData *)d;
- (NSDate *)readDate;
- (void)writeDate:(NSDate *)d;

- (void)copyFrom:(const void *)d
          length:(size_t)byteCount;

- (void)consumeVersion;
- (void)writeVersionForObject:(BNRStoredObject *)obj;
- (UInt8)versionOfData;

- (unsigned char *)buffer;
- (unsigned)length;
- (void)setLength:(unsigned)r;
- (unsigned)capacity;
- (NSString *)description;
@end
