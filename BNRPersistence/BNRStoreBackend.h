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
@class BNRBackendCursor;
@class BNRDataBuffer;

// BNRStoreBackend is an abstract class.  The concrete subclass uses a particular
// key-value store.  At different times, these subclasses have used BerkeleyDB, 
// GDBM, and TokyoCabinent.

@interface BNRStoreBackend : NSObject {

    // In preparation for sync, the clientID, if not zero, is used with 
    // the rowID as the key.  The idea being: many clients can create objects that 
    // get shared in a central repository.  The clientID ensures uniqueness of the key.
    UInt32 clientID;
}

@property (nonatomic) UInt32 clientID;

#pragma mark Transaction support
- (BOOL)beginTransaction;
- (BOOL)commitTransaction;
- (BOOL)abortTransaction;
- (BOOL)hasOpenTransaction;

#pragma mark Writing changes

- (void)insertData:(BNRDataBuffer *)attNames 
          forClass:(Class)c
             rowID:(UInt32)n;

- (void)deleteDataForClass:(Class)c
                     rowID:(UInt32)n;
                  
- (void)updateData:(BNRDataBuffer *)d 
          forClass:(Class)c 
             rowID:(UInt32)n;

#pragma mark Fetching

- (BNRDataBuffer *)dataForClass:(Class)c 
                   rowID:(UInt32)n;

- (BNRBackendCursor *)cursorForClass:(Class)c;

- (void)close;

@end
