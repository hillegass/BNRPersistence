//
//  BNRClassDictionaryEnumerator.h
//  TCSpeedTest
//
//  Created by Aaron Hillegass on 1/27/10.
//  Copyright 2010 Big Nerd Ranch. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "BNRUniquingTable.h"

#include <ext/hash_map> using namespace stdext;
using std::pair;
using namespace __gnu_cxx;


@interface BNRUniquingTableEnumerator : NSObject {
    hash_map<PKey, id>::iterator iter;
    hash_map<PKey, id> *mapTable;
}

- (id)initWithTable:(hash_map<PKey, id>  *)t;
@end
