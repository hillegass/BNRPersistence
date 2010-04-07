//
//  IntegrationTests.h
//  EncryptionTest
//
//  Created by Adam Preble on 4/6/10.
//  Copyright 2010 Big Nerd Ranch. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#import <SenTestingKit/SenTestingKit.h>

@class BNRStore;

@interface IntegrationTests : SenTestCase {
    BNRStore *store;
}

@end
