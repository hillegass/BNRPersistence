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

#import <Foundation/Foundation.h>

#ifdef __cplusplus
#include <ext/hash_map> using namespace stdext;
using std::pair;
using namespace __gnu_cxx;

typedef pair<Class, UInt32> PKey;

namespace __gnu_cxx {
    template<>
    struct hash<PKey>
    {
        size_t operator()(const PKey keyValue) const
        {
            return (size_t)keyValue.first + keyValue.second;
        };
    };
}    

//struct UniquingHashConfiguration{
//    enum {              
//        bucket_size = 4,  // 0 < bucket_size
//        min_buckets = 262144   // min_buckets = 2 ^^ N, 0 < N
//    }; 
//    
//    size_t operator()(const PKey keyValue) const {
//        return (size_t)keyValue.first + keyValue.second;        
//    }
//    
//    bool operator()(const PKey left, const PKey right) const {
//        //here should be the code to compare two VALUE_KEY_CLASS objects
//        // 
//        return (left.second == right.second) && (left.first == right.first);
//    }
//};

typedef pair<PKey, id> UniquingHashedPair;

#endif 

@interface BNRUniquingTable : NSObject {
#ifdef __cplusplus
    hash_map<PKey, id > *mapTable;
#else
    void *mapTable; 
#endif
    
}
- (id)objectForClass:(Class)c rowID:(UInt32)row;
- (void)setObject:(id)obj forClass:(Class)c rowID:(UInt32)row;
- (void)removeObjectForClass:(Class)c rowID:(UInt32)row;
- (NSEnumerator *)objectEnumerator;
- (NSUInteger)count;

@end
