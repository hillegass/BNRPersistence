//
//  BNRClassMetaData+Encryption.h
//  EncryptionTest
//
//  Created by Adam Preble on 4/6/10.
//  Copyright 2010 Big Nerd Ranch. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#import "BNRClassMetaData.h"

@interface BNRClassMetaData (Encryption)

/*!
 @method setEncryptionKeyHashForKey
 @abstract Sets the appropriate encryptionKeyHash for the given encryptionKey.
 */
- (void)setEncryptionKeyHashForKey:(NSString *)encryptionKey;

/*!
 @method encryptionKeyMatches
 @abstract Returns YES if the given key matches the encryptionKeyHash.
 */
- (BOOL)hashMatchesEncryptionKey:(NSString *)encryptionKey;

@end
