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

#import "BNRClassDictionary.h"

@implementation BNRClassDictionary

- (id)init
{
    self = [super init];
    if (self) {
		mapTable = new hash_map<Class, id, hash<Class>, equal_to<Class> >(389);
    }
    return self;
}
- (void)dealloc
{
#if (1)  // silence a compiler complaint - but better double-check this
	typedef hash_map<Class, id, hash<Class>, equal_to<Class> > bnr_hashmap_t;
	for(bnr_hashmap_t::iterator i(mapTable->begin()), j(mapTable->end()); i!=j; ++i){ 
		//NSLog(@"class:%@ id:%p", NSStringFromClass((id)i->first), i->second);
		[(id)i->second release];
	}
#else // original code
	hash_map<Class, id, hash<Class>, equal_to<Class> >::iterator iter = mapTable->begin();
    while (iter != mapTable->end()) {
		[(id)iter->second release];
        iter++;
    }
    mapTable->clear();
#endif
	
    delete mapTable;
    [super dealloc];
}

- (void)setObject:(id)obj forClass:(Class)c
{
   // NSMapInsert(mapTable, c, obj);
    id oldValue = (*mapTable)[c];
    if (oldValue == obj) {
        return;
    }
    [obj retain];
    [oldValue release];
    (*mapTable)[c] = obj;
}

- (id)objectForClass:(Class)c
{
    return (*mapTable)[c];
}
@end
