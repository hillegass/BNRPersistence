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
#import "BNRStore.h"

/*! BNRStoredObject is the superclass for all objects that get saved into the store */

@interface BNRStoredObject : NSObject {
    
    // Weak reference
    __weak BNRStore *store;
    
    // rowID is given out by the store. No other instance
    // of the class will have the same rowID
    UInt32 rowID;
    // The least significant bit of status is used for hasContent
    // The other 31 are used for the retain count.
    UInt32 status;
    
}

#pragma mark Versioning and upgrading support
+ (unsigned char)currentClassVersionNumber;
+ (void)configureInStore:(BNRStore *)s;
+ (BOOL)upgradeAllInstancesinStore:(BNRStore *)storeToUpgrade;

#pragma mark Getting data in and out
// readContentFromBuffer: is used during loading
- (void)readContentFromBuffer:(BNRDataBuffer *)d;

// writeContentToBuffer: is used during saving
- (void)writeContentToBuffer:(BNRDataBuffer *)d;

#pragma mark Versioning
- (UInt8)writeVersion;

#pragma mark Relationships

// prepareForDelete implements delete rules
- (void)prepareForDelete;

// dissolveAllRelationships is for when you are trying to release
// all the objects in the store, but you are worried that there
// might be retain-cycles
- (void)dissolveAllRelationships;

#pragma mark Full-text Indexing

// Should return a set of strings.  Each string is the name of
// a property of a string type
+ (NSSet *)textIndexedAttributes;

#pragma mark Dealing with store

// Every StoredObject knows the store that is holding it
- (void)setStore:(BNRStore *)s;
- (BNRStore *)store;

#pragma mark Row ID
- (UInt32)rowID;
- (void)setRowID:(UInt32)n;

#pragma mark Has Content
// Is the object fetched?
- (BOOL)hasContent;
- (void)fetchContent;

// checkForContent is a convenience method that checks to see
// if the object has fetched its data and fetchs it if necessary.
- (void)checkForContent;

#pragma mark Logging utils
- (void)logDescription;

@end
