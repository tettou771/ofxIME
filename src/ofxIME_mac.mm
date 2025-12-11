// macOS固有の実装

#ifdef __APPLE__

#include "ofxIME.h"
#include "ofxIMEView.h"

#import <Cocoa/Cocoa.h>
#import <Carbon/Carbon.h>

#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

// 静的メンバの定義
void* ofxIMEBase::sharedIMEView = nullptr;
void* ofxIMEBase::sharedOriginalContentView = nullptr;
ofxIMEBase* ofxIMEBase::activeIMEInstance = nullptr;
int ofxIMEBase::imeViewRefCount = 0;

void ofxIMEBase::startIMEObserver() {
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

void ofxIMEBase::stopIMEObserver() {
    CFNotificationCenterRemoveObserver(
        CFNotificationCenterGetDistributedCenter(),
        this,
        kTISNotifySelectedKeyboardInputSourceChanged,
        NULL
    );
}

void ofxIMEBase::onInputSourceChanged(CFNotificationCenterRef center,
                                  void *observer,
                                  CFNotificationName name,
                                  const void *object,
                                  CFDictionaryRef userInfo) {
    // observerはofxIMEBaseインスタンスへのポインタ
    ofxIMEBase *ime = static_cast<ofxIMEBase*>(observer);
    if (ime) {
        ime->syncWithSystemIME();
    }
}

void ofxIMEBase::syncWithSystemIME() {
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

void ofxIMEBase::setupIMEInputView() {
    // 参照カウントを増やす
    imeViewRefCount++;

    // 既にViewが存在する場合は、このインスタンスをアクティブにするだけ
    if (sharedIMEView != nullptr) {
        becomeActiveIME();
        return;
    }

    // GLFWウィンドウからNSWindowを取得
    GLFWwindow* glfwWin = (GLFWwindow*)ofGetWindowPtr()->getWindowContext();
    if (!glfwWin) return;

    NSWindow* nsWindow = glfwGetCocoaWindow(glfwWin);
    if (!nsWindow) return;

    NSView* contentView = [nsWindow contentView];
    if (!contentView) return;

    // 元のcontentViewを保存（グローバル）
    sharedOriginalContentView = (__bridge void*)contentView;

    // カスタムViewを作成（このインスタンスへの参照を渡す）
    ofxIMEView* customView = [[ofxIMEView alloc] initWithFrame:[contentView frame] imeInstance:this];
    [customView setOriginalView:contentView];
    [customView setAutoresizingMask:[contentView autoresizingMask]];

    sharedIMEView = (__bridge_retained void*)customView;
    activeIMEInstance = this;

    // contentViewの上にカスタムViewを追加してFirstResponderにする
    [contentView addSubview:customView];
    [nsWindow makeFirstResponder:customView];
}

void ofxIMEBase::removeIMEInputView() {
    // 参照カウントを減らす
    imeViewRefCount--;

    // まだ他のIMEが使用中なら削除しない
    if (imeViewRefCount > 0) {
        // このインスタンスがアクティブだった場合、アクティブを解除
        if (activeIMEInstance == this) {
            activeIMEInstance = nullptr;
        }
        return;
    }

    // 最後のIMEがdisableされたのでViewを削除
    if (sharedIMEView == nullptr) return;

    ofxIMEView* customView = (__bridge_transfer ofxIMEView*)sharedIMEView;
    sharedIMEView = nullptr;
    activeIMEInstance = nullptr;

    // カスタムViewを削除
    [customView removeFromSuperview];

    // 元のcontentViewをFirstResponderに戻す
    if (sharedOriginalContentView != nullptr) {
        NSView* origView = (__bridge NSView*)sharedOriginalContentView;
        NSWindow* nsWindow = [origView window];
        if (nsWindow) {
            [nsWindow makeFirstResponder:origView];
        }
        sharedOriginalContentView = nullptr;
    }
}

void ofxIMEBase::becomeActiveIME() {
    if (sharedIMEView == nullptr) return;

    // ViewのimeInstanceを自分に差し替える
    ofxIMEView* customView = (__bridge ofxIMEView*)sharedIMEView;
    [customView setImeInstance:this];
    activeIMEInstance = this;
}

// C-style callback functions for ofxIMEView (to avoid header conflicts)
extern "C" {

void ofxIME_insertText(ofxIMEBase* ime, const char32_t* str, size_t len) {
    if (ime && str) {
        u32string u32str(str, len);
        ime->insertText(u32str);
    }
}

void ofxIME_setMarkedText(ofxIMEBase* ime, const char32_t* str, size_t len, int selLoc, int selLen) {
    if (ime) {
        u32string u32str(str, len);
        ime->setMarkedTextFromOS(u32str, selLoc, selLen);
    }
}

void ofxIME_unmarkText(ofxIMEBase* ime) {
    if (ime) {
        ime->unmarkText();
    }
}

void ofxIME_getMarkedTextScreenPosition(ofxIMEBase* ime, float* x, float* y) {
    if (ime && x && y) {
        ofVec2f pos = ime->getMarkedTextScreenPosition();
        *x = pos.x;
        *y = pos.y;
    }
}

} // extern "C"

#endif
