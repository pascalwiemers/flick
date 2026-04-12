#import "AppDelegate.h"
#import "EditorViewController.h"
#import "GitHubSyncManager.h"

@implementation AppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)notification {
    self.noteStore = new flick::NoteStore();
    self.mathEngine = new flick::MathEngine();
    self.listEngine = new flick::ListEngine();
    self.statsEngine = new flick::StatsEngine();

    // GitHub sync manager
    self.syncManager = [[GitHubSyncManager alloc] init];

    // Restore window frame or use default
    NSRect frame = NSMakeRect(200, 200, 520, 400);
    NSArray *savedFrame = [[NSUserDefaults standardUserDefaults] arrayForKey:@"windowFrame"];
    if (savedFrame.count == 4) {
        frame = NSMakeRect(
            [savedFrame[0] doubleValue], [savedFrame[1] doubleValue],
            [savedFrame[2] doubleValue], [savedFrame[3] doubleValue]
        );
    }

    NSWindowStyleMask style = NSWindowStyleMaskTitled | NSWindowStyleMaskResizable |
                              NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable |
                              NSWindowStyleMaskFullSizeContentView;
    self.window = [[NSWindow alloc] initWithContentRect:frame
                                              styleMask:style
                                                backing:NSBackingStoreBuffered
                                                  defer:NO];
    self.window.titlebarAppearsTransparent = YES;
    self.window.titleVisibility = NSWindowTitleHidden;
    self.window.movableByWindowBackground = YES;
    self.window.title = @"Flick";
    self.window.minSize = NSMakeSize(300, 200);

    // Hide traffic light buttons
    [self.window standardWindowButton:NSWindowCloseButton].hidden = YES;
    [self.window standardWindowButton:NSWindowMiniaturizeButton].hidden = YES;
    [self.window standardWindowButton:NSWindowZoomButton].hidden = YES;

    // Restore dark mode preference
    BOOL darkMode = [[NSUserDefaults standardUserDefaults] boolForKey:@"darkMode"];
    // Default to dark if no preference saved
    if (![[NSUserDefaults standardUserDefaults] objectForKey:@"darkMode"])
        darkMode = YES;

    self.editorVC = [[EditorViewController alloc] initWithNoteStore:self.noteStore
                                                         mathEngine:self.mathEngine
                                                         listEngine:self.listEngine
                                                        statsEngine:self.statsEngine
                                                           darkMode:darkMode];
    self.editorVC.syncManager = self.syncManager;

    // Wire up sync manager callbacks
    __weak AppDelegate *weakSelf = self;
    self.syncManager.onStatusChanged = ^(NSString *status) {
        AppDelegate *s = weakSelf;
        if (!s) return;
        [s.editorVC refreshIndicators];
    };
    self.syncManager.onAuthenticatedChanged = ^(BOOL authenticated) {
        AppDelegate *s = weakSelf;
        if (!s) return;
        [s.editorVC refreshIndicators];
    };
    self.syncManager.onUserCodeChanged = ^(NSString *code) {
        AppDelegate *s = weakSelf;
        if (!s) return;
        if (code && code.length > 0) {
            [s.editorVC showAuthOverlay:code];
        } else {
            [s.editorVC hideAuthOverlay];
        }
    };
    self.syncManager.onNotesRestored = ^{
        AppDelegate *s = weakSelf;
        if (!s) return;
        // Reload notes from disk
        s.noteStore->reload();
    };

    self.window.contentViewController = self.editorVC;

    // Set window background to match theme
    [self.editorVC updateWindowBackground:self.window];

    [self.window makeKeyAndOrderFront:nil];
    [self.window makeFirstResponder:self.editorVC.textView];
}

- (void)applicationWillTerminate:(NSNotification *)notification {
    // Save window frame
    NSRect frame = self.window.frame;
    NSArray *frameArray = @[@(frame.origin.x), @(frame.origin.y),
                            @(frame.size.width), @(frame.size.height)];
    [[NSUserDefaults standardUserDefaults] setObject:frameArray forKey:@"windowFrame"];

    // Sync notes to GitHub on quit
    [self.syncManager syncOnQuit];

    delete self.noteStore;
    delete self.mathEngine;
    delete self.listEngine;
    delete self.statsEngine;
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender {
    return YES;
}

@end
