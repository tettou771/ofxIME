// ofxIMEView.mm
// Note: We avoid including ofxIME.h here to prevent GLEW/OpenGL header conflicts
// Instead we use forward declaration and C-style callback functions

#import "ofxIMEView.h"
#include <vector>

// Forward declaration - we'll use extern functions to call ofxIMEBase methods
class ofxIMEBase;

// C++ helper functions (implemented in ofxIME_mac.mm)
extern "C" {
    void ofxIME_insertText(ofxIMEBase* ime, const char32_t* str, size_t len);
    void ofxIME_setMarkedText(ofxIMEBase* ime, const char32_t* str, size_t len, int selLoc, int selLen);
    void ofxIME_unmarkText(ofxIMEBase* ime);
    void ofxIME_getMarkedTextScreenPosition(ofxIMEBase* ime, float* x, float* y);
}

@implementation ofxIMEView

- (instancetype)initWithFrame:(NSRect)frameRect imeInstance:(ofxIMEBase*)ime {
    self = [super initWithFrame:frameRect];
    if (self) {
        _originalView = nil;
        _imeInstance = ime;
        _markedTextStorage = [[NSMutableAttributedString alloc] init];
        _markedRange = NSMakeRange(NSNotFound, 0);
        _selectedRange = NSMakeRange(0, 0);
        _handledByIME = NO;
    }
    return self;
}

- (void)setOriginalView:(NSView *)view {
    _originalView = view;
}

- (void)setImeInstance:(ofxIMEBase *)ime {
    _imeInstance = ime;
}

// Forward key events to IME and original view
- (void)keyDown:(NSEvent *)event {
    // Reset flag before processing
    _handledByIME = NO;

    // Let IME process the event
    // This may call insertText: or setMarkedText: which will set _handledByIME = YES
    [self interpretKeyEvents:@[event]];

    // Only forward to original view if IME didn't handle it
    // This prevents double input (once from IME, once from oF key handler)
    if (!_handledByIME && _originalView) {
        [_originalView keyDown:event];
    }
}

- (void)keyUp:(NSEvent *)event {
    if (_originalView) {
        [_originalView keyUp:event];
    }
}

- (void)flagsChanged:(NSEvent *)event {
    if (_originalView) {
        [_originalView flagsChanged:event];
    }
}

// Forward mouse events
- (void)mouseDown:(NSEvent *)event {
    if (_originalView) {
        [_originalView mouseDown:event];
    }
}

- (void)mouseUp:(NSEvent *)event {
    if (_originalView) {
        [_originalView mouseUp:event];
    }
}

- (void)mouseMoved:(NSEvent *)event {
    if (_originalView) {
        [_originalView mouseMoved:event];
    }
}

- (void)mouseDragged:(NSEvent *)event {
    if (_originalView) {
        [_originalView mouseDragged:event];
    }
}

- (void)scrollWheel:(NSEvent *)event {
    if (_originalView) {
        [_originalView scrollWheel:event];
    }
}

- (void)rightMouseDown:(NSEvent *)event {
    if (_originalView) {
        [_originalView rightMouseDown:event];
    }
}

- (void)rightMouseUp:(NSEvent *)event {
    if (_originalView) {
        [_originalView rightMouseUp:event];
    }
}

- (BOOL)acceptsFirstResponder {
    return YES;
}

- (BOOL)canBecomeKeyView {
    return YES;
}

#pragma mark - NSTextInputClient Protocol

// Insert confirmed text
- (void)insertText:(id)string replacementRange:(NSRange)replacementRange {
    // Mark as handled by IME
    _handledByIME = YES;

    NSString *insertString;
    if ([string isKindOfClass:[NSAttributedString class]]) {
        insertString = [(NSAttributedString*)string string];
    } else {
        insertString = (NSString*)string;
    }

    // Skip control characters (like BS = 0x08 sent after backspace)
    if (insertString.length == 1) {
        unichar c = [insertString characterAtIndex:0];
        if (c < 0x20 || c == 0x7F) {
            return;
        }
    }

    // Clear marked text
    _markedRange = NSMakeRange(NSNotFound, 0);
    [_markedTextStorage setAttributedString:[[NSAttributedString alloc] init]];

    // Pass confirmed text to ofxIME
    if (_imeInstance && insertString.length > 0) {
        // Convert NSString to u32string
        std::vector<char32_t> u32vec;
        for (NSUInteger i = 0; i < insertString.length; ++i) {
            unichar c = [insertString characterAtIndex:i];
            // Handle surrogate pairs
            if (CFStringIsSurrogateHighCharacter(c) && i + 1 < insertString.length) {
                unichar low = [insertString characterAtIndex:++i];
                if (CFStringIsSurrogateLowCharacter(low)) {
                    u32vec.push_back(CFStringGetLongCharacterForSurrogatePair(c, low));
                }
            } else {
                u32vec.push_back((char32_t)c);
            }
        }
        ofxIME_insertText(_imeInstance, u32vec.data(), u32vec.size());
    }
}

// Execute command (Enter, Tab, arrow keys, etc.)
- (void)doCommandBySelector:(SEL)selector {
    // Check if modifier keys are pressed - if so, let oF handle it
    NSEventModifierFlags modifiers = [NSEvent modifierFlags];
    BOOL hasModifier = (modifiers & (NSEventModifierFlagCommand | NSEventModifierFlagControl)) != 0;

    // Handle newline/insert commands - mark as handled so oF doesn't double-process
    // But if modifier is pressed (like Ctrl+Enter), let oF handle it
    if (!hasModifier &&
        (selector == @selector(insertNewline:) ||
         selector == @selector(insertLineBreak:))) {
        // Insert newline via IME
        _handledByIME = YES;
        if (_imeInstance) {
            char32_t newline = U'\n';
            ofxIME_insertText(_imeInstance, &newline, 1);
        }
        return;
    }

    // Other commands (deleteBackward, arrow keys, etc.) - don't mark as handled
    // so they get forwarded to oF via keyDown
}

// Set marked (composing) text
- (void)setMarkedText:(id)string selectedRange:(NSRange)selectedRange replacementRange:(NSRange)replacementRange {
    // Mark as handled by IME
    _handledByIME = YES;

    NSString *markedString;
    if ([string isKindOfClass:[NSAttributedString class]]) {
        [_markedTextStorage setAttributedString:(NSAttributedString*)string];
        markedString = [(NSAttributedString*)string string];
    } else {
        [_markedTextStorage setAttributedString:[[NSAttributedString alloc] initWithString:(NSString*)string]];
        markedString = (NSString*)string;
    }

    if (markedString.length > 0) {
        _markedRange = NSMakeRange(0, markedString.length);
        _selectedRange = selectedRange;
    } else {
        _markedRange = NSMakeRange(NSNotFound, 0);
        _selectedRange = NSMakeRange(0, 0);
    }

    // Pass marked text to ofxIME
    if (_imeInstance) {
        // Convert NSString to u32string
        std::vector<char32_t> u32vec;
        for (NSUInteger i = 0; i < markedString.length; ++i) {
            unichar c = [markedString characterAtIndex:i];
            // Handle surrogate pairs
            if (CFStringIsSurrogateHighCharacter(c) && i + 1 < markedString.length) {
                unichar low = [markedString characterAtIndex:++i];
                if (CFStringIsSurrogateLowCharacter(low)) {
                    u32vec.push_back(CFStringGetLongCharacterForSurrogatePair(c, low));
                }
            } else {
                u32vec.push_back((char32_t)c);
            }
        }
        ofxIME_setMarkedText(_imeInstance, u32vec.data(), u32vec.size(),
                            (int)selectedRange.location, (int)selectedRange.length);
    }
}

// Confirm marked text
- (void)unmarkText {
    _markedRange = NSMakeRange(NSNotFound, 0);
    [_markedTextStorage setAttributedString:[[NSAttributedString alloc] init]];

    if (_imeInstance) {
        ofxIME_unmarkText(_imeInstance);
    }
}

// Return selected range
- (NSRange)selectedRange {
    return _selectedRange;
}

// Return marked text range
- (NSRange)markedRange {
    return _markedRange;
}

// Check if has marked text
- (BOOL)hasMarkedText {
    return _markedRange.location != NSNotFound && _markedRange.length > 0;
}

// Return attributed substring for range
- (nullable NSAttributedString *)attributedSubstringForProposedRange:(NSRange)range actualRange:(nullable NSRangePointer)actualRange {
    if (_markedRange.location != NSNotFound && _markedTextStorage.length > 0) {
        if (range.location < _markedTextStorage.length) {
            NSRange adjustedRange = NSMakeRange(range.location, MIN(range.length, _markedTextStorage.length - range.location));
            if (actualRange) {
                *actualRange = adjustedRange;
            }
            return [_markedTextStorage attributedSubstringFromRange:adjustedRange];
        }
    }
    return nil;
}

// Valid attributes for marked text
- (NSArray<NSAttributedStringKey> *)validAttributesForMarkedText {
    return @[NSUnderlineStyleAttributeName, NSUnderlineColorAttributeName];
}

// Return screen rect for character range (for IME candidate window positioning)
- (NSRect)firstRectForCharacterRange:(NSRange)range actualRange:(nullable NSRangePointer)actualRange {
    if (!_imeInstance) {
        return NSMakeRect(0, 0, 0, 0);
    }

    // Get cursor position from ofxIME
    float posX = 0, posY = 0;
    ofxIME_getMarkedTextScreenPosition(_imeInstance, &posX, &posY);

    // Convert from oF coordinate system to screen coordinates
    NSWindow *window = self.window;
    if (!window) {
        return NSMakeRect(0, 0, 0, 0);
    }

    // oF uses top-left origin, macOS uses bottom-left origin
    NSRect windowFrame = [window frame];
    NSRect contentRect = [window contentRectForFrameRect:windowFrame];

    // Window-local coordinates
    CGFloat windowX = posX;
    CGFloat windowY = contentRect.size.height - posY;  // Flip Y coordinate

    // Convert to screen coordinates
    NSPoint screenPoint = [window convertPointToScreen:NSMakePoint(windowX, windowY)];

    // Return rect for IME candidate window (height is arbitrary 20 pixels)
    return NSMakeRect(screenPoint.x, screenPoint.y - 20, 0, 20);
}

// Return character index for point
- (NSUInteger)characterIndexForPoint:(NSPoint)point {
    return NSNotFound;
}

@end
