//
//  PhoneSpeedTestAppDelegate.h
//  PhoneSpeedTest
//
//  Created by Aaron Hillegass on 1/26/10.
//  Copyright Big Nerd Ranch 2010. All rights reserved.
//

#import <UIKit/UIKit.h>

@interface PhoneSpeedTestAppDelegate : NSObject <UIApplicationDelegate> {
    UIWindow *window;
    IBOutlet UILabel *label;
}
- (IBAction)simpleInsertTC:(id)sender;
- (IBAction)simpleInsertCD:(id)sender;
- (IBAction)complexInsertTC:(id)sender;
- (IBAction)complexInsertCD:(id)sender;
- (IBAction)simpleFetchTC:(id)sender;
- (IBAction)simpleFetchCD:(id)sender;
- (IBAction)complexFetchTC:(id)sender;
- (IBAction)complexFetchCD:(id)sender;
- (IBAction)clearFiles:(id)sender;

@property (nonatomic, retain) IBOutlet UIWindow *window;

@end

