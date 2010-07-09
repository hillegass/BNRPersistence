// The MIT License
//
// Copyright (c) 2008 Big Nerd Ranch, Inc.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#import "BNRStoreBackend.h"
#include <tcutil.h>
#include <tchdb.h>

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


typedef pair<Class, TCHDB *> TCFileHashedPair;

#endif 

@interface BNRTCBackend : BNRStoreBackend {
    NSString *path;
#ifdef __cplusplus
    hash_map<Class, TCHDB *, hash<Class>, equal_to<Class> > *dbTable;
#else
    void *dbTable; 
#endif
    TCHDB *namedBufferDB;
}
- (id)initWithPath:(NSString *)p error:(NSError **)err;
- (TCHDB *)fileForClass:(Class)c;
- (NSString *)path;

@end
