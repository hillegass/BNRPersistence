//
//  BNRCrypto.m
//  PhoneSpeedTest
//
//  Created by Adam Preble on 4/8/10.
//  Copyright 2010 Big Nerd Ranch. All rights reserved.
//

#import "BNRCrypto.h"

#if TARGET_OS_IPHONE // iPhone:

#import <Security/Security.h>

void BNRRandomBytes(void *buffer, int length)
{
    SecRandomCopyBytes(kSecRandomDefault, length, (uint8_t*)buffer);
}

#else // Mac:

#import <openssl/rand.h>
#import <libkern/OSTypes.h>

void BNRRandomBytes(void *buffer, int length)
{
    RAND_pseudo_bytes((UInt8*)buffer, length);
}
#endif
