//
//  IntDictionaryTests.m
//  StoreTest
//
//  Created by Aaron Hillegass on 6/26/06.
//  Copyright 2006 Big Nerd Ranch. All rights reserved.
//

#import "IntDictionaryTests.h"
#import "BNRIntDictionary.h"
#define ITEM_COUNT 500

@implementation IntDictionaryTests

- (void)testDict
{
    BNRIntDictionary *dict = [[BNRIntDictionary alloc] init];
    int i;
    for (i = 0; i < ITEM_COUNT; i++) {
        NSString *foo;
        UInt32 anIndex = random();
        foo = [NSString stringWithFormat:@"test %u", anIndex];
        [dict setObject:foo forInt:anIndex];
        NSString *s = [dict objectForInt:anIndex];
        STAssertEqualObjects(s,foo, @"read != write");        
    }
    [dict release];
 }

@end
