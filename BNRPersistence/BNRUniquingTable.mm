//
//  BNRUniquingTable.m
//  TCSpeedTest
//
//  Created by Aaron Hillegass on 1/28/10.
//  Copyright 2010 Big Nerd Ranch. All rights reserved.
//

#import "BNRUniquingTable.h"
#import "BNRUniquingTableEnumerator.h"


@implementation BNRUniquingTable
- (id)init
{
    [super init];
    mapTable = new hash_map<PKey, id>(786433);
    return self;
}
- (void)dealloc
{
    delete mapTable;
    [super dealloc];
}

- (void)setObject:(id)obj forClass:(Class)c rowID:(UInt32)row
{
    PKey pk;
    pk.first = c; pk.second = row;
    (*mapTable)[pk] = obj;
}

- (id)objectForClass:(Class)c rowID:(UInt32)row;
{
    PKey pk;
    pk.first = c; pk.second = row;
    return (*mapTable)[pk];
}

- (void)removeObjectForClass:(Class)c rowID:(UInt32)row
{
    PKey pk;
    pk.first = c; pk.second = row;
    mapTable->erase(pk);
}

- (NSUInteger)count
{
    return mapTable->size();
}

- (NSEnumerator *)objectEnumerator
{
    BNRUniquingTableEnumerator *result = [[BNRUniquingTableEnumerator alloc] initWithTable:mapTable];
    return [result autorelease];
    
}

@end
