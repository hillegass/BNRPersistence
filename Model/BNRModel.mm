//
//  BNRModel.m
//  TCSpeedTest
//
//  Created by Aaron Hillegass on 2/4/10.
//  Copyright 2010 Big Nerd Ranch. All rights reserved.
//

#import "BNRModel.h"

@interface BNRModelParser : NSObject {
    BNRModel *currentModel;
    BNREntity *currentEntity;
}
- (BOOL)fillModel:(BNRModel *)model
   fromDataInPath:(NSString *)path
            error:(NSError **)err;

@end

@implementation BNRModelParser

- (void)parser:(NSXMLParser *)parser 
didStartElement:(NSString *)elementName 
  namespaceURI:(NSString *)namespaceURI 
 qualifiedName:(NSString *)qName 
    attributes:(NSDictionary *)attributeDict
{
    NSLog(@"started Element: %@", elementName);
}

- (void)parser:(NSXMLParser *)parser 
 didEndElement:(NSString *)elementName 
  namespaceURI:(NSString *)namespaceURI 
 qualifiedName:(NSString *)qName
{
    NSLog(@"ended Element: %@", elementName);

}

- (BOOL)fillModel:(BNRModel *)model
           fromDataInPath:(NSString *)path
                         error:(NSError **)err
{
    NSURL *url = [[NSURL alloc] initFileURLWithPath:path];
    currentModel = model;
    currentEntity = nil;
    NSXMLParser *parser = [[NSXMLParser alloc] initWithContentsOfURL:url];
    [url release];
    
    [parser setDelegate:self];
    BOOL success = [parser parse];
    if (!success && err) {
        NSError *error = [parser parserError];
        *err = error;
    } 
    [parser release];
    currentModel = nil;
    currentEntity = nil;
    return success;
}


@end


@implementation BNRModel

- (id)initWithXML:(NSString *)path error:(NSError **)err
{
    [super init];
    
    BNRModelParser *parser = [[BNRModelParser alloc] init];
    BOOL success  =  [parser fillModel:self
                           fromDataInPath:path
                                error:err];
    [parser release];
    
    if (!success) {
        [self dealloc];
        return nil;
    }
    
    return self;
}
- (void)dealloc
{
    hash_map<Class, BNREntity *, hash<Class>, equal_to<Class> >::iterator iter = entityTable->begin();
    while (iter != entityTable->end()) {
        EntityHashedPair currentPair = *iter;
        BNREntity *e = currentPair.second;
        [e release];
    }    
    delete entityTable;
    [super dealloc];
}
- (BNREntity *)entityForClass:(Class)c
{
    return (*entityTable)[c];
}

- (void)setEntity:(BNREntity *)e forClass:(Class)c
{
    (*entityTable)[c] = e;
    [e retain];
}

@end
