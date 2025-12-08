#pragma once

#ifdef __APPLE__

#import <Cocoa/Cocoa.h>

// 前方宣言
class ofxIME;

// NSTextInputClientプロトコルを実装したNSView
// OSのIMEから入力を受け取り、ofxIMEに渡す
@interface ofxIMEView : NSView <NSTextInputClient>

@property (nonatomic, weak) NSView *originalView;
@property (nonatomic, assign) ofxIME *imeInstance;

// 未確定文字列を保持
@property (nonatomic, strong) NSMutableAttributedString *markedTextStorage;
@property (nonatomic, assign) NSRange markedRange;
@property (nonatomic, assign) NSRange selectedRange;

// IMEが文字を処理したかどうかのフラグ
@property (nonatomic, assign) BOOL handledByIME;

- (instancetype)initWithFrame:(NSRect)frameRect imeInstance:(ofxIME*)ime;
- (void)setOriginalView:(NSView *)view;
- (void)setImeInstance:(ofxIME*)ime;

@end

#endif
