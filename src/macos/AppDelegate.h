#pragma once
#import <Cocoa/Cocoa.h>
#include "../core/core_notestore.h"
#include "../core/core_mathengine.h"
#include "../core/core_listengine.h"

@class EditorViewController;
@class GitHubSyncManager;

@interface AppDelegate : NSObject <NSApplicationDelegate>
@property (nonatomic) flick::NoteStore *noteStore;
@property (nonatomic) flick::MathEngine *mathEngine;
@property (nonatomic) flick::ListEngine *listEngine;
@property (nonatomic, strong) NSWindow *window;
@property (nonatomic, strong) EditorViewController *editorVC;
@property (nonatomic, strong) GitHubSyncManager *syncManager;
@end
