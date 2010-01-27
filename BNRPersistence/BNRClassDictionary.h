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

#import <Cocoa/Cocoa.h>

/*!
 @class BNRClassDictionary
 @abstract a collection that holds key-value pairs where the key is a Class and
 the values are objects. 
 In BNRPersistence, the object is a BNRIntDictionary of rowID->storedObject
 @discussion Objects are retained.
*/

@interface BNRClassDictionary : NSObject {
    NSMapTable *mapTable;
}
/*!
 @method init
 @abstract The designated initializer for this class
*/
- (id)init;

/*!
 @method setObject:forClass
 @abstract Puts the key-value pair into the dictionary.  If the Class is already 
 in the dictionary,  its object is replaced with 'obj'
 @param obj An object
 @param c A Class
*/
- (void)setObject:(id)obj forClass:(Class)c;

/*!
 @method objectForClass:
 @abstract returns the value for the key 'c'
*/
- (id)objectForClass:(Class)c;

/*!
 @method objectEnumerator
 @abstract Returns a BNRMapTableEnumerator which can step through all the values 
 (not the the keys) in the dictionary
*/
- (NSEnumerator *)objectEnumerator;

@end
