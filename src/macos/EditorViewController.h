#pragma once
#import <Cocoa/Cocoa.h>
#include "../core/core_notestore.h"
#include "../core/core_mathengine.h"
#include "../core/core_listengine.h"

@class GitHubSyncManager;

@interface EditorViewController : NSViewController <NSTextViewDelegate, NSMenuDelegate>

@property (nonatomic, readonly) NSTextView *textView;
@property (nonatomic, strong) GitHubSyncManager *syncManager;

- (instancetype)initWithNoteStore:(flick::NoteStore *)noteStore
                       mathEngine:(flick::MathEngine *)mathEngine
                       listEngine:(flick::ListEngine *)listEngine
                         darkMode:(BOOL)darkMode;

- (void)updateWindowBackground:(NSWindow *)window;
- (void)refreshIndicators;
- (void)showAuthOverlay:(NSString *)code;
- (void)hideAuthOverlay;

@end
