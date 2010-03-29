//
//  BNREntity.m
//  TCSpeedTest
//
//  Created by Aaron Hillegass on 2/4/10.
//  Copyright 2010 Big Nerd Ranch. All rights reserved.
//

#import "BNREntity.h"


@implementation BNREntity

- (id)initWithDictionary:(NSDictionary *)d
{
    [super init];
    
    
    
    return self;
}

- (NSDictionary *)attributes
{
    return attributes;
}
- (BNRAttribute *)attributeNamed:(NSString *)name
{
    return [attributes objectForKey:name];
}

- (NSDictionary *)relationships
{
    return relationships;
}
- (BNRRelationship *)relationshipNamed:(NSString *)name
{
    return [relationships objectForKey:name];
}

@end
