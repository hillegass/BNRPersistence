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
#import "GRLNode.h"
#import "BNRDataBuffer.h"
#import "GRLEdge.h"


@implementation GRLNode
+ (BOOL)automaticallyNotifiesObserversForKey:(NSString *)key
{
    return NO;
}
- (id)init
{
    [super init];
    arrivingEdges = [[NSMutableSet alloc] init];
    departingEdges = [[NSMutableSet alloc] init];
    return self;
}
- (void)dealloc
{
    [arrivingEdges release];
    [departingEdges release];
    [super dealloc];
}
- (void)readContentFromBuffer:(BNRDataBuffer *)d
{
    point.x = [d readFloat32];
    point.y = [d readFloat32];
}
- (void)writeContentToBuffer:(BNRDataBuffer *)d
{
    [d writeFloat32:point.x];
    [d writeFloat32:point.y];
}
- (void)dissolveRelationships
{
    [arrivingEdges release];
    arrivingEdges = nil;
    
    [departingEdges release];
    departingEdges = nil;
}

- (void)prepareForDelete
{
    GRLEdge *obj;

    while (obj = [departingEdges anyObject]) {
        [[self store] deleteObject:obj];
        [obj setSourceNode:nil];
        [obj setDestinationNode:nil];
    }
    
    while (obj = [arrivingEdges anyObject]) {
        [[self store] deleteObject:obj];
        [obj setSourceNode:nil];
        [obj setDestinationNode:nil];
    }
}

- (void)setPoint:(NSPoint)p
{
    [self willChangeValueForKey:@"point"];
    point = p;
    [self didChangeValueForKey:@"point"];
}
- (NSPoint)point
{
    return point;
}

- (NSSet *)arrivingEdges
{
    return arrivingEdges;
}

- (NSSet *)departingEdges
{
    return departingEdges;
}

- (NSString *)description
{
    return [NSString stringWithFormat:@"<GRLNode %d: %@>", rowID, NSStringFromPoint(point)];
}
@end
