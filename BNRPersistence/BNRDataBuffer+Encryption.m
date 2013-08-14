//
//  BNRDataBuffer+Encryption.m
//  EncryptionTest
//
//  Created by Adam Preble on 4/6/10.
//  Copyright 2010 Big Nerd Ranch. All rights reserved.
//

#import "BNRDataBuffer+Encryption.h"
#import <CommonCrypto/CommonDigest.h>
#import <CommonCrypto/CommonCryptor.h>

/*!
 @function CryptHelper
 @abstract Performs salted AES128 encryption on the given data buffer and returns a new buffer with the result.
 @param key           The "passphrase" to use as part of the encryption key.
 @param salt          The salt to use as part of the encryption key.
 @param operation     kCCEncrypt or kCCDecrypt.
 @param data          Bytes to encrypt or decrypt.
 @param length        Number of bytes in data.
 @param outData       (output) Pointer to the data region containing the decrypted or encrypted data.
                      It is the caller's responsiblity to free() this memory.
 @param outDataLength (output) Number of bytes of data in outData.
 */
BOOL CryptHelper(NSString *key, const BNRSalt *salt, CCOperation operation, const void *data, size_t length, void **outData, size_t *outDataLength)
{
    if (key == nil || [key length] == 0)
        return NO;
    
    *outData = NULL;
    *outDataLength = 0;

    NSData *keyData = [key dataUsingEncoding:NSUTF8StringEncoding];

    // Create a hash of the key.
    CC_MD5_CTX ctx;
    CC_MD5_Init(&ctx);
    CC_MD5_Update(&ctx, (void*)&salt->word, sizeof(salt->word));
    CC_MD5_Update(&ctx, [keyData bytes], [keyData length]);
    UInt32 md[4];
    CC_MD5_Final((unsigned char*)md, &ctx);
    
    CCCryptorRef cryptor = nil;
    CCOptions options = operation == kCCEncrypt ? kCCOptionPKCS7Padding : 0;
    CCCryptorStatus status = CCCryptorCreate(operation, kCCAlgorithmAES128, options, md, sizeof(md), NULL/*IV*/, &cryptor);

    if (status != kCCSuccess) {
        NSLog(@"CCCryptorCreate(): error:%d", status);
        return NO;
    }
    
    int outputLength = CCCryptorGetOutputLength(cryptor, length, true);
    void *outputBuffer = malloc(outputLength);
    if (outputBuffer == NULL)
    {
        NSLog(@"BFEncrypt(): Memory allocation error:%d, %s", errno, strerror(errno));
        CCCryptorRelease(cryptor);
        return NO;
    }
    
    size_t bytesOutput = 0;
    size_t totalBytesOutput = 0;
    UInt8 *writePtr = (UInt8 *)outputBuffer;
    status = CCCryptorUpdate(cryptor, data, length, writePtr, outputLength, &bytesOutput);
    if (status != kCCSuccess)
    {
        NSLog(@"CCCryptorUpdate(): error:%d", status);
        CCCryptorRelease(cryptor);
        free(outputBuffer);
        return NO;
    }
    writePtr += bytesOutput;
    outputLength -= bytesOutput;
    totalBytesOutput += bytesOutput;
    
    status = CCCryptorFinal(cryptor, writePtr, outputLength, &bytesOutput);
    if (status != kCCSuccess)
    {
        NSLog(@"CCCryptorFinal(): error:%d", status);
        CCCryptorRelease(cryptor);
        free(outputBuffer);
        return NO;
    }
    totalBytesOutput += bytesOutput;
    
    *outData = outputBuffer;
    *outDataLength = totalBytesOutput;
    
    CCCryptorRelease(cryptor);
    return YES;
}

@implementation BNRDataBuffer (Encryption)

- (BOOL)decryptWithKey:(NSString *)key salt:(const BNRSalt *)salt
{
    if (key == nil || [key length] == 0)
        return YES;
    
    BOOL ret = YES;

    BNRSalt littleSalt = *salt;
    BNRSaltToLittle(&littleSalt);
    
    void *decryptedBuffer = NULL;
    size_t decryptedBufferLength;
    BOOL success = CryptHelper(key, &littleSalt, kCCDecrypt, buffer, length, &decryptedBuffer, &decryptedBufferLength);

    size_t saltSize = sizeof(littleSalt.word);
    if (success && decryptedBufferLength >= saltSize && memcmp(decryptedBuffer, &littleSalt.word, saltSize) == 0)
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

- (void)encryptWithKey:(NSString *)key salt:(const BNRSalt *)salt
{
    if (key == nil || [key length] == 0)
        return;

    BNRSalt littleSalt = *salt;
    BNRSaltToLittle(&littleSalt);
    
    // Create a scratch buffer so we can prepend the 8 bytes of salt.
    // We will look for these salt bytes when decrypting to confirm that we were successful.
    int scratchBufferLength = 8 + length;
    UInt8 *scratchBuffer = (UInt8*)malloc(scratchBufferLength);
    memcpy(scratchBuffer, littleSalt.word, 8);
    memcpy(scratchBuffer + 8, buffer, length);
    
    void *encryptedBuffer = NULL;
    size_t encryptedBufferLength = 0;

    CryptHelper(key, &littleSalt, kCCEncrypt, scratchBuffer, scratchBufferLength, &encryptedBuffer, &encryptedBufferLength);

    free(scratchBuffer);
    
    [self setData:encryptedBuffer length:encryptedBufferLength];
}

@end
