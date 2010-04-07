//
//  Person.h
//  EncryptionTest
//
//  Created by Adam Preble on 4/6/10.
//  Copyright 2010 Big Nerd Ranch. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#import "BNRStoredObject.h"

@interface Person : BNRStoredObject
{
    NSString *name;
}
@property (nonatomic, retain) NSString *name;
@end

