//
//  BufferEncryptionTests.m
//  EncryptionTest
//
//  Created by Adam Preble on 4/7/10.
//  Copyright 2010 Big Nerd Ranch. All rights reserved.
//

#import "BufferEncryptionTests.h"
#import "BNRDataBuffer.h"
#import "BNRDataBuffer+Encryption.h"
#import <openssl/rand.h>

@interface NSData (RandomizedData)
+ (id)dataWithRandomBytesOfLength:(int)length;
@end

@implementation NSData (RandomizedData)
+ (id)dataWithRandomBytesOfLength:(int)length
{
    void *bytes = malloc(length);
    RAND_pseudo_bytes(bytes, length);
    NSData *output = [[self class] dataWithBytes:bytes length:length]; // Use [self class] so it will be NSMutableData if that's the class.
    free(bytes);
    return output;
}
@end



@implementation BufferEncryptionTests

- (void)setUp
{
    randomData = [NSMutableData dataWithRandomBytesOfLength:1024];
    // Make a copy because BNRDataBuffer will take ownership of the buffer.  Have to use dataWithData: because [NSData copy] returns the same bytes buffer.
    void *copyOfData = malloc([randomData length]);
    memcpy(copyOfData, [randomData bytes], [randomData length]);
    buffer = [[BNRDataBuffer alloc] initWithData:copyOfData length:[randomData length]];
    
    RAND_pseudo_bytes(salt, 8);
}
- (void)tearDown
{
    [buffer release];
    buffer = nil;
}

- (BOOL)bufferMatchesOriginalData
{
    return memcmp([buffer buffer], [randomData bytes], [randomData length]) == 0 ? YES : NO;
}

- (void)testSetUp
{
    STAssertFalse([buffer buffer] == [randomData bytes], @"Copy of data was not made?");
    STAssertTrue([self bufferMatchesOriginalData], @"buffer contents matches randomData");
}
- (void)testEmptyKey
{
    [buffer encryptWithKey:@"" salt:salt];
    STAssertTrue([self bufferMatchesOriginalData], @"Encryption with empty key should not encrypt.");
}
- (void)testNilKey
{
    [buffer encryptWithKey:nil salt:salt];
    STAssertTrue([self bufferMatchesOriginalData], @"Encryption with nil key should not encrypt.");
}
- (void)testBasicCycle
{
    [buffer encryptWithKey:@"howdy" salt:salt];
    STAssertFalse([self bufferMatchesOriginalData], @"Encrypted bytes should not match clear bytes.");
    [buffer decryptWithKey:@"howdy" salt:salt];
    STAssertTrue([self bufferMatchesOriginalData], @"Decrypted data should match");
}
- (void)testBasicCycleBadPassword
{
    [buffer encryptWithKey:@"howdy" salt:salt];
    STAssertFalse([self bufferMatchesOriginalData], @"Encrypted bytes should not match clear bytes.");
    [buffer decryptWithKey:@"howdy1" salt:salt];
    STAssertFalse([self bufferMatchesOriginalData], @"Decrypted data with different password should not match");
}
- (void)testBasicCycleBadPasswordRepeated
{
    // Make sure that our encryption doesn't cook up the same key for the password repeated (such as a block cipher might).
    [buffer encryptWithKey:@"howdy" salt:salt];
    STAssertFalse([self bufferMatchesOriginalData], @"Encrypted bytes should not match clear bytes.");
    [buffer decryptWithKey:@"howdyhowdy" salt:salt];
    STAssertFalse([self bufferMatchesOriginalData], @"Decrypted data with different password should not match");
}


@end
