#import "GitHubSyncManager.h"

static NSString *const kClientID = @"Ov23li5Q9HZMR3fdyzsp";

@interface GitHubSyncManager () {
    NSURLSession *_session;
    NSUserDefaults *_defaults;
    dispatch_source_t _pollTimer;
    NSString *_token;
    NSString *_username;
    NSString *_deviceCode;
    int _pollInterval;
    BOOL _repoReady;
    BOOL _syncPending;
}
@end

@implementation GitHubSyncManager

- (instancetype)init {
    self = [super init];
    if (self) {
        _session = [NSURLSession sharedSession];
        _defaults = [NSUserDefaults standardUserDefaults];
        _token = [_defaults stringForKey:@"github/token"] ?: @"";
        _username = [_defaults stringForKey:@"github/username"] ?: @"";
        _pollInterval = 5;
        _repoReady = NO;
        _syncPending = NO;
        _status = @"disconnected";

        if (_token.length > 0 && _username.length > 0) {
            _status = @"connected";
        }
    }
    return self;
}

- (BOOL)authenticated {
    return _token.length > 0;
}

- (void)setStatus:(NSString *)s {
    if ([_status isEqualToString:s]) return;
    _status = [s copy];
    if (self.onStatusChanged) self.onStatusChanged(_status);
}

- (void)setError:(NSString *)msg {
    _errorMessage = [msg copy];
    [self setStatus:@"error"];
}

// ─── Auth ───────────────────────────────────────────────────────

- (void)startAuth {
    if (_token.length > 0) return;
    [self setStatus:@"authenticating"];

    NSURL *url = [NSURL URLWithString:@"https://github.com/login/device/code"];
    NSMutableURLRequest *req = [NSMutableURLRequest requestWithURL:url];
    req.HTTPMethod = @"POST";
    [req setValue:@"application/x-www-form-urlencoded" forHTTPHeaderField:@"Content-Type"];
    [req setValue:@"application/json" forHTTPHeaderField:@"Accept"];

    NSString *body = [NSString stringWithFormat:@"client_id=%@&scope=repo", kClientID];
    req.HTTPBody = [body dataUsingEncoding:NSUTF8StringEncoding];

    __weak GitHubSyncManager *weakSelf = self;
    [[_session dataTaskWithRequest:req completionHandler:^(NSData *data, NSURLResponse *response, NSError *error) {
        dispatch_async(dispatch_get_main_queue(), ^{
            GitHubSyncManager *s = weakSelf;
            if (!s) return;

            if (error) {
                [s setError:[NSString stringWithFormat:@"Failed to start auth: %@", error.localizedDescription]];
                return;
            }

            NSDictionary *json = [NSJSONSerialization JSONObjectWithData:data options:0 error:nil];
            s->_deviceCode = json[@"device_code"];
            s->_userCode = json[@"user_code"];
            s->_verificationUrl = json[@"verification_uri"];
            s->_pollInterval = [json[@"interval"] intValue] ?: 5;

            if (s.onUserCodeChanged) s.onUserCodeChanged(s->_userCode);

            // Open verification URL in browser
            [[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:s->_verificationUrl]];

            // Start polling
            [s startPolling];
        });
    }] resume];
}

- (void)startPolling {
    if (_pollTimer) {
        dispatch_source_cancel(_pollTimer);
        _pollTimer = nil;
    }

    __weak GitHubSyncManager *weakSelf = self;
    _pollTimer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, dispatch_get_main_queue());
    dispatch_source_set_timer(_pollTimer,
                              dispatch_time(DISPATCH_TIME_NOW, _pollInterval * NSEC_PER_SEC),
                              _pollInterval * NSEC_PER_SEC, NSEC_PER_SEC);
    dispatch_source_set_event_handler(_pollTimer, ^{
        GitHubSyncManager *s = weakSelf;
        if (s) [s pollForToken];
    });
    dispatch_resume(_pollTimer);
}

- (void)cancelAuth {
    if (_pollTimer) {
        dispatch_source_cancel(_pollTimer);
        _pollTimer = nil;
    }
    _deviceCode = nil;
    _userCode = nil;
    _verificationUrl = nil;
    if (self.onUserCodeChanged) self.onUserCodeChanged(nil);
    [self setStatus:@"disconnected"];
}

- (void)logout {
    if (_pollTimer) {
        dispatch_source_cancel(_pollTimer);
        _pollTimer = nil;
    }
    _token = @"";
    _username = @"";
    _repoReady = NO;
    [_defaults removeObjectForKey:@"github/token"];
    [_defaults removeObjectForKey:@"github/username"];
    if (self.onAuthenticatedChanged) self.onAuthenticatedChanged(NO);
    [self setStatus:@"disconnected"];
}

- (void)pollForToken {
    NSURL *url = [NSURL URLWithString:@"https://github.com/login/oauth/access_token"];
    NSMutableURLRequest *req = [NSMutableURLRequest requestWithURL:url];
    req.HTTPMethod = @"POST";
    [req setValue:@"application/x-www-form-urlencoded" forHTTPHeaderField:@"Content-Type"];
    [req setValue:@"application/json" forHTTPHeaderField:@"Accept"];

    NSString *body = [NSString stringWithFormat:@"client_id=%@&device_code=%@&grant_type=urn:ietf:params:oauth:grant-type:device_code",
                      kClientID, _deviceCode];
    req.HTTPBody = [body dataUsingEncoding:NSUTF8StringEncoding];

    __weak GitHubSyncManager *weakSelf = self;
    [[_session dataTaskWithRequest:req completionHandler:^(NSData *data, NSURLResponse *response, NSError *error) {
        dispatch_async(dispatch_get_main_queue(), ^{
            GitHubSyncManager *s = weakSelf;
            if (!s) return;
            if (error) return;

            NSDictionary *json = [NSJSONSerialization JSONObjectWithData:data options:0 error:nil];

            if (json[@"access_token"]) {
                if (s->_pollTimer) {
                    dispatch_source_cancel(s->_pollTimer);
                    s->_pollTimer = nil;
                }
                [s onTokenReceived:json[@"access_token"]];
            } else {
                NSString *err = json[@"error"];
                if ([err isEqualToString:@"slow_down"]) {
                    s->_pollInterval += 5;
                    [s startPolling];
                } else if (![err isEqualToString:@"authorization_pending"]) {
                    if (s->_pollTimer) {
                        dispatch_source_cancel(s->_pollTimer);
                        s->_pollTimer = nil;
                    }
                    [s setError:[NSString stringWithFormat:@"Auth failed: %@", err]];
                }
            }
        });
    }] resume];
}

- (void)onTokenReceived:(NSString *)token {
    _token = [token copy];
    [_defaults setObject:_token forKey:@"github/token"];
    _userCode = nil;
    _verificationUrl = nil;
    if (self.onUserCodeChanged) self.onUserCodeChanged(nil);
    if (self.onAuthenticatedChanged) self.onAuthenticatedChanged(YES);
    [self fetchUsername];
}

- (void)fetchUsername {
    NSURL *url = [NSURL URLWithString:@"https://api.github.com/user"];
    NSMutableURLRequest *req = [NSMutableURLRequest requestWithURL:url];
    [req setValue:[NSString stringWithFormat:@"Bearer %@", _token] forHTTPHeaderField:@"Authorization"];
    [req setValue:@"application/json" forHTTPHeaderField:@"Accept"];

    __weak GitHubSyncManager *weakSelf = self;
    [[_session dataTaskWithRequest:req completionHandler:^(NSData *data, NSURLResponse *response, NSError *error) {
        dispatch_async(dispatch_get_main_queue(), ^{
            GitHubSyncManager *s = weakSelf;
            if (!s) return;
            if (error) {
                [s setError:@"Failed to get user info"];
                return;
            }
            NSDictionary *json = [NSJSONSerialization JSONObjectWithData:data options:0 error:nil];
            s->_username = json[@"login"];
            [s->_defaults setObject:s->_username forKey:@"github/username"];
            [s setStatus:@"connected"];
        });
    }] resume];
}

// ─── Repo ───────────────────────────────────────────────────────

- (void)ensureRepo {
    if (_repoReady) {
        [self doSync];
        return;
    }

    NSString *urlStr = [NSString stringWithFormat:@"https://api.github.com/repos/%@/flick-notes", _username];
    NSMutableURLRequest *req = [NSMutableURLRequest requestWithURL:[NSURL URLWithString:urlStr]];
    [req setValue:[NSString stringWithFormat:@"Bearer %@", _token] forHTTPHeaderField:@"Authorization"];
    [req setValue:@"application/json" forHTTPHeaderField:@"Accept"];

    __weak GitHubSyncManager *weakSelf = self;
    [[_session dataTaskWithRequest:req completionHandler:^(NSData *data, NSURLResponse *response, NSError *error) {
        dispatch_async(dispatch_get_main_queue(), ^{
            GitHubSyncManager *s = weakSelf;
            if (!s) return;

            NSHTTPURLResponse *httpResp = (NSHTTPURLResponse *)response;
            if (httpResp.statusCode == 200) {
                [s cloneOrPull];
            } else if (httpResp.statusCode == 404) {
                [s createRepo];
            } else {
                [s setError:@"Failed to check repo"];
            }
        });
    }] resume];
}

- (void)createRepo {
    NSURL *url = [NSURL URLWithString:@"https://api.github.com/user/repos"];
    NSMutableURLRequest *req = [NSMutableURLRequest requestWithURL:url];
    req.HTTPMethod = @"POST";
    [req setValue:[NSString stringWithFormat:@"Bearer %@", _token] forHTTPHeaderField:@"Authorization"];
    [req setValue:@"application/json" forHTTPHeaderField:@"Accept"];
    [req setValue:@"application/json" forHTTPHeaderField:@"Content-Type"];

    NSDictionary *body = @{
        @"name": @"flick-notes",
        @"private": @YES,
        @"description": @"Flick notes backup",
        @"auto_init": @YES
    };
    req.HTTPBody = [NSJSONSerialization dataWithJSONObject:body options:0 error:nil];

    __weak GitHubSyncManager *weakSelf = self;
    [[_session dataTaskWithRequest:req completionHandler:^(NSData *data, NSURLResponse *response, NSError *error) {
        dispatch_async(dispatch_get_main_queue(), ^{
            GitHubSyncManager *s = weakSelf;
            if (!s) return;
            NSHTTPURLResponse *httpResp = (NSHTTPURLResponse *)response;
            if (httpResp.statusCode == 201) {
                [s cloneOrPull];
            } else {
                [s setError:@"Failed to create repo"];
            }
        });
    }] resume];
}

- (void)cloneOrPull {
    NSString *repoPath = [self repoPath];
    NSFileManager *fm = [NSFileManager defaultManager];
    NSString *gitDir = [repoPath stringByAppendingPathComponent:@".git"];

    if ([fm fileExistsAtPath:gitDir]) {
        // Set remote URL then pull
        [self runGit:@[@"remote", @"set-url", @"origin", [self remoteUrl]] inDir:repoPath completion:^(int code, NSString *output) {
            [self runGit:@[@"pull", @"--rebase", @"--autostash"] inDir:repoPath completion:^(int code, NSString *output) {
                self->_repoReady = YES;
                [self doSync];
            }];
        }];
    } else {
        [fm createDirectoryAtPath:repoPath withIntermediateDirectories:YES attributes:nil error:nil];
        NSString *tempDir = NSTemporaryDirectory();
        [self runGit:@[@"clone", [self remoteUrl], repoPath] inDir:tempDir completion:^(int code, NSString *output) {
            if (code != 0) {
                [self setError:@"Git clone failed"];
                return;
            }
            self->_repoReady = YES;
            [self restoreFromRepo];
            [self doSync];
        }];
    }
}

// ─── Sync ───────────────────────────────────────────────────────

- (void)sync {
    if (_token.length == 0 || _username.length == 0) return;
    if ([_status isEqualToString:@"syncing"] || [_status isEqualToString:@"authenticating"]) {
        _syncPending = YES;
        return;
    }
    [self setStatus:@"syncing"];
    [self ensureRepo];
}

- (void)restoreFromRepo {
    NSFileManager *fm = [NSFileManager defaultManager];
    NSString *repoPath = [self repoPath];
    NSString *notesPath = [self notesPath];

    [self restoreTrashFromRepo];

    NSArray *repoFiles = [self txtFilesIn:repoPath];
    if (repoFiles.count == 0) return;

    NSArray *localFiles = [self txtFilesIn:notesPath];
    BOOL localIsEmpty = (localFiles.count == 0);
    if (!localIsEmpty && localFiles.count == 1) {
        NSString *content = [NSString stringWithContentsOfFile:[notesPath stringByAppendingPathComponent:localFiles[0]]
                                                      encoding:NSUTF8StringEncoding error:nil];
        localIsEmpty = (content.length == 0 || [[content stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]] length] == 0);
    }
    if (!localIsEmpty) return;

    for (NSString *f in localFiles)
        [fm removeItemAtPath:[notesPath stringByAppendingPathComponent:f] error:nil];
    for (NSString *f in repoFiles)
        [fm copyItemAtPath:[repoPath stringByAppendingPathComponent:f]
                    toPath:[notesPath stringByAppendingPathComponent:f] error:nil];

    if (self.onNotesRestored) self.onNotesRestored();
}

- (void)restoreTrashFromRepo {
    NSFileManager *fm = [NSFileManager defaultManager];
    NSString *repoTrash = [[self repoPath] stringByAppendingPathComponent:@"trash"];
    if (![fm fileExistsAtPath:repoTrash]) return;

    NSString *localTrash = [[self notesPath] stringByAppendingPathComponent:@"trash"];
    [fm createDirectoryAtPath:localTrash withIntermediateDirectories:YES attributes:nil error:nil];

    for (NSString *f in [self txtFilesIn:repoTrash]) {
        NSString *dest = [localTrash stringByAppendingPathComponent:f];
        if (![fm fileExistsAtPath:dest]) {
            [fm copyItemAtPath:[repoTrash stringByAppendingPathComponent:f]
                        toPath:dest error:nil];
        }
    }
}

- (void)doSync {
    NSFileManager *fm = [NSFileManager defaultManager];
    NSString *repoPath = [self repoPath];
    NSString *notesPath = [self notesPath];

    for (NSString *f in [self txtFilesIn:repoPath])
        [fm removeItemAtPath:[repoPath stringByAppendingPathComponent:f] error:nil];
    for (NSString *f in [self txtFilesIn:notesPath])
        [fm copyItemAtPath:[notesPath stringByAppendingPathComponent:f]
                    toPath:[repoPath stringByAppendingPathComponent:f] error:nil];
    [self copyTrashToRepo];

    __weak GitHubSyncManager *weakSelf = self;
    [self runGit:@[@"add", @"-A"] inDir:repoPath completion:^(int code, NSString *output) {
        GitHubSyncManager *s = weakSelf;
        if (!s) return;
        [s runGit:@[@"commit", @"-m", @"sync"] inDir:repoPath completion:^(int code, NSString *output) {
            if (code != 0) {
                [s setStatus:@"connected"];
                if (s->_syncPending) { s->_syncPending = NO; [s sync]; }
                return;
            }
            [s runGit:@[@"push"] inDir:repoPath completion:^(int code, NSString *output) {
                if (code != 0) {
                    [s setError:@"Push failed"];
                    return;
                }
                [s setStatus:@"connected"];
                if (s->_syncPending) { s->_syncPending = NO; [s sync]; }
            }];
        }];
    }];
}

- (void)syncOnQuit {
    if (_token.length == 0 || _username.length == 0 || !_repoReady) return;

    NSFileManager *fm = [NSFileManager defaultManager];
    NSString *repoPath = [self repoPath];
    NSString *notesPath = [self notesPath];

    for (NSString *f in [self txtFilesIn:repoPath])
        [fm removeItemAtPath:[repoPath stringByAppendingPathComponent:f] error:nil];
    for (NSString *f in [self txtFilesIn:notesPath])
        [fm copyItemAtPath:[notesPath stringByAppendingPathComponent:f]
                    toPath:[repoPath stringByAppendingPathComponent:f] error:nil];
    [self copyTrashToRepo];

    // Synchronous git operations
    [self runGitSync:@[@"add", @"-A"] inDir:repoPath];
    int code = [self runGitSync:@[@"commit", @"-m", @"sync"] inDir:repoPath];
    if (code != 0) return;
    [self runGitSync:@[@"push"] inDir:repoPath];
}

- (void)copyTrashToRepo {
    NSFileManager *fm = [NSFileManager defaultManager];
    NSString *localTrash = [[self notesPath] stringByAppendingPathComponent:@"trash"];
    NSString *repoTrash = [[self repoPath] stringByAppendingPathComponent:@"trash"];
    [fm createDirectoryAtPath:repoTrash withIntermediateDirectories:YES attributes:nil error:nil];

    for (NSString *f in [self txtFilesIn:repoTrash])
        [fm removeItemAtPath:[repoTrash stringByAppendingPathComponent:f] error:nil];

    if (![fm fileExistsAtPath:localTrash]) return;

    for (NSString *f in [self txtFilesIn:localTrash])
        [fm copyItemAtPath:[localTrash stringByAppendingPathComponent:f]
                    toPath:[repoTrash stringByAppendingPathComponent:f] error:nil];
}

// ─── Helpers ────────────────────────────────────────────────────

- (void)runGit:(NSArray<NSString *> *)args inDir:(NSString *)dir completion:(void (^)(int, NSString *))callback {
    NSTask *task = [[NSTask alloc] init];
    task.executableURL = [NSURL fileURLWithPath:@"/usr/bin/git"];
    task.arguments = args;
    task.currentDirectoryURL = [NSURL fileURLWithPath:dir];
    NSPipe *pipe = [NSPipe pipe];
    task.standardOutput = pipe;
    task.standardError = pipe;

    __weak GitHubSyncManager *weakSelf = self;
    task.terminationHandler = ^(NSTask *t) {
        NSData *data = [pipe.fileHandleForReading readDataToEndOfFile];
        NSString *output = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
        dispatch_async(dispatch_get_main_queue(), ^{
            if (callback) callback(t.terminationStatus, output);
        });
    };

    NSError *error = nil;
    [task launchAndReturnError:&error];
    if (error && callback) {
        callback(-1, error.localizedDescription);
    }
}

- (int)runGitSync:(NSArray<NSString *> *)args inDir:(NSString *)dir {
    NSTask *task = [[NSTask alloc] init];
    task.executableURL = [NSURL fileURLWithPath:@"/usr/bin/git"];
    task.arguments = args;
    task.currentDirectoryURL = [NSURL fileURLWithPath:dir];
    NSError *error = nil;
    [task launchAndReturnError:&error];
    if (error) return -1;
    [task waitUntilExit];
    return task.terminationStatus;
}

- (NSString *)repoPath {
    NSString *appSupport = [NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, YES) firstObject];
    return [appSupport stringByAppendingPathComponent:@"flick/repo"];
}

- (NSString *)notesPath {
    NSString *appSupport = [NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, YES) firstObject];
    return [appSupport stringByAppendingPathComponent:@"flick"];
}

- (NSString *)remoteUrl {
    return [NSString stringWithFormat:@"https://x-access-token:%@@github.com/%@/flick-notes.git", _token, _username];
}

- (NSArray<NSString *> *)txtFilesIn:(NSString *)dir {
    NSFileManager *fm = [NSFileManager defaultManager];
    NSArray *all = [fm contentsOfDirectoryAtPath:dir error:nil] ?: @[];
    NSMutableArray *result = [NSMutableArray array];
    for (NSString *f in all) {
        if ([f.pathExtension isEqualToString:@"txt"])
            [result addObject:f];
    }
    return [result sortedArrayUsingSelector:@selector(compare:)];
}

@end
