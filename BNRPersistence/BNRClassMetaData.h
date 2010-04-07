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
@class BNRDataBuffer;

/*!
 @const kBNRMetadataRowID
 @abstract Defines the row ID where class metadata are stored.
 */
#define kBNRMetadataRowID (1)

/*! 
 @class BNRClassMetaData
 @abstract Holds onto the classID, the last primary key given out, and
 the version number of the class.
 @discussion The information necessary for the store to get objects into and out
 of the backend.  In particular, each object going into the store needs a unique rowID (or
 or primary key). The rowIDs are unique for that class.  The version number is
 there so that in readContentFromBuffer: an object can figure out what version the
 data was written in.  The classID is written to the stream when you need to know which
 class is in the store.

 rowIDs will start at 2 because the database doesn't allow record 0 and record 1 contains
 this class meta data.

 BNRClassMetaData are kept in a BNRClassDictionary, so they don't actually have the 
 Class itself.
*/


@interface BNRClassMetaData : NSObject {    
    // These attributes are stored as record 1 in the database
    // for the class
    unsigned char classID;
    volatile UInt32 lastPrimaryKey;
    unsigned char versionNumber;
    UInt32 encryptionKeyHash;
    UInt8 encryptionKeySalt[8];
} 

/*!
 @method classID
 @abstract Returns the class's ID
 @discussion Each class in the store has a different classID.  Note that we can only
 handle 256 classes.
*/
- (unsigned char )classID;

/*!
 @method setClassID:
 @abstract For setting the class's ID
*/
- (void)setClassID:(unsigned char )x;

/*!
 @method nextPrimaryKey
 @abstract Returns a unique row ID for that class
 @discussion This method increments the lastPrimaryKey and returns it.
 Threadsafe.
*/
- (UInt32)nextPrimaryKey;

/*!
 @method versionNumber
 @abstract returns the version of the class that is in the backend
*/
- (unsigned char)versionNumber;

/*!
 @method setVersionNumber:
 @abstract Called before saving to record version number of the data
*/
- (void)setVersionNumber:(unsigned char)x;

/*!
 @method readContentFromBuffer:
 @abstract Called automatically when the meta data is first read in from the backend
 */
- (void)readContentFromBuffer:(BNRDataBuffer *)d;

/*!
 @method writeContentToBuffer:
 @abstract Called automatically when the meta data is being written out
 */
- (void)writeContentToBuffer:(BNRDataBuffer *)d;

/*!
 @method encryptionKeyHash
 @abstract Returns the hash of the encryption key used on this class's data.
 */
- (UInt32)encryptionKeyHash;

/*!
 @method encryptionKeySalt
 @abstract Returns the encryption key salt value used on with this class's data.
 */
- (const UInt8 *)encryptionKeySalt;

@end
