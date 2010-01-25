//
//  ClassDictionaryTestCase.m
//  StoreTest
//
//  Created by Aaron Hillegass on 6/26/06.
//  Copyright 2006 Big Nerd Ranch. All rights reserved.
//

#import "ClassDictionaryTestCase.h"
#import "BNRClassDictionary.h"

@implementation ClassDictionaryTestCase

- (void)testDict
{
    BNRClassDictionary *dict = [[BNRClassDictionary alloc] init];
    NSString *foo;
    UInt32 anIndex = random();
    foo = [NSString stringWithFormat:@"test %u", anIndex];
    [dict setObject:foo forClass:[self class]];
    NSString *s = [dict objectForClass:[self class]];
    STAssertEqualObjects(s,foo, @"read != write");        
    [dict release];
}


@end
