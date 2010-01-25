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

#import "GRLGraph.h"


@implementation GRLGraph

- (id)init
{
    [super init];
    nodes = [[NSMutableArray alloc] init];
    edges = [[NSMutableArray alloc] init];
    return self;
}

- (void)dealloc
{
    if ([nodes count] == 0) {
        NSLog(@"deallocing empty graph");
    } else {
        NSLog(@"deallocing non-empty graph");
    }
    [nodes release];
    [edges release];
    [super dealloc];
}

- (void)addNode:(GRLNode *)n
{
    [nodes addObject:n];
}
- (void)removeNode:(GRLNode *)n
{
    [nodes removeObjectIdenticalTo:n];
}


- (void)addEdge:(GRLEdge *)e
{
    [edges addObject:e];
}
- (void)removeEdge:(GRLEdge *)n
{
    [edges removeObjectIdenticalTo:n];
}
- (NSArray *)nodes
{
    return nodes;
}
- (NSArray *)edges
{
    return edges;
}

- (void)setNodes:(NSArray *)nodeArray
{
    [nodes release];
    nodes = [nodeArray mutableCopy];
}

- (void)setEdges:(NSArray *)edgesArray
{
    [edges release];
    edges = [edgesArray mutableCopy];
}

@end
