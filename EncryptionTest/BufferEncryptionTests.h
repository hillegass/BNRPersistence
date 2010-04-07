//
//  BufferEncryptionTests.h
//  EncryptionTest
//
//  Created by Adam Preble on 4/7/10.
//  Copyright 2010 Big Nerd Ranch. All rights reserved.
//

#import <SenTestingKit/SenTestingKit.h>

@class BNRDataBuffer;

@interface BufferEncryptionTests : SenTestCase {
    BNRDataBuffer *buffer;
    NSMutableData *randomData;
    UInt32 salt[2];
}

@end
