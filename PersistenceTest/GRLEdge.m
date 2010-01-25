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

#import "GRLEdge.h"
#import "BNRDataBuffer.h"
#import "GRLNode.h"

@interface GRLNode (EdgeChanges)
- (void)addObjectToArrivingEdges:(GRLEdge *)g;
- (void)removeObjectFromArrivingEdges:(GRLEdge *)g;
- (void)addObjectToDepartingEdges:(GRLEdge *)g;
- (void)removeObjectFromDepartingEdges:(GRLEdge *)g;
@end

@implementation GRLNode (EdgeChanges)

- (void)addObjectToArrivingEdges:(GRLEdge *)g
{
    [arrivingEdges addObject:g];
}

- (void)removeObjectFromArrivingEdges:(GRLEdge *)g
{
    [arrivingEdges removeObject:g];
}

- (void)addObjectToDepartingEdges:(GRLEdge *)g
{
    [departingEdges addObject:g];
}

- (void)removeObjectFromDepartingEdges:(GRLEdge *)g
{
    [departingEdges removeObject:g];
}

@end


@implementation GRLEdge

- (void)setSourceNode:(GRLNode *)n
{
    [n retain];
    [sourceNode removeObjectFromDepartingEdges:self];
    [sourceNode release];
    sourceNode = n;
    [sourceNode addObjectToDepartingEdges:self];
}

- (void)setDestinationNode:(GRLNode *)n
{
    [n retain];
    [destinationNode removeObjectFromArrivingEdges:self];
    [destinationNode release];
    destinationNode = n;
    [destinationNode addObjectToArrivingEdges:self];
}
- (void)dealloc
{
    [self setSourceNode:nil];
    [self setDestinationNode:nil];
    [label release];
    [super dealloc];
}

- (GRLNode *)sourceNode
{
    return sourceNode;
}
- (GRLNode *)destinationNode
{
    return destinationNode;
}

- (NSString *)label
{
    return label;
}
- (void)setLabel:(NSString *)s
{
    s = [s copy];
    [label release];
    label = s;
}

- (void)readContentFromBuffer:(BNRDataBuffer *)d
{
    [self setSourceNode:(GRLNode *)[d readObjectReferenceOfClass:[GRLNode class]
                                                      usingStore:[self store]]];
    
    [self setDestinationNode:(GRLNode *)[d readObjectReferenceOfClass:[GRLNode class]
                                                           usingStore:[self store]]];

    
}
- (void)writeContentToBuffer:(BNRDataBuffer *)d
{
    [d writeObjectReference:sourceNode];
    [d writeObjectReference:destinationNode];
}

- (void)dissolveRelationships
{
    [sourceNode release];
    sourceNode = nil;
    
    [destinationNode release];
    destinationNode = nil;
}

- (void)prepareForDelete
{
    [self setSourceNode:nil];
    [self setDestinationNode:nil];
}

- (NSString *)description
{
    return [NSString stringWithFormat:@"<GRLEdge %d: %d to %d>", rowID, [sourceNode rowID], [destinationNode rowID]];
}

@end
