//
//  BNRClassDictionaryEnumerator.h
//  TCSpeedTest
//
//  Created by Aaron Hillegass on 1/27/10.
//  Copyright 2010 Big Nerd Ranch. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "BNRClassDictionary.h"

#include <ext/hash_map> using namespace stdext;
using std::pair;
using namespace __gnu_cxx;


@interface BNRClassDictionaryEnumerator : NSObject {
    hash_map<Class, id,hash<Class>, equal_to<Class> >::iterator iter;
    hash_map<Class, id, hash<Class>, equal_to<Class> > *mapTable;
}

- (id)initWithTable:(hash_map<Class, id, hash<Class>, equal_to<Class> > *)t;


@end
