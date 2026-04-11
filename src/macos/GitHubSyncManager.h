#pragma once
#import <Cocoa/Cocoa.h>

@interface GitHubSyncManager : NSObject

@property (nonatomic, readonly) NSString *status;
@property (nonatomic, readonly) BOOL authenticated;
@property (nonatomic, readonly) NSString *userCode;
@property (nonatomic, readonly) NSString *verificationUrl;
@property (nonatomic, readonly) NSString *errorMessage;

// Callbacks
@property (nonatomic, copy) void (^onStatusChanged)(NSString *status);
@property (nonatomic, copy) void (^onAuthenticatedChanged)(BOOL authenticated);
@property (nonatomic, copy) void (^onUserCodeChanged)(NSString *code);
@property (nonatomic, copy) void (^onNotesRestored)(void);

- (void)startAuth;
- (void)cancelAuth;
- (void)logout;
- (void)sync;
- (void)syncOnQuit;

@end
