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
#import "BNRIndexManager.h"
#include <dystopia.h>

@interface BNRTCIndexManager : BNRIndexManager {
    NSString *path;
    NSMutableDictionary *textIndexes;
	BOOL usesIndexFileWriteSync;		// will cause every write to sync to the physical storage media; slower, but much less fragile in cases of crash or power outage
	BOOL usesIndexFileCompression;		// will compress offline TC index files; index files can go from a default of ~8.5MB to as little as 12KB!
}
@property (readonly) BOOL usesIndexFileWriteSync;
@property (readonly) BOOL usesIndexFileCompression;

- (id)initWithPath:(NSString *)p useWriteSyncronization:(BOOL)useWriteSyncFlag compressIndexFiles:(BOOL)compressIndexFilesFlag error:(NSError **)err; // designated initializer
- (id)initWithPath:(NSString *)p error:(NSError **)err;

- (TCIDB *)textIndexForClass:(Class)c 
                         key:(NSString *)k;
- (NSString *)path;
- (void)close;

@end
