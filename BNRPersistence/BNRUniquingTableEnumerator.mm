//
//  BNRClassDictionaryEnumerator.mm
//  TCSpeedTest
//
//  Created by Aaron Hillegass on 1/27/10.
//  Copyright 2010 Big Nerd Ranch. All rights reserved.
//

#import "BNRUniquingTableEnumerator.h"


@implementation BNRUniquingTableEnumerator

- (id)initWithTable:(hash_map<PKey, id>  *)t;
{
    [super init];
    mapTable = t;
    iter = mapTable->begin();
    return self;
}

- (id)nextObject
{
    if (iter == mapTable->end()) {
        return nil;
    }
    UniquingHashedPair currentPair = *iter;
    id obj = currentPair.second;
    iter++;
    return obj;
}

@end
