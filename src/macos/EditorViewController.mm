#import "EditorViewController.h"
#import "GitHubSyncManager.h"
#import <QuartzCore/QuartzCore.h>
#include <string>
#include <dispatch/dispatch.h>

// ─── Theme colors ───────────────────────────────────────────────
static NSColor *bgColor(BOOL dark) {
    return dark ? [NSColor colorWithRed:0.1 green:0.1 blue:0.1 alpha:1]
                : [NSColor colorWithRed:0.96 green:0.96 blue:0.96 alpha:1];
}
static NSColor *textColor(BOOL dark) {
    return dark ? [NSColor colorWithRed:0.88 green:0.88 blue:0.88 alpha:1]
                : [NSColor colorWithRed:0.1 green:0.1 blue:0.1 alpha:1];
}
static NSColor *dimColor(BOOL dark) {
    return dark ? [NSColor colorWithRed:0.33 green:0.33 blue:0.33 alpha:1]
                : [NSColor colorWithRed:0.67 green:0.67 blue:0.67 alpha:1];
}
static NSColor *accentColor() {
    return [NSColor colorWithRed:0.29 green:0.62 blue:1.0 alpha:1];
}
static NSColor *selectionColor(BOOL dark) {
    return dark ? [NSColor colorWithRed:0.25 green:0.25 blue:0.25 alpha:1]
                : [NSColor colorWithRed:0.69 green:0.82 blue:1.0 alpha:1];
}
static NSColor *surfaceColor(BOOL dark) {
    return dark ? [NSColor colorWithRed:0.133 green:0.133 blue:0.133 alpha:1]
                : [NSColor colorWithRed:0.91 green:0.91 blue:0.91 alpha:1];
}
static NSColor *borderColor(BOOL dark) {
    return dark ? [NSColor colorWithRed:0.2 green:0.2 blue:0.2 alpha:1]
                : [NSColor colorWithRed:0.816 green:0.816 blue:0.816 alpha:1];
}
static NSColor *hoverColor(BOOL dark) {
    return dark ? [NSColor colorWithRed:0.2 green:0.2 blue:0.2 alpha:1]
                : [NSColor colorWithRed:0.847 green:0.847 blue:0.847 alpha:1];
}
static NSColor *activeSurface(BOOL dark) {
    return dark ? [NSColor colorWithRed:0.165 green:0.165 blue:0.165 alpha:1]
                : [NSColor colorWithRed:0.878 green:0.878 blue:0.878 alpha:1];
}
static NSColor *activeBorder(BOOL dark) {
    return dark ? [NSColor colorWithRed:0.267 green:0.267 blue:0.267 alpha:1]
                : [NSColor colorWithRed:0.733 green:0.733 blue:0.733 alpha:1];
}
static NSColor *inactiveBorder(BOOL dark) {
    return dark ? [NSColor colorWithRed:0.184 green:0.184 blue:0.184 alpha:1]
                : [NSColor colorWithRed:0.835 green:0.835 blue:0.835 alpha:1];
}

static NSFont *monoFont(CGFloat size) {
    NSFont *f = [NSFont fontWithName:@"JetBrains Mono" size:size];
    if (!f) f = [NSFont fontWithName:@"Fira Code" size:size];
    if (!f) f = [NSFont fontWithName:@"Cascadia Code" size:size];
    if (!f) f = [NSFont monospacedSystemFontOfSize:size weight:NSFontWeightRegular];
    return f;
}

// ─── Indicator dots view (fixed position, top-right) ────────────
@interface IndicatorView : NSView
@property (nonatomic, weak) EditorViewController *controller;
@property (nonatomic) BOOL darkMode;
@property (nonatomic) BOOL autoPasteActive;
@property (nonatomic) BOOL listActive;
@property (nonatomic) BOOL markdownActive;
@property (nonatomic) BOOL githubAuthenticated;
@property (nonatomic) BOOL githubSyncing;
@end

@implementation IndicatorView

- (BOOL)isFlipped { return YES; }

- (void)drawRect:(NSRect)dirtyRect {
    [super drawRect:dirtyRect];
    CGFloat dotSize = 6;
    CGFloat spacing = 12;
    CGFloat x = self.bounds.size.width - 8 - dotSize;
    CGFloat y = 8;

    if (self.autoPasteActive) {
        NSRect r = NSMakeRect(x, y, dotSize, dotSize);
        NSBezierPath *dot = [NSBezierPath bezierPathWithOvalInRect:r];
        [accentColor() setFill];
        [dot fill];
        x -= spacing;
    }
    if (self.listActive) {
        NSRect r = NSMakeRect(x, y, dotSize, dotSize);
        NSBezierPath *dot = [NSBezierPath bezierPathWithOvalInRect:r];
        [[NSColor colorWithRed:1.0 green:0.624 blue:0.29 alpha:1] setFill]; // #ff9f4a
        [dot fill];
        x -= spacing;
    }
    if (self.markdownActive) {
        NSRect r = NSMakeRect(x, y, dotSize, dotSize);
        NSBezierPath *dot = [NSBezierPath bezierPathWithOvalInRect:r];
        [[NSColor colorWithRed:0.29 green:1.0 blue:0.498 alpha:1] setFill]; // #4aff7f
        [dot fill];
        x -= spacing;
    }
    if (self.githubAuthenticated) {
        NSRect r = NSMakeRect(x, y, dotSize, dotSize);
        NSBezierPath *dot = [NSBezierPath bezierPathWithOvalInRect:r];
        NSColor *c = self.githubSyncing
            ? [NSColor colorWithRed:1.0 green:0.667 blue:0.29 alpha:1]  // #ffaa4a
            : [NSColor colorWithRed:0.29 green:1.0 blue:0.498 alpha:1]; // #4aff7f
        [c setFill];
        [dot fill];
    }
}

@end

// ─── Overlay view for math results and list checkboxes ──────────
@interface OverlayView : NSView
@property (nonatomic, weak) NSTextView *textView;
@property (nonatomic) flick::MathEngine *mathEngine;
@property (nonatomic) flick::ListEngine *listEngine;
@property (nonatomic) flick::NoteStore *noteStore;
@property (nonatomic) BOOL darkMode;
@property (nonatomic) CGFloat fontSize;
@end

@implementation OverlayView

- (BOOL)isFlipped { return YES; }

- (NSRect)rectForLine:(int)lineIndex {
    NSString *text = self.textView.string;
    NSArray<NSString *> *lines = [text componentsSeparatedByString:@"\n"];
    if (lineIndex < 0 || lineIndex >= (int)lines.count)
        return NSZeroRect;

    NSUInteger charIndex = 0;
    for (int i = 0; i < lineIndex; i++) {
        charIndex += lines[i].length + 1;
    }
    NSUInteger lineLen = lines[lineIndex].length;
    if (lineLen == 0) lineLen = 1;

    NSRange glyphRange = [self.textView.layoutManager glyphRangeForCharacterRange:NSMakeRange(charIndex, lineLen)
                                                             actualCharacterRange:nil];
    NSRect rect = [self.textView.layoutManager boundingRectForGlyphRange:glyphRange
                                                         inTextContainer:self.textView.textContainer];
    NSSize inset = self.textView.textContainerInset;
    rect.origin.x += inset.width;
    rect.origin.y += inset.height;
    return rect;
}

- (NSRect)rectForLineEnd:(int)lineIndex {
    NSString *text = self.textView.string;
    NSArray<NSString *> *lines = [text componentsSeparatedByString:@"\n"];
    if (lineIndex < 0 || lineIndex >= (int)lines.count)
        return NSZeroRect;

    NSUInteger charIndex = 0;
    for (int i = 0; i < lineIndex; i++)
        charIndex += lines[i].length + 1;

    NSUInteger endChar = charIndex + lines[lineIndex].length;
    if (endChar > text.length) endChar = text.length;

    NSRange glyphRange = [self.textView.layoutManager glyphRangeForCharacterRange:NSMakeRange(endChar, 0)
                                                             actualCharacterRange:nil];
    NSRect rect = [self.textView.layoutManager boundingRectForGlyphRange:glyphRange
                                                         inTextContainer:self.textView.textContainer];
    NSSize inset = self.textView.textContainerInset;
    rect.origin.x += inset.width;
    rect.origin.y += inset.height;
    return rect;
}

- (void)drawRect:(NSRect)dirtyRect {
    [super drawRect:dirtyRect];

    NSFont *font = monoFont(self.fontSize);
    NSDictionary *attrs = @{
        NSFontAttributeName: font,
        NSForegroundColorAttributeName: textColor(self.darkMode)
    };

    // ── Math results ──
    for (auto &r : self.mathEngine->results()) {
        NSRect lineRect = [self rectForLine:r.line];
        if (NSIsEmptyRect(lineRect)) continue;

        if (r.isComment) {
            NSColor *bg = bgColor(self.darkMode);
            [[bg colorWithAlphaComponent:0.6] setFill];
            NSRectFillUsingOperation(NSMakeRect(0, lineRect.origin.y, self.bounds.size.width, lineRect.size.height),
                                     NSCompositingOperationSourceOver);
            continue;
        }

        if (r.isSeparator) {
            NSRect endRect = [self rectForLineEnd:r.line];
            CGFloat x = self.bounds.size.width - 32 - 80;
            CGFloat y = endRect.origin.y + lineRect.size.height + 2;
            NSBezierPath *line = [NSBezierPath bezierPath];
            [line moveToPoint:NSMakePoint(x, y)];
            [line lineToPoint:NSMakePoint(x + 80, y)];
            [borderColor(self.darkMode) setStroke];
            [line stroke];
            continue;
        }

        if (r.text.empty()) continue;

        NSString *text = [NSString stringWithUTF8String:r.text.c_str()];
        NSColor *color = [self colorFromHex:r.color];

        NSMutableDictionary *resultAttrs = [attrs mutableCopy];
        resultAttrs[NSForegroundColorAttributeName] = color;
        if (r.isTotal) {
            resultAttrs[NSFontAttributeName] = [[NSFontManager sharedFontManager] convertFont:font toHaveTrait:NSItalicFontMask];
        }

        if (r.isTotal) {
            NSSize textSize = [text sizeWithAttributes:resultAttrs];
            CGFloat x = self.bounds.size.width - 32 - textSize.width;
            CGFloat y = lineRect.origin.y + lineRect.size.height + 6;
            [text drawAtPoint:NSMakePoint(x, y) withAttributes:resultAttrs];
        } else {
            NSRect endRect = [self rectForLineEnd:r.line];
            [text drawAtPoint:NSMakePoint(endRect.origin.x, lineRect.origin.y) withAttributes:resultAttrs];
        }
    }

    // ── List checkboxes ──
    if (!self.listEngine->active()) return;

    for (auto &item : self.listEngine->items()) {
        NSRect lineRect = [self rectForLine:item.line];
        if (NSIsEmptyRect(lineRect)) continue;

        if (item.type == "comment") {
            [[bgColor(self.darkMode) colorWithAlphaComponent:0.6] setFill];
            NSRectFillUsingOperation(NSMakeRect(0, lineRect.origin.y, self.bounds.size.width, lineRect.size.height),
                                     NSCompositingOperationSourceOver);
            continue;
        }

        if (item.type != "item") continue;

        if (item.checked) {
            [[bgColor(self.darkMode) colorWithAlphaComponent:0.4] setFill];
            NSRectFillUsingOperation(NSMakeRect(0, lineRect.origin.y, self.bounds.size.width, lineRect.size.height),
                                     NSCompositingOperationSourceOver);
        }

        CGFloat cbSize = 14;
        CGFloat cbX = 10;
        CGFloat cbY = lineRect.origin.y + (lineRect.size.height - cbSize) / 2;
        NSRect cbRect = NSMakeRect(cbX, cbY, cbSize, cbSize);

        if (item.checked) {
            NSBezierPath *path = [NSBezierPath bezierPathWithRoundedRect:cbRect xRadius:3 yRadius:3];
            [accentColor() setFill];
            [path fill];
            NSDictionary *checkAttrs = @{
                NSFontAttributeName: [NSFont boldSystemFontOfSize:10],
                NSForegroundColorAttributeName: [NSColor whiteColor]
            };
            NSString *check = @"\u2713";
            NSSize checkSize = [check sizeWithAttributes:checkAttrs];
            [check drawAtPoint:NSMakePoint(cbX + (cbSize - checkSize.width) / 2,
                                           cbY + (cbSize - checkSize.height) / 2)
                withAttributes:checkAttrs];

            NSRect endRect = [self rectForLineEnd:item.line];
            CGFloat strikeY = lineRect.origin.y + lineRect.size.height / 2;
            CGFloat strikeEnd = endRect.origin.x;
            if (strikeEnd > 32) {
                NSBezierPath *strike = [NSBezierPath bezierPath];
                [strike moveToPoint:NSMakePoint(32, strikeY)];
                [strike lineToPoint:NSMakePoint(strikeEnd, strikeY)];
                [[dimColor(self.darkMode) colorWithAlphaComponent:0.6] setStroke];
                [strike stroke];
            }
        } else {
            NSBezierPath *path = [NSBezierPath bezierPathWithRoundedRect:NSInsetRect(cbRect, 0.75, 0.75) xRadius:3 yRadius:3];
            [dimColor(self.darkMode) setStroke];
            path.lineWidth = 1.5;
            [path stroke];
        }
    }
}

- (NSColor *)colorFromHex:(const std::string &)hex {
    if (hex.size() < 7) return textColor(self.darkMode);
    unsigned int r, g, b;
    sscanf(hex.c_str() + 1, "%02x%02x%02x", &r, &g, &b);
    return [NSColor colorWithRed:r/255.0 green:g/255.0 blue:b/255.0 alpha:1.0];
}

- (void)mouseDown:(NSEvent *)event {
    if (!self.listEngine->active()) return;

    NSPoint point = [self convertPoint:event.locationInWindow fromView:nil];

    for (auto &item : self.listEngine->items()) {
        if (item.type != "item") continue;
        NSRect lineRect = [self rectForLine:item.line];
        CGFloat cbSize = 14;
        NSRect cbRect = NSMakeRect(6, lineRect.origin.y + (lineRect.size.height - cbSize) / 2 - 4,
                                   cbSize + 8, cbSize + 8);
        if (NSPointInRect(point, cbRect)) {
            std::string newText = self.listEngine->toggleCheck(
                self.textView.string.UTF8String, item.line);
            NSString *ns = [NSString stringWithUTF8String:newText.c_str()];
            NSRange selectedRange = self.textView.selectedRange;
            [self.textView setString:ns];
            if (selectedRange.location <= ns.length)
                [self.textView setSelectedRange:selectedRange];
            return;
        }
    }
}

@end

// ─── Grid overview view ─────────────────────────────────────────
@interface GridOverlayView : NSView
@property (nonatomic) flick::NoteStore *noteStore;
@property (nonatomic) BOOL darkMode;
@property (nonatomic) CGFloat fontSize;
@property (nonatomic, copy) void (^onSelectNote)(int index);
@end

@implementation GridOverlayView

- (BOOL)isFlipped { return YES; }

- (void)drawRect:(NSRect)dirtyRect {
    [super drawRect:dirtyRect];

    [bgColor(self.darkMode) setFill];
    NSRectFill(self.bounds);

    int count = self.noteStore->noteCount();
    if (count == 0) return;

    CGFloat margin = 16;
    CGFloat gap = 12;
    CGFloat availableWidth = self.bounds.size.width - margin * 2;
    int cols = MIN(3, count);
    CGFloat cellWidth = MAX(160, (availableWidth - gap * (cols - 1)) / cols);
    CGFloat cellHeight = cellWidth * 0.75;
    int currentIdx = self.noteStore->currentIndex();

    for (int i = 0; i < count; i++) {
        int col = i % cols;
        int row = i / cols;
        CGFloat x = margin + col * (cellWidth + gap);
        CGFloat y = margin + row * (cellHeight + gap);
        NSRect cardRect = NSMakeRect(x, y, cellWidth, cellHeight);

        // Card background
        BOOL isCurrent = (i == currentIdx);
        NSColor *cardBg = isCurrent ? activeSurface(self.darkMode) : surfaceColor(self.darkMode);
        NSColor *cardBorder = isCurrent ? activeBorder(self.darkMode) : inactiveBorder(self.darkMode);

        NSBezierPath *card = [NSBezierPath bezierPathWithRoundedRect:cardRect xRadius:4 yRadius:4];
        [cardBg setFill];
        [card fill];
        [cardBorder setStroke];
        card.lineWidth = 1;
        [card stroke];

        // Note text preview
        std::string noteText = self.noteStore->getText(i);
        NSString *preview = [NSString stringWithUTF8String:noteText.c_str()];
        if (preview.length == 0) preview = @"(empty)";

        NSColor *tColor = (noteText.empty()) ? dimColor(self.darkMode) : textColor(self.darkMode);
        NSMutableParagraphStyle *para = [[NSMutableParagraphStyle alloc] init];
        para.lineBreakMode = NSLineBreakByTruncatingTail;
        NSDictionary *textAttrs = @{
            NSFontAttributeName: monoFont(11),
            NSForegroundColorAttributeName: tColor,
            NSParagraphStyleAttributeName: para
        };
        NSRect textRect = NSInsetRect(cardRect, 12, 12);
        [preview drawInRect:textRect withAttributes:textAttrs];
    }
}

- (void)mouseDown:(NSEvent *)event {
    NSPoint point = [self convertPoint:event.locationInWindow fromView:nil];

    int count = self.noteStore->noteCount();
    CGFloat margin = 16;
    CGFloat gap = 12;
    CGFloat availableWidth = self.bounds.size.width - margin * 2;
    int cols = MIN(3, count);
    CGFloat cellWidth = MAX(160, (availableWidth - gap * (cols - 1)) / cols);
    CGFloat cellHeight = cellWidth * 0.75;

    for (int i = 0; i < count; i++) {
        int col = i % cols;
        int row = i / cols;
        CGFloat x = margin + col * (cellWidth + gap);
        CGFloat y = margin + row * (cellHeight + gap);
        NSRect cardRect = NSMakeRect(x, y, cellWidth, cellHeight);
        if (NSPointInRect(point, cardRect)) {
            if (self.onSelectNote) self.onSelectNote(i);
            return;
        }
    }
}

@end

// ─── Markdown preview view ──────────────────────────────────────
@interface MarkdownPreviewView : NSView
@property (nonatomic, strong) NSScrollView *scrollView;
@property (nonatomic, strong) NSTextView *previewTextView;
@property (nonatomic) BOOL darkMode;
@property (nonatomic) CGFloat fontSize;
@end

@implementation MarkdownPreviewView

- (instancetype)initWithFrame:(NSRect)frame darkMode:(BOOL)dark fontSize:(CGFloat)fontSize {
    self = [super initWithFrame:frame];
    if (self) {
        _darkMode = dark;
        _fontSize = fontSize;

        _scrollView = [[NSScrollView alloc] initWithFrame:self.bounds];
        _scrollView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        _scrollView.hasVerticalScroller = YES;
        _scrollView.scrollerStyle = NSScrollerStyleOverlay;
        _scrollView.drawsBackground = NO;
        _scrollView.borderType = NSNoBorder;

        _previewTextView = [[NSTextView alloc] initWithFrame:_scrollView.bounds];
        _previewTextView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        _previewTextView.editable = NO;
        _previewTextView.selectable = YES;
        _previewTextView.textContainerInset = NSMakeSize(24, 32);
        _previewTextView.drawsBackground = YES;
        _previewTextView.backgroundColor = bgColor(dark);

        _scrollView.documentView = _previewTextView;
        [self addSubview:_scrollView];
    }
    return self;
}

- (void)setMarkdownText:(NSString *)text {
    // Use NSAttributedString markdown init (macOS 12+)
    NSError *error = nil;
    NSAttributedString *md = nil;

    if (@available(macOS 12.0, *)) {
        NSAttributedStringMarkdownParsingOptions *opts = [[NSAttributedStringMarkdownParsingOptions alloc] init];
        opts.interpretedSyntax = NSAttributedStringMarkdownInterpretedSyntaxInlineOnlyPreservingWhitespace;
        md = [[NSAttributedString alloc] initWithMarkdownString:text options:opts baseURL:nil error:&error];
    }

    if (!md) {
        // Fallback: just display as plain text
        md = [[NSAttributedString alloc] initWithString:text attributes:@{
            NSFontAttributeName: monoFont(self.fontSize),
            NSForegroundColorAttributeName: textColor(self.darkMode)
        }];
    }

    // Apply our font and colors on top
    NSMutableAttributedString *styled = [[NSMutableAttributedString alloc] initWithAttributedString:md];
    [styled addAttribute:NSFontAttributeName value:monoFont(self.fontSize) range:NSMakeRange(0, styled.length)];
    [styled addAttribute:NSForegroundColorAttributeName value:textColor(self.darkMode) range:NSMakeRange(0, styled.length)];

    [self.previewTextView.textStorage setAttributedString:styled];
    self.previewTextView.backgroundColor = bgColor(self.darkMode);
}

- (void)updateTheme {
    self.previewTextView.backgroundColor = bgColor(self.darkMode);
}

@end

// ─── GitHub auth overlay view ───────────────────────────────────
@interface GitHubAuthOverlayView : NSView
@property (nonatomic, copy) NSString *userCode;
@property (nonatomic) BOOL darkMode;
@property (nonatomic) BOOL copied;
@property (nonatomic, copy) void (^onCancel)(void);
@property (nonatomic, copy) void (^onCopyCode)(void);
@end

@implementation GitHubAuthOverlayView

- (BOOL)isFlipped { return YES; }

- (void)drawRect:(NSRect)dirtyRect {
    [super drawRect:dirtyRect];

    // Dark overlay background
    [[NSColor colorWithRed:0 green:0 blue:0 alpha:0.8] setFill];
    NSRectFill(self.bounds);

    if (!self.userCode || self.userCode.length == 0) return;

    // Center card
    CGFloat cardW = 320, cardH = 200;
    CGFloat cardX = (self.bounds.size.width - cardW) / 2;
    CGFloat cardY = (self.bounds.size.height - cardH) / 2;
    NSRect cardRect = NSMakeRect(cardX, cardY, cardW, cardH);

    NSBezierPath *card = [NSBezierPath bezierPathWithRoundedRect:cardRect xRadius:12 yRadius:12];
    [surfaceColor(self.darkMode) setFill];
    [card fill];
    [borderColor(self.darkMode) setStroke];
    card.lineWidth = 1;
    [card stroke];

    // "Enter this code on GitHub:"
    NSFont *smallFont = monoFont(13);
    NSDictionary *labelAttrs = @{
        NSFontAttributeName: smallFont,
        NSForegroundColorAttributeName: dimColor(self.darkMode)
    };
    NSString *label = @"Enter this code on GitHub:";
    NSSize labelSize = [label sizeWithAttributes:labelAttrs];
    [label drawAtPoint:NSMakePoint(cardX + (cardW - labelSize.width) / 2, cardY + 20)
        withAttributes:labelAttrs];

    // Code box
    NSFont *codeFont = monoFont(32);
    NSDictionary *codeAttrs = @{
        NSFontAttributeName: [NSFont boldSystemFontOfSize:32],
        NSForegroundColorAttributeName: accentColor()
    };
    NSSize codeSize = [self.userCode sizeWithAttributes:codeAttrs];
    CGFloat codeBoxW = codeSize.width + 24;
    CGFloat codeBoxH = codeSize.height + 12;
    CGFloat codeBoxX = cardX + (cardW - codeBoxW) / 2;
    CGFloat codeBoxY = cardY + 50;
    NSRect codeBoxRect = NSMakeRect(codeBoxX, codeBoxY, codeBoxW, codeBoxH);

    NSBezierPath *codeBox = [NSBezierPath bezierPathWithRoundedRect:codeBoxRect xRadius:6 yRadius:6];
    [hoverColor(self.darkMode) setFill];
    [codeBox fill];
    [borderColor(self.darkMode) setStroke];
    [codeBox stroke];

    [self.userCode drawAtPoint:NSMakePoint(codeBoxX + 12, codeBoxY + 6) withAttributes:codeAttrs];

    // "Click to copy" / "Copied!"
    NSString *copyLabel = self.copied ? @"Copied!" : @"Click to copy";
    NSColor *copyColor = self.copied ? accentColor() : dimColor(self.darkMode);
    NSDictionary *copyAttrs = @{
        NSFontAttributeName: monoFont(11),
        NSForegroundColorAttributeName: copyColor
    };
    NSSize copySize = [copyLabel sizeWithAttributes:copyAttrs];
    [copyLabel drawAtPoint:NSMakePoint(cardX + (cardW - copySize.width) / 2, codeBoxY + codeBoxH + 8)
        withAttributes:copyAttrs];

    // "Waiting for authorization..."
    NSString *waitLabel = @"Waiting for authorization...";
    NSDictionary *waitAttrs = @{
        NSFontAttributeName: monoFont(12),
        NSForegroundColorAttributeName: dimColor(self.darkMode)
    };
    NSSize waitSize = [waitLabel sizeWithAttributes:waitAttrs];
    [waitLabel drawAtPoint:NSMakePoint(cardX + (cardW - waitSize.width) / 2, codeBoxY + codeBoxH + 30)
        withAttributes:waitAttrs];

    // Cancel button
    CGFloat btnW = 80, btnH = 28;
    CGFloat btnX = cardX + (cardW - btnW) / 2;
    CGFloat btnY = cardY + cardH - btnH - 20;
    NSRect btnRect = NSMakeRect(btnX, btnY, btnW, btnH);

    NSBezierPath *btn = [NSBezierPath bezierPathWithRoundedRect:btnRect xRadius:4 yRadius:4];
    [hoverColor(self.darkMode) setFill];
    [btn fill];
    [borderColor(self.darkMode) setStroke];
    [btn stroke];

    NSString *cancelLabel = @"Cancel";
    NSDictionary *cancelAttrs = @{
        NSFontAttributeName: monoFont(12),
        NSForegroundColorAttributeName: textColor(self.darkMode)
    };
    NSSize cancelSize = [cancelLabel sizeWithAttributes:cancelAttrs];
    [cancelLabel drawAtPoint:NSMakePoint(btnX + (btnW - cancelSize.width) / 2, btnY + (btnH - cancelSize.height) / 2)
        withAttributes:cancelAttrs];
}

- (void)mouseDown:(NSEvent *)event {
    NSPoint point = [self convertPoint:event.locationInWindow fromView:nil];

    CGFloat cardW = 320, cardH = 200;
    CGFloat cardX = (self.bounds.size.width - cardW) / 2;
    CGFloat cardY = (self.bounds.size.height - cardH) / 2;

    // Cancel button hit test
    CGFloat btnW = 80, btnH = 28;
    CGFloat btnX = cardX + (cardW - btnW) / 2;
    CGFloat btnY = cardY + cardH - btnH - 20;
    NSRect btnRect = NSMakeRect(btnX, btnY, btnW, btnH);
    if (NSPointInRect(point, btnRect)) {
        if (self.onCancel) self.onCancel();
        return;
    }

    // Code box hit test (copy)
    NSFont *codeFont = [NSFont boldSystemFontOfSize:32];
    NSDictionary *codeAttrs = @{NSFontAttributeName: codeFont};
    NSSize codeSize = [self.userCode sizeWithAttributes:codeAttrs];
    CGFloat codeBoxW = codeSize.width + 24;
    CGFloat codeBoxH = codeSize.height + 12;
    CGFloat codeBoxX = cardX + (cardW - codeBoxW) / 2;
    CGFloat codeBoxY = cardY + 50;
    NSRect codeBoxRect = NSMakeRect(codeBoxX, codeBoxY, codeBoxW, codeBoxH);
    if (NSPointInRect(point, codeBoxRect)) {
        if (self.onCopyCode) self.onCopyCode();
        return;
    }
}

@end

// ─── Main editor view controller ────────────────────────────────
@interface EditorViewController () {
    flick::NoteStore *_noteStore;
    flick::MathEngine *_mathEngine;
    flick::ListEngine *_listEngine;
    BOOL _darkMode;
    CGFloat _fontSize;
    BOOL _syncing;
    BOOL _animating;
    BOOL _autoPasteActive;
    BOOL _markdownPreview;
    BOOL _gridVisible;
    NSScrollView *_scrollView;
    OverlayView *_overlayView;
    IndicatorView *_indicatorView;
    GridOverlayView *_gridOverlayView;
    MarkdownPreviewView *_markdownPreviewView;
    GitHubAuthOverlayView *_authOverlayView;
    dispatch_source_t _evalTimer;
    dispatch_source_t _pasteboardTimer;
    NSInteger _lastPasteboardCount;
    NSString *_lastCapturedText;
    CGFloat _swipeAccumulator;
    dispatch_source_t _swipeCooldownTimer;
    BOOL _swipeCoolingDown;
    id _keyMonitor;
    id _scrollMonitor;
}
@end

@implementation EditorViewController

- (instancetype)initWithNoteStore:(flick::NoteStore *)noteStore
                       mathEngine:(flick::MathEngine *)mathEngine
                       listEngine:(flick::ListEngine *)listEngine
                         darkMode:(BOOL)darkMode {
    self = [super init];
    if (self) {
        _noteStore = noteStore;
        _mathEngine = mathEngine;
        _listEngine = listEngine;
        _darkMode = darkMode;
        _fontSize = [[NSUserDefaults standardUserDefaults] doubleForKey:@"fontSize"];
        if (_fontSize < 8) _fontSize = 14;
        _syncing = NO;
        _animating = NO;
        _autoPasteActive = NO;
        _markdownPreview = NO;
        _gridVisible = NO;
        _swipeAccumulator = 0;
        _swipeCoolingDown = NO;
        _lastPasteboardCount = [NSPasteboard generalPasteboard].changeCount;
        _lastCapturedText = [[NSPasteboard generalPasteboard] stringForType:NSPasteboardTypeString] ?: @"";
    }
    return self;
}

- (void)loadView {
    NSView *container = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 520, 400)];
    container.wantsLayer = YES;

    // Scroll view + text view
    _scrollView = [[NSScrollView alloc] initWithFrame:container.bounds];
    _scrollView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    _scrollView.hasVerticalScroller = YES;
    _scrollView.scrollerStyle = NSScrollerStyleOverlay;
    _scrollView.drawsBackground = NO;
    _scrollView.borderType = NSNoBorder;

    _textView = [[NSTextView alloc] initWithFrame:_scrollView.bounds];
    _textView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    _textView.delegate = self;
    _textView.richText = NO;
    _textView.allowsUndo = YES;
    _textView.textContainerInset = NSMakeSize(24, 32);
    _textView.drawsBackground = YES;
    _textView.continuousSpellCheckingEnabled = NO;
    _textView.automaticQuoteSubstitutionEnabled = NO;
    _textView.automaticDashSubstitutionEnabled = NO;

    _scrollView.documentView = _textView;
    [container addSubview:_scrollView];

    // Overlay view for math results and checkboxes
    _overlayView = [[OverlayView alloc] initWithFrame:_textView.bounds];
    _overlayView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    _overlayView.textView = _textView;
    _overlayView.mathEngine = _mathEngine;
    _overlayView.listEngine = _listEngine;
    _overlayView.noteStore = _noteStore;
    _overlayView.darkMode = _darkMode;
    _overlayView.fontSize = _fontSize;
    [_textView addSubview:_overlayView];

    // Grid overlay (hidden initially)
    _gridOverlayView = [[GridOverlayView alloc] initWithFrame:container.bounds];
    _gridOverlayView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    _gridOverlayView.noteStore = _noteStore;
    _gridOverlayView.darkMode = _darkMode;
    _gridOverlayView.fontSize = _fontSize;
    _gridOverlayView.hidden = YES;
    __weak EditorViewController *weakSelf = self;
    _gridOverlayView.onSelectNote = ^(int index) {
        EditorViewController *s = weakSelf;
        if (!s) return;
        s->_syncing = YES;
        s->_noteStore->setCurrentIndex(index);
        [s loadCurrentNote];
        s->_syncing = NO;
        s->_gridVisible = NO;
        s->_gridOverlayView.hidden = YES;
        [s->_textView.window makeFirstResponder:s->_textView];
    };
    [container addSubview:_gridOverlayView];

    // Markdown preview (hidden initially)
    _markdownPreviewView = [[MarkdownPreviewView alloc] initWithFrame:container.bounds
                                                             darkMode:_darkMode
                                                             fontSize:_fontSize];
    _markdownPreviewView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    _markdownPreviewView.hidden = YES;
    [container addSubview:_markdownPreviewView];

    // GitHub auth overlay (hidden initially)
    _authOverlayView = [[GitHubAuthOverlayView alloc] initWithFrame:container.bounds];
    _authOverlayView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    _authOverlayView.darkMode = _darkMode;
    _authOverlayView.hidden = YES;
    _authOverlayView.onCancel = ^{
        EditorViewController *s = weakSelf;
        if (!s) return;
        [s->_syncManager cancelAuth];
        s->_authOverlayView.hidden = YES;
    };
    _authOverlayView.onCopyCode = ^{
        EditorViewController *s = weakSelf;
        if (!s) return;
        [[NSPasteboard generalPasteboard] clearContents];
        [[NSPasteboard generalPasteboard] setString:s->_authOverlayView.userCode forType:NSPasteboardTypeString];
        s->_authOverlayView.copied = YES;
        [s->_authOverlayView setNeedsDisplay:YES];
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 2 * NSEC_PER_SEC), dispatch_get_main_queue(), ^{
            EditorViewController *s2 = weakSelf;
            if (!s2) return;
            s2->_authOverlayView.copied = NO;
            [s2->_authOverlayView setNeedsDisplay:YES];
        });
    };
    [container addSubview:_authOverlayView];

    // Indicator dots (topmost, fixed position)
    _indicatorView = [[IndicatorView alloc] initWithFrame:container.bounds];
    _indicatorView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    _indicatorView.controller = self;
    _indicatorView.darkMode = _darkMode;
    [container addSubview:_indicatorView];

    // Track scroll and resize to update overlay
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(textViewFrameChanged:)
                                                 name:NSViewFrameDidChangeNotification
                                               object:_textView];
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(scrollViewChanged:)
                                                 name:NSViewBoundsDidChangeNotification
                                               object:_scrollView.contentView];

    [self applyTheme];
    [self loadCurrentNote];

    self.view = container;
}

- (void)viewDidAppear {
    [super viewDidAppear];
    [self.view.window makeFirstResponder:_textView];
    [self setupKeyboardShortcuts];
    [self setupScrollMonitor];
    [self setupPasteboardMonitor];

    // If GitHub sync is connected, do initial sync
    if (_syncManager && _syncManager.authenticated) {
        [_syncManager sync];
    }
}

- (void)setupKeyboardShortcuts {
    __weak EditorViewController *weakSelf = self;
    _keyMonitor = [NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskKeyDown handler:^NSEvent *(NSEvent *event) {
        EditorViewController *s = weakSelf;
        if (!s) return event;

        BOOL cmd = (event.modifierFlags & NSEventModifierFlagCommand) != 0;
        BOOL shift = (event.modifierFlags & NSEventModifierFlagShift) != 0;

        if (cmd && shift) {
            NSString *chars = event.charactersIgnoringModifiers;
            if ([chars isEqualToString:@"v"]) { [s toggleAutoPaste]; return nil; }
        }

        if (cmd && !shift) {
            NSString *chars = event.charactersIgnoringModifiers;
            if ([chars isEqualToString:@"n"]) { [s createNewNote]; return nil; }
            if ([chars isEqualToString:@"w"]) { [s deleteCurrentNote]; return nil; }
            if ([chars isEqualToString:@"d"]) { [s toggleDarkMode]; return nil; }
            if ([chars isEqualToString:@"q"]) { [NSApp terminate:nil]; return nil; }
            if ([chars isEqualToString:@"="]) { [s changeFontSize:2]; return nil; }
            if ([chars isEqualToString:@"-"]) { [s changeFontSize:-2]; return nil; }
            if ([chars isEqualToString:@"m"]) { [s toggleMarkdownPreview]; return nil; }
            if ([chars isEqualToString:@"g"]) { [s syncOrAuth]; return nil; }

            // Cmd+Left/Right for note navigation
            if (event.keyCode == 123) { [s navigateTo:s->_noteStore->currentIndex() - 1]; return nil; }
            if (event.keyCode == 124) { [s navigateTo:s->_noteStore->currentIndex() + 1]; return nil; }
        }

        // Shift+Tab for grid overview
        if (shift && event.keyCode == 48) { [s toggleGridOverview]; return nil; }

        return event;
    }];
}

- (void)setupScrollMonitor {
    __weak EditorViewController *weakSelf = self;
    _scrollMonitor = [NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskScrollWheel handler:^NSEvent *(NSEvent *event) {
        EditorViewController *s = weakSelf;
        if (!s) return event;

        // Ctrl+vertical scroll
        if (event.modifierFlags & NSEventModifierFlagControl) {
            if (event.scrollingDeltaY > 0)
                [s navigateTo:s->_noteStore->currentIndex() - 1];
            else if (event.scrollingDeltaY < 0)
                [s navigateTo:s->_noteStore->currentIndex() + 1];
            return nil;
        }

        // Two-finger horizontal swipe (trackpad)
        if (event.hasPreciseScrollingDeltas) {
            BOOL dominated = fabs(event.scrollingDeltaX) > fabs(event.scrollingDeltaY);
            if (dominated) {
                if (s->_animating || s->_swipeCoolingDown)
                    return nil;

                s->_swipeAccumulator += event.scrollingDeltaX;
                CGFloat threshold = 200;
                if (s->_swipeAccumulator > threshold) {
                    s->_swipeAccumulator = 0;
                    [s startSwipeCooldown];
                    [s navigateTo:s->_noteStore->currentIndex() - 1];
                } else if (s->_swipeAccumulator < -threshold) {
                    s->_swipeAccumulator = 0;
                    [s startSwipeCooldown];
                    [s navigateTo:s->_noteStore->currentIndex() + 1];
                }
                return nil;
            }
        }

        return event;
    }];
}

- (void)startSwipeCooldown {
    _swipeCoolingDown = YES;
    __weak EditorViewController *weakSelf = self;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 800 * NSEC_PER_MSEC), dispatch_get_main_queue(), ^{
        EditorViewController *s = weakSelf;
        if (!s) return;
        s->_swipeCoolingDown = NO;
        s->_swipeAccumulator = 0;
    });
}

- (void)setupPasteboardMonitor {
    __weak EditorViewController *weakSelf = self;
    _pasteboardTimer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, dispatch_get_main_queue());
    dispatch_source_set_timer(_pasteboardTimer,
                              dispatch_time(DISPATCH_TIME_NOW, 500 * NSEC_PER_MSEC),
                              500 * NSEC_PER_MSEC, 100 * NSEC_PER_MSEC);
    dispatch_source_set_event_handler(_pasteboardTimer, ^{
        EditorViewController *s = weakSelf;
        if (!s || !s->_autoPasteActive) return;

        NSPasteboard *pb = [NSPasteboard generalPasteboard];
        NSInteger currentCount = pb.changeCount;
        if (currentCount == s->_lastPasteboardCount) return;
        s->_lastPasteboardCount = currentCount;

        NSString *text = [pb stringForType:NSPasteboardTypeString];
        if (!text || text.length == 0 || text.length > 10000) return;
        if ([text isEqualToString:s->_lastCapturedText]) return;

        s->_lastCapturedText = [text copy];
        s->_noteStore->appendText(text.UTF8String);

        // Reload the text view to show appended text
        s->_syncing = YES;
        NSString *newText = [NSString stringWithUTF8String:s->_noteStore->currentText().c_str()];
        [s->_textView setString:newText ?: @""];
        s->_syncing = NO;
        [s scheduleEvaluation];
    });
    dispatch_resume(_pasteboardTimer);
}

// ─── Theme ──────────────────────────────────────────────────────
- (void)applyTheme {
    _textView.backgroundColor = bgColor(_darkMode);
    _textView.insertionPointColor = textColor(_darkMode);
    _textView.selectedTextAttributes = @{
        NSBackgroundColorAttributeName: selectionColor(_darkMode),
        NSForegroundColorAttributeName: textColor(_darkMode)
    };

    NSFont *font = monoFont(_fontSize);
    _textView.font = font;
    _textView.textColor = textColor(_darkMode);

    if (_textView.string.length > 0) {
        NSDictionary *attrs = @{
            NSFontAttributeName: font,
            NSForegroundColorAttributeName: textColor(_darkMode)
        };
        [_textView.textStorage setAttributes:attrs range:NSMakeRange(0, _textView.string.length)];
    }

    _overlayView.darkMode = _darkMode;
    _overlayView.fontSize = _fontSize;
    [_overlayView setNeedsDisplay:YES];

    _indicatorView.darkMode = _darkMode;
    [_indicatorView setNeedsDisplay:YES];

    _gridOverlayView.darkMode = _darkMode;
    [_gridOverlayView setNeedsDisplay:YES];

    _markdownPreviewView.darkMode = _darkMode;
    _markdownPreviewView.fontSize = _fontSize;
    [_markdownPreviewView updateTheme];

    _authOverlayView.darkMode = _darkMode;
    [_authOverlayView setNeedsDisplay:YES];

    // Use the window from the scroll view (not self.view) to avoid
    // triggering loadView recursion
    NSWindow *win = _scrollView.window;
    if (win)
        [self updateWindowBackground:win];
}

- (void)updateWindowBackground:(NSWindow *)window {
    window.backgroundColor = bgColor(_darkMode);
}

- (void)toggleDarkMode {
    _darkMode = !_darkMode;
    [[NSUserDefaults standardUserDefaults] setBool:_darkMode forKey:@"darkMode"];
    [self applyTheme];
}

- (void)changeFontSize:(CGFloat)delta {
    _fontSize = fmax(8, fmin(48, _fontSize + delta));
    [[NSUserDefaults standardUserDefaults] setDouble:_fontSize forKey:@"fontSize"];
    [self applyTheme];
}

// ─── Indicators ─────────────────────────────────────────────────
- (void)refreshIndicators {
    _indicatorView.autoPasteActive = _autoPasteActive;
    _indicatorView.listActive = _listEngine->active();
    _indicatorView.markdownActive = _markdownPreview;
    _indicatorView.githubAuthenticated = _syncManager ? _syncManager.authenticated : NO;
    _indicatorView.githubSyncing = _syncManager ? [_syncManager.status isEqualToString:@"syncing"] : NO;
    [_indicatorView setNeedsDisplay:YES];
}

// ─── AutoPaste ──────────────────────────────────────────────────
- (void)toggleAutoPaste {
    _autoPasteActive = !_autoPasteActive;
    if (_autoPasteActive) {
        _lastPasteboardCount = [NSPasteboard generalPasteboard].changeCount;
        _lastCapturedText = [[NSPasteboard generalPasteboard] stringForType:NSPasteboardTypeString] ?: @"";
    }
    [self refreshIndicators];
}

// ─── Markdown Preview ───────────────────────────────────────────
- (void)toggleMarkdownPreview {
    if (_gridVisible) return;
    _markdownPreview = !_markdownPreview;
    _markdownPreviewView.hidden = !_markdownPreview;

    if (_markdownPreview) {
        [_markdownPreviewView setMarkdownText:_textView.string];
    } else {
        [_textView.window makeFirstResponder:_textView];
    }
    [self refreshIndicators];
}

// ─── Grid Overview ──────────────────────────────────────────────
- (void)toggleGridOverview {
    if (_markdownPreview) {
        _markdownPreview = NO;
        _markdownPreviewView.hidden = YES;
    }
    _gridVisible = !_gridVisible;
    _gridOverlayView.hidden = !_gridVisible;
    if (_gridVisible) {
        [_gridOverlayView setNeedsDisplay:YES];
    } else {
        [_textView.window makeFirstResponder:_textView];
    }
    [self refreshIndicators];
}

// ─── GitHub Sync ────────────────────────────────────────────────
- (void)syncOrAuth {
    if (!_syncManager) return;
    if (_syncManager.authenticated) {
        [_syncManager sync];
    } else {
        [_syncManager startAuth];
    }
}

- (void)showAuthOverlay:(NSString *)code {
    _authOverlayView.userCode = code;
    _authOverlayView.copied = NO;
    _authOverlayView.hidden = NO;
    [_authOverlayView setNeedsDisplay:YES];
}

- (void)hideAuthOverlay {
    _authOverlayView.hidden = YES;
}

- (void)disconnectGitHub {
    if (!_syncManager) return;
    [_syncManager logout];
    [self refreshIndicators];
}

// ─── Note management ────────────────────────────────────────────
- (void)loadCurrentNote {
    _syncing = YES;
    NSString *text = [NSString stringWithUTF8String:_noteStore->currentText().c_str()];
    [_textView setString:text ?: @""];
    _syncing = NO;
    [self scheduleEvaluation];
}

- (void)createNewNote {
    if (_animating) return;
    if (_markdownPreview) {
        _markdownPreview = NO;
        _markdownPreviewView.hidden = YES;
        [self refreshIndicators];
    }

    // Animate: new note slides in from the left
    _noteStore->createNote();
    [self animateTransition:NO completion:^{
        // note already created, just load it
    }];
}

- (void)deleteCurrentNote {
    _noteStore->deleteNote(_noteStore->currentIndex());
    [self loadCurrentNote];
}

- (void)navigateTo:(int)index {
    if (_animating) return;
    if (_markdownPreview) {
        _markdownPreview = NO;
        _markdownPreviewView.hidden = YES;
        [self refreshIndicators];
    }

    int count = _noteStore->noteCount();
    if (count <= 1) return;

    BOOL slideLeft;
    if (index < 0) {
        index = count - 1;
        slideLeft = NO;
    } else if (index >= count) {
        index = 0;
        slideLeft = YES;
    } else {
        slideLeft = index > _noteStore->currentIndex();
    }
    if (index == _noteStore->currentIndex()) return;

    int pendingIndex = index;

    _animating = YES;

    // Snapshot current content
    NSRect visibleRect = _scrollView.frame;
    NSBitmapImageRep *snapshot = [_scrollView bitmapImageRepForCachingDisplayInRect:_scrollView.bounds];
    [_scrollView cacheDisplayInRect:_scrollView.bounds toBitmapImageRep:snapshot];
    NSImage *snapshotImage = [[NSImage alloc] initWithSize:_scrollView.bounds.size];
    [snapshotImage addRepresentation:snapshot];

    NSImageView *snapshotView = [[NSImageView alloc] initWithFrame:visibleRect];
    snapshotView.image = snapshotImage;
    snapshotView.imageScaling = NSImageScaleNone;
    snapshotView.wantsLayer = YES;
    [self.view addSubview:snapshotView positioned:NSWindowAbove relativeTo:_scrollView];

    // Load new note immediately
    _noteStore->setCurrentIndex(pendingIndex);
    [self loadCurrentNote];

    // Position scroll view off-screen on the incoming side
    CGFloat width = self.view.bounds.size.width;
    NSRect scrollOrigFrame = _scrollView.frame;
    NSRect scrollStartFrame = scrollOrigFrame;
    scrollStartFrame.origin.x = slideLeft ? width : -width;
    _scrollView.frame = scrollStartFrame;

    // Animate both
    __weak EditorViewController *weakSelf = self;
    [NSAnimationContext runAnimationGroup:^(NSAnimationContext *ctx) {
        ctx.duration = 0.2;
        ctx.timingFunction = [CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionEaseOut];

        // Snapshot slides out
        NSRect snapEndFrame = visibleRect;
        snapEndFrame.origin.x = slideLeft ? -width : width;
        snapshotView.animator.frame = snapEndFrame;

        // Real scroll view slides in
        self->_scrollView.animator.frame = scrollOrigFrame;
    } completionHandler:^{
        EditorViewController *s = weakSelf;
        if (!s) return;
        [snapshotView removeFromSuperview];
        s->_scrollView.frame = scrollOrigFrame;
        s->_animating = NO;
        [s->_textView.window makeFirstResponder:s->_textView];
        [s scheduleEvaluation];
    }];
}

- (void)animateTransition:(BOOL)slideLeft completion:(void (^)(void))completion {
    _animating = YES;

    NSRect visibleRect = _scrollView.frame;
    NSBitmapImageRep *snapshot = [_scrollView bitmapImageRepForCachingDisplayInRect:_scrollView.bounds];
    [_scrollView cacheDisplayInRect:_scrollView.bounds toBitmapImageRep:snapshot];
    NSImage *snapshotImage = [[NSImage alloc] initWithSize:_scrollView.bounds.size];
    [snapshotImage addRepresentation:snapshot];

    NSImageView *snapshotView = [[NSImageView alloc] initWithFrame:visibleRect];
    snapshotView.image = snapshotImage;
    snapshotView.imageScaling = NSImageScaleNone;
    snapshotView.wantsLayer = YES;
    [self.view addSubview:snapshotView positioned:NSWindowAbove relativeTo:_scrollView];

    [self loadCurrentNote];

    CGFloat width = self.view.bounds.size.width;
    NSRect scrollOrigFrame = _scrollView.frame;
    NSRect scrollStartFrame = scrollOrigFrame;
    scrollStartFrame.origin.x = slideLeft ? width : -width;
    _scrollView.frame = scrollStartFrame;

    __weak EditorViewController *weakSelf = self;
    [NSAnimationContext runAnimationGroup:^(NSAnimationContext *ctx) {
        ctx.duration = 0.2;
        ctx.timingFunction = [CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionEaseOut];

        NSRect snapEndFrame = visibleRect;
        snapEndFrame.origin.x = slideLeft ? -width : width;
        snapshotView.animator.frame = snapEndFrame;
        self->_scrollView.animator.frame = scrollOrigFrame;
    } completionHandler:^{
        EditorViewController *s = weakSelf;
        if (!s) return;
        [snapshotView removeFromSuperview];
        s->_scrollView.frame = scrollOrigFrame;
        s->_animating = NO;
        [s->_textView.window makeFirstResponder:s->_textView];
        if (completion) completion();
    }];
}

// ─── Text view delegate ─────────────────────────────────────────
- (void)textDidChange:(NSNotification *)notification {
    if (_syncing) return;
    _noteStore->setCurrentText(_textView.string.UTF8String);
    [self scheduleEvaluation];
}

- (void)scheduleEvaluation {
    if (_evalTimer) { dispatch_source_cancel(_evalTimer); _evalTimer = nil; }

    __weak EditorViewController *weakSelf = self;
    std::string text = _textView.string.UTF8String ?: "";

    _evalTimer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, dispatch_get_main_queue());
    dispatch_source_set_timer(_evalTimer, dispatch_time(DISPATCH_TIME_NOW, 50 * NSEC_PER_MSEC), DISPATCH_TIME_FOREVER, 0);
    dispatch_source_set_event_handler(_evalTimer, ^{
        EditorViewController *strong = weakSelf;
        if (!strong) return;

        bool hasMath = text.find("math:") != std::string::npos;
        if (hasMath)
            strong->_mathEngine->evaluate(text);
        else
            strong->_mathEngine->evaluate("");
        strong->_listEngine->evaluate(text);

        // Syntax highlighting for math mode
        [strong applySyntaxHighlighting:hasMath];

        [strong->_overlayView setNeedsDisplay:YES];
        [strong updateOverlayFrame];
        [strong refreshIndicators];
    });
    dispatch_resume(_evalTimer);
}

// ─── Syntax highlighting ────────────────────────────────────────
- (void)applySyntaxHighlighting:(bool)mathMode {
    NSTextStorage *storage = _textView.textStorage;
    NSFont *font = monoFont(_fontSize);
    NSColor *baseColor = textColor(_darkMode);

    // Reset all text to base color
    [storage beginEditing];
    [storage addAttribute:NSForegroundColorAttributeName value:baseColor range:NSMakeRange(0, storage.length)];
    [storage addAttribute:NSFontAttributeName value:font range:NSMakeRange(0, storage.length)];

    if (mathMode) {
        NSColor *varColor = [NSColor colorWithRed:0.42 green:0.54 blue:0.68 alpha:1]; // #6b8aad
        NSColor *colonColor = [NSColor colorWithRed:0.33 green:0.33 blue:0.33 alpha:1]; // #555555

        NSString *text = storage.string;
        NSArray<NSString *> *lines = [text componentsSeparatedByString:@"\n"];

        // Get variable names from the math engine
        auto &varNames = _mathEngine->variableNames();

        NSUInteger charOffset = 0;
        for (NSUInteger lineIdx = 0; lineIdx < lines.count; lineIdx++) {
            NSString *line = lines[lineIdx];
            NSString *trimmed = [line stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];

            if (trimmed.length == 0 || [trimmed hasPrefix:@"//"]) {
                charOffset += line.length + 1;
                continue;
            }

            // Highlight variable assignment: "name : expr"
            NSRange colonRange = [line rangeOfString:@":"];
            if (colonRange.location != NSNotFound && colonRange.location > 0) {
                NSString *before = [[line substringToIndex:colonRange.location] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
                NSRegularExpression *validName = [NSRegularExpression regularExpressionWithPattern:@"^[a-zA-Z][a-zA-Z0-9_ ]*$" options:0 error:nil];
                if ([validName numberOfMatchesInString:before options:0 range:NSMakeRange(0, before.length)] > 0
                    && ![before isEqualToString:@"math"]) {
                    [storage addAttribute:NSForegroundColorAttributeName value:varColor
                                    range:NSMakeRange(charOffset, colonRange.location)];
                    [storage addAttribute:NSForegroundColorAttributeName value:colonColor
                                    range:NSMakeRange(charOffset + colonRange.location, 1)];
                }
            }

            // Highlight variable references
            NSString *lowerLine = [line lowercaseString];
            for (auto &var : varNames) {
                NSString *varNS = [[NSString stringWithUTF8String:var.c_str()] lowercaseString];
                NSUInteger searchFrom = 0;

                // Skip the definition part on assignment lines
                if (colonRange.location != NSNotFound && colonRange.location > 0) {
                    NSString *before = [[line substringToIndex:colonRange.location] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
                    if ([[before lowercaseString] isEqualToString:varNS]) {
                        searchFrom = colonRange.location + 1;
                    }
                }

                while (searchFrom < lowerLine.length) {
                    NSRange found = [lowerLine rangeOfString:varNS options:0 range:NSMakeRange(searchFrom, lowerLine.length - searchFrom)];
                    if (found.location == NSNotFound) break;

                    BOOL leftOk = (found.location == 0) || ![[NSCharacterSet alphanumericCharacterSet] characterIsMember:[lowerLine characterAtIndex:found.location - 1]];
                    NSUInteger end = found.location + found.length;
                    BOOL rightOk = (end >= lowerLine.length) || ![[NSCharacterSet alphanumericCharacterSet] characterIsMember:[lowerLine characterAtIndex:end]];

                    if (leftOk && rightOk) {
                        [storage addAttribute:NSForegroundColorAttributeName value:varColor
                                        range:NSMakeRange(charOffset + found.location, found.length)];
                    }
                    searchFrom = found.location + found.length;
                }
            }

            charOffset += line.length + 1;
        }
    }

    [storage endEditing];
}

- (void)updateOverlayFrame {
    NSRect frame = _textView.bounds;
    if (_textView.layoutManager) {
        [_textView.layoutManager ensureLayoutForTextContainer:_textView.textContainer];
        NSRect usedRect = [_textView.layoutManager usedRectForTextContainer:_textView.textContainer];
        frame.size.height = fmax(frame.size.height, usedRect.size.height + 64);
    }
    _overlayView.frame = frame;
}

- (void)textViewFrameChanged:(NSNotification *)notification {
    [self updateOverlayFrame];
    [_overlayView setNeedsDisplay:YES];
}

- (void)scrollViewChanged:(NSNotification *)notification {
    [_overlayView setNeedsDisplay:YES];
}

// ─── Context menu ───────────────────────────────────────────────
- (NSMenu *)textView:(NSTextView *)view menu:(NSMenu *)menu forEvent:(NSEvent *)event atIndex:(NSUInteger)charIndex {
    NSMenu *contextMenu = [[NSMenu alloc] initWithTitle:@""];

    [contextMenu addItemWithTitle:@"New Note" action:@selector(createNewNote) keyEquivalent:@""];
    [contextMenu addItemWithTitle:@"Delete Note" action:@selector(deleteCurrentNote) keyEquivalent:@""];
    [contextMenu addItemWithTitle:@"Grid Overview" action:@selector(toggleGridOverview) keyEquivalent:@""];
    [contextMenu addItem:[NSMenuItem separatorItem]];

    NSMenuItem *mdItem = [[NSMenuItem alloc] initWithTitle:@"Markdown Preview" action:@selector(toggleMarkdownPreview) keyEquivalent:@""];
    mdItem.state = _markdownPreview ? NSControlStateValueOn : NSControlStateValueOff;
    [contextMenu addItem:mdItem];

    NSMenuItem *apItem = [[NSMenuItem alloc] initWithTitle:@"AutoPaste" action:@selector(toggleAutoPaste) keyEquivalent:@""];
    apItem.state = _autoPasteActive ? NSControlStateValueOn : NSControlStateValueOff;
    [contextMenu addItem:apItem];

    [contextMenu addItem:[NSMenuItem separatorItem]];
    [contextMenu addItemWithTitle:@"Increase Font" action:@selector(increaseFontAction) keyEquivalent:@""];
    [contextMenu addItemWithTitle:@"Decrease Font" action:@selector(decreaseFontAction) keyEquivalent:@""];
    [contextMenu addItem:[NSMenuItem separatorItem]];

    if (_syncManager) {
        if (_syncManager.authenticated) {
            [contextMenu addItemWithTitle:@"Sync Now" action:@selector(syncOrAuth) keyEquivalent:@""];
            [contextMenu addItemWithTitle:@"Disconnect GitHub" action:@selector(disconnectGitHub) keyEquivalent:@""];
        } else {
            [contextMenu addItemWithTitle:@"Connect GitHub" action:@selector(syncOrAuth) keyEquivalent:@""];
        }
        [contextMenu addItem:[NSMenuItem separatorItem]];
    }

    NSMenuItem *darkItem = [[NSMenuItem alloc] initWithTitle:@"Dark Mode" action:@selector(toggleDarkMode) keyEquivalent:@""];
    darkItem.state = _darkMode ? NSControlStateValueOn : NSControlStateValueOff;
    [contextMenu addItem:darkItem];

    [contextMenu addItem:[NSMenuItem separatorItem]];
    [contextMenu addItemWithTitle:@"Quit" action:@selector(quitApp) keyEquivalent:@""];

    return contextMenu;
}

- (void)increaseFontAction { [self changeFontSize:2]; }
- (void)decreaseFontAction { [self changeFontSize:-2]; }
- (void)quitApp { [NSApp terminate:nil]; }

- (void)dealloc {
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    if (_evalTimer) dispatch_source_cancel(_evalTimer);
    if (_pasteboardTimer) dispatch_source_cancel(_pasteboardTimer);
    if (_keyMonitor) [NSEvent removeMonitor:_keyMonitor];
    if (_scrollMonitor) [NSEvent removeMonitor:_scrollMonitor];
}

@end
