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

#import "BNRClassMetaData.h"
#import "BNRDataBuffer.h"
#import <libkern/OSAtomic.h>
#import "BNRCrypto.h"

@implementation BNRClassMetaData

- (id)init
{
    self = [super init];
	if (self) {
		// Primary keys start at 2
		lastPrimaryKey = 1;
		versionNumber = 1;
		classID = 0;
		
		// Randomize the salt to start with; if a class is being loaded the salt will be set in -readContentFromBuffer:.
		BNRRandomBytes(&salt.word, sizeof(salt.word));
	}
    return self;
}
- (void)readContentFromBuffer:(BNRDataBuffer *)d
{
    lastPrimaryKey = [d readUInt32];
    versionNumber = [d readUInt8];
    classID = [d readUInt8];
    if ([d length] > 6)
    {
        for (int i = 0; i < BNR_SALT_WORD_COUNT; i++)
            salt.word[i] = [d readUInt32];
    }
        
}
- (void)writeContentToBuffer:(BNRDataBuffer *)d
{
    [d writeUInt32:lastPrimaryKey];
    [d writeUInt8:versionNumber];
    [d writeUInt8:classID];

    for (int i = 0; i < BNR_SALT_WORD_COUNT; i++)
        [d writeUInt32:salt.word[i]];
}
- (unsigned char )classID
{
    return classID;
}
- (void)setClassID:(unsigned char )x
{
    classID = x;
}
- (UInt32)nextPrimaryKey
{
    // FIXME: Write AtomicIncrementUInt32Barrier() using Atomic C&S.
    UInt32 nextPrimaryKey = (UInt32)OSAtomicIncrement32Barrier(
                                (volatile int32_t *)&lastPrimaryKey);
    return nextPrimaryKey;
}

- (unsigned char)versionNumber
{
    return versionNumber;
}

- (void)setVersionNumber:(unsigned char)x
{
    versionNumber = x;
}

- (const BNRSalt *)encryptionKeySalt
{
    return &salt;
}
@end
