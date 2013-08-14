//
//  BNRCrypto.m
//  PhoneSpeedTest
//
//  Created by Adam Preble on 4/8/10.
//  Copyright 2010 Big Nerd Ranch. All rights reserved.
//
#import <ConditionalMacros.h>
#import <AvailabilityMacros.h>

#import "BNRCrypto.h"

#if TARGET_OS_IPHONE || (TARGET_OS_MAC && (MAC_OS_X_VERSION_MAX_ALLOWED > 1070)) 
// Note that macros such as MAC_OS_X_VERSION_10_7 don't exist in earlier SDKs; Apple recommends using the numeric value

#import <Security/SecRandom.h>

void BNRRandomBytes(void *buffer, size_t length)
{
    SecRandomCopyBytes(kSecRandomDefault, length, (uint8_t*)buffer);
}

#else // Older OS X

#import <openssl/rand.h>
#import <libkern/OSTypes.h>

void BNRRandomBytes(void *buffer, size_t length)
{
    /* This function was deprecated with OS X 10.7. */
    RAND_pseudo_bytes((UInt8*)buffer, (int)length);
}
#endif
