#pragma once

#include "ofMain.h"
#include "ofxIME.h"

class ofApp : public ofBaseApp {
public:
    void setup();
    void update();
    void draw();
    void keyPressed(int key);
    void mousePressed(int x, int y, int button);

    ofxIME ime;
    ofTrueTypeFont font;       // 日本語表示用フォント
    ofTrueTypeFont smallFont;  // 説明用の小さいフォント
    vector<string> confirmedTexts;  // 確定済みテキストの履歴
    ofRectangle buttonRect;     // 送信ボタンの領域
    ofRectangle inputAreaRect;  // 入力エリアの領域
};
