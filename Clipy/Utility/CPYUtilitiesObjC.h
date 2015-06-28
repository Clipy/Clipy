//
//  CPYUtilitiesObjC.h
//  Clipy
//
//  Created by 古林俊佑 on 2015/06/21.
//  Copyright (c) 2015年 Shunsuke Furubayashi. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <Cocoa/Cocoa.h>

@interface CPYUtilitiesObjC : NSObject

+ (BOOL)postCommandV;
+ (NSString *)transformKeyCode:(NSInteger)keyCode;

@end
