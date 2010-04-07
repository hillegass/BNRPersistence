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

void BFEncrypt(NSString *key, const UInt8 *salt, int enc, void *data, long length)
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

- (void)decryptWithKey:(NSString *)key salt:(const UInt8 *)salt
{
    BFEncrypt(key, salt, BF_DECRYPT, buffer, length);
}

- (void)encryptWithKey:(NSString *)key salt:(const UInt8 *)salt
{
    BFEncrypt(key, salt, BF_ENCRYPT, buffer, length);
}

@end
