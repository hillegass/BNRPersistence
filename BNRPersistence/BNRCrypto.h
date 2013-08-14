//
//  BNRCrypto.h
//  PhoneSpeedTest
//
//  Created by Adam Preble on 4/8/10.
//  Copyright 2010 Big Nerd Ranch. All rights reserved.
//
#import <sys/types.h>

/*! Fills |buffer| with |length| random bytes. */
void BNRRandomBytes(void *buffer, size_t length);