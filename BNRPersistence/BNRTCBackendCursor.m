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

#import "BNRTCBackendCursor.h"
#import "BNRDataBuffer.h"

@implementation BNRTCBackendCursor

- (id)initWithFile:(TCHDB *)f
{
    [super init];
    file = f;
    bool successful = tchdbiterinit(file);
    if (!successful) {
        int ecode = tchdbecode(file);
        NSLog(@"Bad tchdbiterinit in initWithFile: %s", tchdberrmsg(ecode));
    }
    
    return self;
}

- (UInt32)nextBuffer:(BNRDataBuffer *)buff
{
    UInt32 result;
    UInt32 *buffer;
    int size;
    buffer = (UInt32 *)tchdbiternext(file, &size);

    if (!buffer) {
        return 0;
    }
    
    result = CFSwapInt32LittleToHost(*buffer);
    
    int bufferSize;
    void *data = tchdbget(file, buffer, sizeof(UInt32), &bufferSize);
        
    [buff setData:data
           length:bufferSize];
    
    return result;
}

@end
