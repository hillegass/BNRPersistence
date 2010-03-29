//
//  BNRModel.h
//  TCSpeedTest
//
//  Created by Aaron Hillegass on 2/4/10.
//  Copyright 2010 Big Nerd Ranch. All rights reserved.
//

#import <Foundation/Foundation.h>
@class BNREntity;

#ifdef __cplusplus
#include <ext/hash_map> using namespace stdext;
using std::pair;
using namespace __gnu_cxx;

namespace __gnu_cxx {
    template<>
    struct hash<Class>
    {
        size_t operator()(const Class ptr) const
        {
            return (size_t)ptr;
        };
    };
}    


typedef pair<Class, BNREntity *> EntityHashedPair;

#endif

@interface BNRModel : NSObject {
#ifdef __cplusplus
    hash_map<Class, BNREntity *, hash<Class>, equal_to<Class> > *entityTable;
#else
    void *entityTable; 
#endif
}
- (id)initWithXML:(NSString *)path error:(NSError **)e;
- (BNREntity *)entityForClass:(Class)c;
- (void)setEntity:(BNREntity *)e forClass:(Class)c;
@end
