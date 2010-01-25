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

#import "Playlist.h"
#import "Song.h"
#import "BNRDataBuffer.h"

@implementation Playlist


- (void)dealloc
{
    [title release];
    [songs release];
    [super dealloc];
}

#pragma mark Accessors

- (NSString *)title
{
    [self checkForContent];
    return title;
}

- (void)setTitle:(NSString *)t
{
    t = [t copy];
    [title release];
    title = t;
}

- (void)insertObject:(Song *)s inSongsAtIndex:(NSUInteger)i
{
    if (!songs) {
        songs = [[NSMutableArray alloc] init];
    }
    [songs insertObject:s atIndex:i];
}
- (void)removeObjectFromSongsAtIndex:(NSUInteger)idx
{
    [songs removeObjectAtIndex:idx];
}

- (NSArray *)songs
{
    [self checkForContent];
    return songs;
}

#pragma mark Read and Writing

- (void)readContentFromBuffer:(BNRDataBuffer *)d
{
    [title release];
    title = [[d readString] retain];
    
    [songs release];
    songs = [d readArrayOfClass:[Song class]
                     usingStore:[self store]];
    [songs retain];
}

- (void)writeContentToBuffer:(BNRDataBuffer *)d
{
    [d writeString:title];
    [d writeArray:songs ofClass:[Song class]];
}

@end
