#import <Foundation/Foundation.h>
#import <LocalAuthentication/LocalAuthentication.h>
#include "touch_id.h"
#include <iostream>

bool macos_touch_id_authenticate(const char* prompt_reason) {
    __block bool auth_success = false;
    
    @autoreleasepool {
        LAContext *context = [[LAContext alloc] init];
        NSError *error = nil;
        
        NSString *reason = prompt_reason ? [NSString stringWithUTF8String:prompt_reason] : @"Authenticate gate scan";
        
        if ([context canEvaluatePolicy:LAPolicyDeviceOwnerAuthenticationWithBiometrics error:&error]) {
            dispatch_semaphore_t sema = dispatch_semaphore_create(0);
            
            [context evaluatePolicy:LAPolicyDeviceOwnerAuthenticationWithBiometrics
                    localizedReason:reason
                              reply:^(BOOL success, NSError * _Nullable err) {
                auth_success = success;
                if (!success && err) {
                    std::cerr << "[TouchID] Authentication failed: " 
                              << [err.localizedDescription UTF8String] << std::endl;
                }
                dispatch_semaphore_signal(sema);
            }];
            
            dispatch_semaphore_wait(sema, DISPATCH_TIME_FOREVER);
        } else {
            std::cerr << "[TouchID] Biometrics (Touch ID) are not available or not configured on this machine." << std::endl;
            if (error) {
                std::cerr << "[TouchID] Error Details: " << [error.localizedDescription UTF8String] << std::endl;
            }
        }
    }
    
    return auth_success;
}
