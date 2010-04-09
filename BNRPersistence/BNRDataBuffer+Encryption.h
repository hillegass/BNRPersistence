//
//  BNRDataBuffer+Encryption.h
//  EncryptionTest
//
//  Created by Adam Preble on 4/6/10.
//  Copyright 2010 Big Nerd Ranch. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "BNRDataBuffer.h"

@interface BNRDataBuffer (Encryption)

- (BOOL)decryptWithKey:(NSString *)key salt:(const UInt32 *)salt;
- (void)encryptWithKey:(NSString *)key salt:(const UInt32 *)salt;

@end
