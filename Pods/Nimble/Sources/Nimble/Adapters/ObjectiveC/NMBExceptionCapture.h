#import <Foundation/Foundation.h>
#import <dispatch/dispatch.h>

@interface NMBExceptionCapture : NSObject

- (id)initWithHandler:(void(^)(NSException *))handler finally:(void(^)())finally;
- (void)tryBlock:(void(^)())unsafeBlock;

@end

typedef void(^NMBSourceCallbackBlock)(BOOL successful);
