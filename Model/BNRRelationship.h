//
//  BNRRelationship.h
//  TCSpeedTest
//
//  Created by Aaron Hillegass on 2/4/10.
//  Copyright 2010 Big Nerd Ranch. All rights reserved.
//

#import <Foundation/Foundation.h>

enum {
    BNRToOneRelationship = 1,
    BNRUnorderedToManyRelationship,
    BNROrderedToManyRelationship
};

enum {
    BNRNullifyDeleteRule = 1,
    BNRCascadeDeleteRule,
    BNRNoneDeleteRule
};

@interface BNRRelationship : NSObject {
    NSString *destinationEntityName;
    int type;
    NSString *inverseRelationshipName;
    int deleteRule;
}
@property (nonatomic, copy) NSString *destinationEntityName;
@property (nonatomic, copy) NSString *inverseRelationshipName;
@property (nonatomic) int type;
@property (nonatomic) int deleteRule;
@end
