//
//  BNRDataBuffer+Encryption.m
//  EncryptionTest
//
//  Created by Adam Preble on 4/6/10.
//  Copyright 2010 Big Nerd Ranch. All rights reserved.
//

#import "BNRDataBuffer+Encryption.h"
#import <openssl/blowfish.h>

void BFEncrypt(NSString *key, int enc, void *data, long length)
{
    if (key == nil || [key length] == 0)
        return;
    
    unsigned char ivec[8];
    memset(ivec, 0x0, 8);
    int num = 0;
    
    BF_KEY schedule;
    BF_set_key(&schedule, [key lengthOfBytesUsingEncoding:NSUTF8StringEncoding], (unsigned char*)[key UTF8String]);
    
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

- (void)decryptWithKey:(NSString *)key
{
    BFEncrypt(key, BF_DECRYPT, buffer, length);
}

- (void)encryptWithKey:(NSString *)key
{
    BFEncrypt(key, BF_ENCRYPT, buffer, length);
}

@end
