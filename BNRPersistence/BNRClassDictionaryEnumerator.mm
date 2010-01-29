//
//  BNRClassDictionaryEnumerator.mm
//  TCSpeedTest
//
//  Created by Aaron Hillegass on 1/27/10.
//  Copyright 2010 Big Nerd Ranch. All rights reserved.
//

#import "BNRClassDictionaryEnumerator.h"


@implementation BNRClassDictionaryEnumerator

- (id)initWithTable:(hash_map<Class, id, hash<Class>, equal_to<Class> > *)t
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
    HashedPair currentPair = *iter;
    id obj = currentPair.second;
    iter++;
    return obj;
}

//hash_map<Class, id, HashConfiguration<Class> >::iterator iter = mapTable->begin();
//while (iter != mapTable->end()) {
//    HashedPair currentPair = *iter;
//    id obj = currentPair.second;
//    [obj performSelector:s];
//    iter++;
//}

@end
