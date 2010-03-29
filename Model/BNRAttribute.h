//
//  BNRAttribute.h
//  TCSpeedTest
//
//  Created by Aaron Hillegass on 2/4/10.
//  Copyright 2010 Big Nerd Ranch. All rights reserved.
//

#import <Foundation/Foundation.h>

enum {
    BNRUndefinedAttributeType = 0,
    BNRInteger8AttributeType = 100,
    BNRUInteger8AttributeType = 150,
    BNRInteger32AttributeType = 200,
    BNRUInteger32AttributeType = 250,
    BNRInteger64AttributeType = 300,
    BNRUInteger64AttributeType = 350,
    BNRDecimalAttributeType = 400,
    BNRDouble64AttributeType = 500,
    BNRFloat32AttributeType = 600,
    BNRStringAttributeType = 700,
    BNRBooleanAttributeType = 800,
    BNRDateAttributeType = 900,
    BNRBinaryDataAttributeType = 1000,
    BNRArchivableObjectAttributeType = 2000    
};

typedef NSUInteger BNRAttributeType;


@interface BNRAttribute : NSObject {
    BNRAttributeType type;
    BOOL indexed;
    BOOL textIndexed;
}
@property (nonatomic) BNRAttributeType type;
@property (nonatomic) BOOL indexed;
@property (nonatomic) BOOL textIndexed;

@end
