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

#import "MyDocument.h"
#import "GRLGraphView.h"
#import "GRLEdge.h"
#import "GRLGraph.h"
#import "GRLNode.h"
#import "BNRStore.h"
// For timing
#import <mach/mach_time.h>

#define SIZE 300
/*
NSRect expandRectToIncludePoint(NSRect r, NSPoint p)
{
    NSRect result = r;
    if (p.x < r.origin.x) {
        result.origin.x = p.x;
        result.size.width += r.origin.x - p.x;
    } else if (p.x > r.origin.x + r.size.width) {
        result.size.width = p.x - r.origin.x;
    }
    
    if (p.y < r.origin.y) {
        result.origin.y = p.y;
        result.size.height += r.origin.y - p.y;
    } else if (p.y > r.origin.y + r.size.height) {
        result.size.height = p.y - r.origin.y;
    }
    return result;
}
*/
float distanceBetweenPoints(NSPoint a, NSPoint b)
{
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    return sqrt(dx * dx + dy * dy);
}
@implementation MyDocument

- (id)init
{
    self = [super init];
    if (self) {
        [[self store] addClass:[GRLEdge class]];
        [[self store] addClass:[GRLNode class]];
        graph = [[GRLGraph alloc] init];
        NSLog(@"graph = %@", graph);
        [[self store] setDelegate:self];
    }
    return self;
}

- (void)dealloc
{
    [store dissolveAllRelationships];
    [graph release];
    [super dealloc];
}

- (NSString *)windowNibName
{
    // Override returning the nib file name of the document
    // If you need to use a subclass of NSWindowController or if your document supports multiple NSWindowControllers, you should remove this method and override -makeWindowControllers instead.
    return @"MyDocument";
}

- (void)windowControllerDidLoadNib:(NSWindowController *) aController
{
    [super windowControllerDidLoadNib:aController];
    [graphView setGraph:graph];
}



- (void)jiggle:(id)sender
{
    NSArray *nodes = [graph nodes];
    int count = [nodes count];
    if (count == 0) {
        return;
    }
    int i;
    for (i = 0 ; i < 3; i++) {
        float deltaX, deltaY;
        deltaX = (float)(random() % 50 - 25);
        deltaY = (float)(random() % 50 - 25);
        int nodeIndex = random() % count;
        GRLNode *n = [nodes objectAtIndex:nodeIndex];
        NSPoint p = [n point];
        p.x += deltaX;
        p.y += deltaY;
        [store willUpdateObject:n];
        [n setPoint:p];
    }
}


- (void)scramble:(id)sender
{
    // Create some nodes
    int i;
    for (i = 0; i < 50; i++) {
        NSPoint p;
        p.x = (float)(random() % SIZE - SIZE/2);
        p.y = (float)(random() % SIZE - SIZE/2);
        GRLNode *n = [[GRLNode alloc] init];
        [n setPoint:p];
        [[self store] insertObject:n];
        [n release];
    }
    
    NSArray *nodes = [graph nodes];
    int count = [nodes count];
    for (i = 0; i < 75; i++) {
        int aIndex = random() % count;
        int bIndex = random() % count;
        GRLNode *aNode = [nodes objectAtIndex:aIndex];
        GRLNode *bNode = [nodes objectAtIndex:bIndex];
        float distance = distanceBetweenPoints([aNode point], [bNode point]);
        if (distance > 150) {
            continue;
        }
        GRLEdge *e = [[GRLEdge alloc] init];
        [e setSourceNode:aNode];
        [e setDestinationNode:bNode];
        
        // This inserts it into the graph
        [[self store] insertObject:e];
        [e release];
    }
}

- (void)cull:(id)sender
{
    // Remove some nodes
    int i;
    for (i = 0 ; i < 2; i++) {
        NSArray *nodes = [graph nodes];
        int count = [nodes count];
        if (count == 0) {
            return;
        }
        int nodeIndex = random() % count;
        GRLNode *n = [nodes objectAtIndex:nodeIndex];   
        
        // GRLNode's prepareForDelete will cascade delete
        // to delete attached edges
        [store deleteObject:n];
    }
    
}


- (void)readFromStore
{
    // Time the read
    mach_timebase_info_data_t info;
    uint64_t start, end, elapsed;
    start = mach_absolute_time();
    if (mach_timebase_info (&info) != KERN_SUCCESS) {
        printf ("mach_timebase_info failed\n");
    }
    
    NSMutableArray *nodes = [store allObjectsForClass:[GRLNode class]];
    NSMutableArray *edges = [store allObjectsForClass:[GRLEdge class]];

    end = mach_absolute_time();
    elapsed = end - start;
    
    uint64_t nanos;
    nanos = elapsed * info.numer / info.denom;
    uint64_t millis = nanos / 1000000;
    printf ("elapsed time to load %lu nodes, %lu edges was %llu milliseconds\n", [nodes count], [edges count], millis);
    
    [graph release];
    graph = [[GRLGraph alloc] init];
    [graph setNodes:nodes];
    [graph setEdges:edges];
    
    [graphView setGraph:graph];
}

- (void)store:(BNRStore *)st willInsertObject:(BNRStoredObject *)so
{
    // NSLog(@"Adding to graph %@", so);
    if ([so class] == [GRLEdge class]) {
        [graph addEdge:(GRLEdge *)so];
    } else {
        [graph addNode:(GRLNode *)so];
    }
    
    [graphView setNeedsDisplay:YES];

}
- (void)store:(BNRStore *)st willDeleteObject:(BNRStoredObject *)so
{
    // NSLog(@"Removing from graph %@", so);
    if ([so isKindOfClass:[GRLEdge class]]) {
        [graph removeEdge:(GRLEdge *)so];
    } else {
        [graph removeNode:(GRLNode *)so];
    }
    
    [graphView setNeedsDisplay:YES];

}
- (void)store:(BNRStore *)st willUpdateObject:(BNRStoredObject *)so
{
    //NSLog(@"MyDocument: willUpdate %@", so);
    [graphView setNeedsDisplay:YES];
}

- (void)store:(BNRStore *)st didChangeObject:(BNRStoredObject *)so;
{
    //NSLog(@"MyDocument: willUpdate %@", so);
    [graphView setNeedsDisplay:YES];
}



@end
