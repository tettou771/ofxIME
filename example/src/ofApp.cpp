#include "ofApp.h"

void ofApp::setup() {
    ofSetWindowTitle("ofxIME Example");
    ofBackground(40);

    // フォントの設定
    int fontSize = 24;
    ofTrueTypeFontSettings settings("/System/Library/Fonts/ヒラギノ角ゴシック W3.ttc", fontSize);
    settings.addRanges(ofAlphabet::Latin);
    settings.addRanges(ofAlphabet::Japanese);
    settings.addRange(ofUnicode::KatakanaHalfAndFullwidthForms);
    settings.addRange(ofUnicode::range{0x3000, 0x303F});
    font.load(settings);

    // 説明用の小さいフォント
    ofTrueTypeFontSettings smallSettings("/System/Library/Fonts/ヒラギノ角ゴシック W3.ttc", round(fontSize * 0.6));
    smallSettings.addRanges(ofAlphabet::Latin);
    smallSettings.addRanges(ofAlphabet::Japanese);
    smallSettings.addRange(ofUnicode::KatakanaHalfAndFullwidthForms);
    smallSettings.addRange(ofUnicode::range{0x3000, 0x303F});
    smallFont.load(smallSettings);

    // IMEにも同じフォントを設定
#ifdef TARGET_OS_MAC
    ime.setFont("/System/Library/Fonts/ヒラギノ角ゴシック W3.ttc", fontSize);
#else
    ime.setFont("C:\\Windows\\Fonts\\meiryo.ttc", fontSize);
#endif

    // ボタン領域の設定
    buttonRect = ofRectangle(20, 180, 100, 30);

    // 入力エリアの領域
    inputAreaRect = ofRectangle(18, 58, ofGetWidth() - 36, 100);

    ime.enable();
}

void ofApp::update() {
}

void ofApp::draw() {
    ofSetColor(255);

    // 入力エリアの説明
    smallFont.drawString("OSのIMEを使って日本語を入力できます", 20, 25);
    smallFont.drawString("ボタンをクリックすると入力内容を下のリストに追加します", 20, 45);

    // IME入力エリアを描画
    ofDrawRectangle(18, 58, ofGetWidth() - 36, 100);
    ofSetColor(0);
    ime.draw(20, 90);

    // 送信ボタン
    ofSetColor(80, 140, 200);
    ofDrawRectangle(buttonRect);
    ofSetColor(255);
    smallFont.drawString("送信", buttonRect.x + 30, buttonRect.y + 22);

    // 確定済みテキストの履歴を表示
    ofSetColor(200);
    float y = 260;
    float lineHeight = font.getLineHeight();
    for (auto &text : confirmedTexts) {
        font.drawString(text, 20, y);
        y += lineHeight;
    }

    // 現在の入力モードを表示
    if (ime.isJapaneseMode()) {
        ofSetColor(100, 200, 100);
        smallFont.drawString("日本語", ofGetWidth() - 80, 25);
    } else {
        ofSetColor(150);
        smallFont.drawString("英数", ofGetWidth() - 80, 25);
    }
}

void ofApp::keyPressed(int key) {
    // Ctrl + E でIMEのenable/disable切り替え
    if (key == 'e' && ofGetKeyPressed(OF_KEY_CONTROL)) {
        if (ime.isEnabled()) {
            ime.disable();
        } else {
            ime.enable();
        }
    }
}

void ofApp::mousePressed(int x, int y, int button) {
    // 送信ボタンがクリックされたら
    if (buttonRect.inside(x, y)) {
        string text = ime.getString();
        if (!text.empty()) {
            confirmedTexts.push_back(text);
            ime.clear();
        }
    }
    // 入力エリア内をクリックしたらenable、外ならdisable
    else if (inputAreaRect.inside(x, y)) {
        if (!ime.isEnabled()) {
            ime.enable();
        }
        // クリック位置にカーソルを移動
        ime.setCursorByMouse(x, y);
    }
    else {
        if (ime.isEnabled()) {
            ime.disable();
        }
    }
}
