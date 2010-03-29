//
//  BNREntity.h
//  TCSpeedTest
//
//  Created by Aaron Hillegass on 2/4/10.
//  Copyright 2010 Big Nerd Ranch. All rights reserved.
//

#import <Foundation/Foundation.h>
@class BNRAttribute;
@class BNRRelationship;

@interface BNREntity : NSObject {
    NSDictionary *attributes;
    NSDictionary *relationships;
}
- (NSDictionary *)attributes;
- (BNRAttribute *)attributeNamed:(NSString *)name;

- (NSDictionary *)relationships;
- (BNRRelationship *)relationshipNamed:(NSString *)name;

@end
