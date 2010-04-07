//
//  BNRClassMetaData+Encryption.m
//  EncryptionTest
//
//  Created by Adam Preble on 4/6/10.
//  Copyright 2010 Big Nerd Ranch. All rights reserved.
//

#import "BNRClassMetaData+Encryption.h"
#import <openssl/md5.h>

UInt32 HashForKey(NSString *key, unsigned char *salt)
{
    if (!key || [key length] == 0)
        return 0;
    
    MD5_CTX ctx;
    MD5_Init(&ctx);
    MD5_Update(&ctx, salt, 8);
    MD5_Update(&ctx, [key UTF8String], [key lengthOfBytesUsingEncoding:NSUTF8StringEncoding]);
    UInt32 md[4];
    MD5_Final((unsigned char*)md, &ctx);
    return md[0] ^ md[1] ^ md[2] ^ md[3];
}

@implementation BNRClassMetaData (Encryption)

- (void)setEncryptionKeyHashForKey:(NSString *)encryptionKey
{
    encryptionKeyHash = HashForKey(encryptionKey, encryptionKeySalt);
    NSLog(@"encryptionKeyHash = 0x%08x", encryptionKeyHash);
}

- (BOOL)hashMatchesEncryptionKey:(NSString *)encryptionKey;
{
    UInt32 testHash = HashForKey(encryptionKey, encryptionKeySalt);
    return testHash == encryptionKeyHash;
}

@end
