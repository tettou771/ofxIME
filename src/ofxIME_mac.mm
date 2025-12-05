// macOS固有の実装

#ifdef __APPLE__

#include "ofxIME.h"
#include "ofxIMEView.h"

#import <Cocoa/Cocoa.h>
#import <Carbon/Carbon.h>

#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

void ofxIME::startIMEObserver() {
    CFNotificationCenterAddObserver(
        CFNotificationCenterGetDistributedCenter(),
        this,
        onInputSourceChanged,
        kTISNotifySelectedKeyboardInputSourceChanged,
        NULL,
        CFNotificationSuspensionBehaviorDeliverImmediately
    );

    // 初期状態を同期
    syncWithSystemIME();
}

void ofxIME::stopIMEObserver() {
    CFNotificationCenterRemoveObserver(
        CFNotificationCenterGetDistributedCenter(),
        this,
        kTISNotifySelectedKeyboardInputSourceChanged,
        NULL
    );
}

void ofxIME::onInputSourceChanged(CFNotificationCenterRef center,
                                  void *observer,
                                  CFNotificationName name,
                                  const void *object,
                                  CFDictionaryRef userInfo) {
    // observerはofxIMEインスタンスへのポインタ
    ofxIME *ime = static_cast<ofxIME*>(observer);
    if (ime) {
        ime->syncWithSystemIME();
    }
}

void ofxIME::syncWithSystemIME() {
    TISInputSourceRef source = TISCopyCurrentKeyboardInputSource();
    if (source) {
        CFStringRef sourceID = (CFStringRef)TISGetInputSourceProperty(source, kTISPropertyInputSourceID);
        if (sourceID) {
            // 日本語入力ソースかどうかを判定
            bool isJapanese = (CFStringFind(sourceID, CFSTR("Japanese"), 0).location != kCFNotFound) ||
                              (CFStringFind(sourceID, CFSTR("Hiragana"), 0).location != kCFNotFound);

            if (isJapanese) {
                // 日本語モードに切り替え
                if (state == Eisu) {
                    state = Kana;
                }
            } else {
                // 英数モードに切り替え
                if (state == Composing) {
                    // 未確定文字列があれば確定
                    unmarkText();
                }
                state = Eisu;
            }
        }
        CFRelease(source);
    }
}

void ofxIME::setupIMEInputView() {
    if (imeInputView != nullptr) return;

    // GLFWウィンドウからNSWindowを取得
    GLFWwindow* glfwWin = (GLFWwindow*)ofGetWindowPtr()->getWindowContext();
    if (!glfwWin) return;

    NSWindow* nsWindow = glfwGetCocoaWindow(glfwWin);
    if (!nsWindow) return;

    NSView* contentView = [nsWindow contentView];
    if (!contentView) return;

    // 元のcontentViewを保存
    originalContentView = (__bridge void*)contentView;

    // カスタムViewを作成（ofxIMEへの参照を渡す）
    ofxIMEView* customView = [[ofxIMEView alloc] initWithFrame:[contentView frame] imeInstance:this];
    [customView setOriginalView:contentView];
    [customView setAutoresizingMask:[contentView autoresizingMask]];

    imeInputView = (__bridge_retained void*)customView;

    // contentViewの上にカスタムViewを追加してFirstResponderにする
    [contentView addSubview:customView];
    [nsWindow makeFirstResponder:customView];
}

void ofxIME::removeIMEInputView() {
    if (imeInputView == nullptr) return;

    ofxIMEView* customView = (__bridge_transfer ofxIMEView*)imeInputView;
    imeInputView = nullptr;

    // カスタムViewを削除
    [customView removeFromSuperview];

    // 元のcontentViewをFirstResponderに戻す
    if (originalContentView != nullptr) {
        NSView* origView = (__bridge NSView*)originalContentView;
        NSWindow* nsWindow = [origView window];
        if (nsWindow) {
            [nsWindow makeFirstResponder:origView];
        }
        originalContentView = nullptr;
    }
}

// C-style callback functions for ofxIMEView (to avoid header conflicts)
extern "C" {

void ofxIME_insertText(ofxIME* ime, const char32_t* str, size_t len) {
    if (ime && str) {
        u32string u32str(str, len);
        ime->insertText(u32str);
    }
}

void ofxIME_setMarkedText(ofxIME* ime, const char32_t* str, size_t len, int selLoc, int selLen) {
    if (ime) {
        u32string u32str(str, len);
        ime->setMarkedTextFromOS(u32str, selLoc, selLen);
    }
}

void ofxIME_unmarkText(ofxIME* ime) {
    if (ime) {
        ime->unmarkText();
    }
}

void ofxIME_getMarkedTextScreenPosition(ofxIME* ime, float* x, float* y) {
    if (ime && x && y) {
        ofVec2f pos = ime->getMarkedTextScreenPosition();
        *x = pos.x;
        *y = pos.y;
    }
}

} // extern "C"

#endif
