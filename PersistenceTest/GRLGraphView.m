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
#import "GRLGraphView.h"
#import "GRLGraph.h"
#import "GRLNode.h"
#import "GRLEdge.h"


@implementation GRLGraphView

- (id)initWithFrame:(NSRect)f
{
    [super initWithFrame:f];
    [self setBounds:NSMakeRect(-150, -150, 300, 300)];
    return self;
}
- (void)dealloc
{
    [graph release];
    [super dealloc];
}
- (void)setGraph:(GRLGraph *)g
{
    if (g == graph) {
        return;
    }
    [graph release];
    [g retain];
    graph = g;
    [self setNeedsDisplay:YES];
}

- (void)setFrameSize:(NSSize)s
{
    [super setFrameSize:s];
    [self setBounds:NSMakeRect(-150, -150, 300, 300)];
}

- (void)drawRect:(NSRect)rect
{
    NSRect bounds = [self bounds];
    [[NSColor darkGrayColor] set];
    [NSBezierPath fillRect:bounds];
    
    [[NSColor redColor] set];
    NSArray *nodes = [graph nodes];
    int i, count;
    count = [nodes count];
    if (count < 400) {
        NSRect r = NSMakeRect(0,0, 6, 6);
        for (i = 0; i < count; i++) {
            GRLNode *n = [nodes objectAtIndex:i];
            NSPoint np = [n point];
            r.origin.x = np.x - 3;
            r.origin.y = np.y - 3;
            [NSBezierPath fillRect:r];
        }        
    } else {
        NSLog(@"Not drawing nodes, %d is too many", count);
    }
 
    NSArray *edges = [graph edges];
    count = [edges count];
    if (count < 700)
    {
        [[NSColor whiteColor] set];
        [NSBezierPath setDefaultLineWidth:0.9];
        for (i = 0; i < count; i++) {
            GRLEdge *e = [edges objectAtIndex:i];
            GRLNode *n1 = [e sourceNode];
            NSPoint np1 = [n1 point];
            GRLNode *n2 = [e destinationNode];
            NSPoint np2 = [n2 point];
            [NSBezierPath strokeLineFromPoint:np1
                                      toPoint:np2];
        }        
    } else {
        NSLog(@"Not drawing edges, %d is too many", count);
    }
}

@end
