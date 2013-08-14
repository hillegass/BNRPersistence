//
//  GeneralTests.m
//  EncryptionTest
//
//  Created by Adam Preble on 4/9/10.
//  Copyright 2010 Big Nerd Ranch. All rights reserved.
//

#import <SenTestingKit/SenTestingKit.h>
#import "Person.h"
#import "BNRTCBackend.h"
#import "BNRStore.h"

extern NSString * const IntegrationTestsDatabasePath;

@interface GeneralTests : SenTestCase {
    BNRStore *store;
}
- (void)reopen;
@end

@implementation GeneralTests

- (void)setUp
{
    [[NSFileManager defaultManager] removeItemAtPath:IntegrationTestsDatabasePath error:nil]; // erase in case we were debugging and uncerimoniously exited
    [self reopen];
}
- (void)tearDown
{
    [store release];
    [[NSFileManager defaultManager] removeItemAtPath:IntegrationTestsDatabasePath error:nil];
}

- (void)reopen
{
    [store release];
    
    NSError *error;
    NSString *path = IntegrationTestsDatabasePath;
    
    BNRTCBackend *backend = [[BNRTCBackend alloc] initWithPath:path
                                                         error:&error];
    if (!backend) {
        NSLog(@"Unable to create database at %@", path);
    }
    
    store = [[BNRStore alloc] init];
    [store setBackend:backend];
    [backend release];
    
    [store addClass:[Person class]];
}

- (void)testCreation
{
    STAssertNotNil(store, @"Store not created");
}


#if NS_BLOCKS_AVAILABLE
- (void)testBasicBlocks
{
    [store insertObject:[Person personWithName:@"Fred"]];
    [store insertObject:[Person personWithName:@"Alice"]];
    [store insertObject:[Person personWithName:@"Q"]];
    
    NSError *error;
    [store saveChanges:&error];
    
    
    // Enumerate the people and put a suffix of "1" on the names of the first two:
    
    __block int blockInvocations = 0;
    [store enumerateAllObjectsForClass:[Person class] usingBlock:^(UInt32 rowID, BNRStoredObject *object, BOOL *stop) {
        blockInvocations++;
        STAssertEquals([Person class], [object class], @"Bad class");
        [store willUpdateObject:object];
        Person *person = (Person *)object;
        [person setName:[[person name] stringByAppendingString:@"1"]];
        *stop = (blockInvocations == 2);
    }];
    
    // Check our results:
    STAssertEquals(blockInvocations, 2, @"Expected block to be called twice");
    
    NSArray *people = [store allObjectsForClass:[Person class]];
    STAssertNotNil(people, @"allObjectsForClass returned nil");
    STAssertEquals([people count], (NSUInteger)3, @"expected number of people");
    int countOfSuffix1 = [[people indexesOfObjectsPassingTest:^(id object, NSUInteger idx, BOOL *stop) { 
        return [[object valueForKey:@"name"] hasSuffix:@"1"]; 
    }] count];
    STAssertEquals(countOfSuffix1, 2, @"Expected 2 with suffix \"1\".");
}
#endif

@end
