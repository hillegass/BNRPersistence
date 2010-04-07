//
//  BNRDataBuffer+Encryption.m
//  EncryptionTest
//
//  Created by Adam Preble on 4/6/10.
//  Copyright 2010 Big Nerd Ranch. All rights reserved.
//

#import "BNRDataBuffer+Encryption.h"
#import <openssl/blowfish.h>
#import <openssl/md5.h>

void BFEncrypt(NSString *key, const UInt32 *salt, int enc, void *data, long length)
{
    if (key == nil || [key length] == 0)
        return;
    
    unsigned char ivec[8];
    memset(ivec, 0x0, 8);
    int num = 0;
    
    // Create a hash of the key.
    MD5_CTX ctx;
    MD5_Init(&ctx);
    MD5_Update(&ctx, (void*)salt, 8);
    MD5_Update(&ctx, (void*)[key UTF8String], [key lengthOfBytesUsingEncoding:NSUTF8StringEncoding]);
    UInt32 md[4];
    MD5_Final((unsigned char*)md, &ctx);
    
    BF_KEY schedule;
    BF_set_key(&schedule, 4*sizeof(UInt32), (unsigned char*)md);
    
    void *dataOut = malloc(length);
    if (dataOut == NULL)
    {
        NSLog(@"BFEncrypt(): Memory allocation error.");
        return;
    }
    
    BF_cfb64_encrypt(data, dataOut, length, &schedule, ivec, &num, enc);
    
    memcpy(data, dataOut, length);
    free(dataOut);
}

@implementation BNRDataBuffer (Encryption)

- (BOOL)decryptWithKey:(NSString *)key salt:(const UInt32 *)salt
{
    if (key == nil || [key length] == 0)
        return YES;
    
    BOOL ret = YES;
    
    UInt32 littleSalt[2];
    littleSalt[0] = CFSwapInt32HostToLittle(salt[0]);
    littleSalt[1] = CFSwapInt32HostToLittle(salt[1]);
    
    // Decrypt in a temporary buffer so that if it doesn't match up with the salt we can keep the buffer the same,
    // such as in the case of an object record that was not encrypted (but the key is now set).
    UInt8 *decryptedBuffer = (UInt8*)malloc(length);
    memcpy(decryptedBuffer, buffer, length); 
    BFEncrypt(key, littleSalt, BF_DECRYPT, decryptedBuffer, length);
    if (length >= 8 && memcmp(decryptedBuffer, littleSalt, 8) == 0)
    {
        // Salt matches, so we believe the given key is good.
        // Let's move the data into our own buffer.
        memcpy(buffer, decryptedBuffer+8, length-8);
        length -= 8;
        ret = YES;
    }
    else
    {
        // We weren't able to decrypt the buffer successfully.
        // This might just be okay if the object wasn't encrypted in the first place.
        ret = NO;
    }
    free(decryptedBuffer);
    return ret;
}

- (void)encryptWithKey:(NSString *)key salt:(const UInt32 *)salt
{
    if (key == nil || [key length] == 0)
        return;
    
    UInt32 littleSalt[2];
    littleSalt[0] = CFSwapInt32HostToLittle(salt[0]);
    littleSalt[1] = CFSwapInt32HostToLittle(salt[1]);
    
    int encryptedBufferLength = 8 + length;
    UInt8 *encryptedBuffer = (UInt8*)malloc(encryptedBufferLength);
    memcpy(encryptedBuffer, littleSalt, 8);
    memcpy(encryptedBuffer + 8, buffer, length);
    BFEncrypt(key, littleSalt, BF_ENCRYPT, encryptedBuffer, encryptedBufferLength);
    [self setData:encryptedBuffer length:encryptedBufferLength];
}

@end
