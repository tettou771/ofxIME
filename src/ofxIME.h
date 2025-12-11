#pragma once

#include <string>

// OS固有のヘッダ
#ifdef WIN32
#include <Windows.h>
#include <Imm.h>
#pragma comment(lib,"imm32.lib")
#elif defined __APPLE__
#include <Carbon/Carbon.h>
#endif

#include "ofMain.h"
using namespace std;

// 前方宣言（非テンプレートベースクラス）
class ofxIMEBase {
public:
    ofxIMEBase() {
        ofSetEscapeQuitsApp(false);
        state = Eisu;
        clear();
    }
    virtual ~ofxIMEBase() = default;

    void enable();
    void disable();
    void clear();

    bool isEnabled() { return enabled; }
    bool isJapaneseMode() { return state == Kana || state == Composing; }

    // u32stringで内部保持し、getStringでUTF-8に変換して返す
    string getString();
    void setString(const string &str);
    u32string getU32String();
    string getLine(int l);
    string getLineSubstr(int l, int begin, int end);
    string getMarkedText();
    string getMarkedTextSubstr(int begin, int end);

    // IMEから受け取った文字列を挿入（OSからのコールバック用）
    void insertText(const u32string &str);
    void setMarkedTextFromOS(const u32string &str, int selectedLocation, int selectedLength);
    void unmarkText();

    // 変換候補の設定（OSからのコールバック用）
    void setCandidates(const vector<u32string> &cands, int selectedIndex);
    void clearCandidates();

    // 描画位置のスクリーン座標を返す（IME候補ウィンドウ表示用）
    // テンプレートサブクラスでオーバーライド
    virtual ofVec2f getMarkedTextScreenPosition() { return lastDrawPos; }

    static string UTF32toUTF8(const u32string &u32str);
    static string UTF32toUTF8(const char32_t &u32char);
    static u32string UTF8toUTF32(const string &str);

protected:
    bool enabled = false;
    ofVec2f lastDrawPos;  // 最後にdrawした位置を記憶（候補ウィンドウ用）

    // 未確定文字列（marked text）
    u32string markedText;
    int markedSelectedLocation = 0;  // 選択開始位置
    int markedSelectedLength = 0;     // 選択範囲長

    // 変換候補
    vector<u32string> candidates;
    int candidateSelectedIndex = 0;

    // 確定済み文字列
    vector<u32string> line; // 各行の文字列をvectorで持つ

    // 選択範囲
    typedef tuple<int, int> TextSelectPos;
    TextSelectPos selectBegin, selectEnd;
    void selectCancel() {
        selectBegin = selectEnd = TextSelectPos(0, 0);
    }
    bool isSelected() {
        return selectBegin != selectEnd;
    }
    void selectAll() {
        selectBegin = TextSelectPos(0, 0);
        selectEnd = TextSelectPos(line.size() - 1, line.back().length());
    }
    void deleteSelected();

    // 改行して新しい行を作る
    void newLine();

    // 行の移動
    void lineChange(int n);

    // カーソル（確定済み文字列内の位置）
    int cursorLine; // 何行目にいるか
    int cursorPos;  // 行内の何文字目か

    // 指定した位置に文字列を挿入する関数
    void addStr(u32string &target, const u32string &str, int &p);

    // カーソル位置の文字を削除する関数
    void backspaceCharacter(u32string &str, int &pos, bool lineMerge = false);
    void deleteCharacter(u32string &str, int &pos, bool lineMerge = false);

#ifdef WIN32
    // 内部処理用のUTF-32 → Shift-JIS に変換する関数（Windowsのみ必要）
    string UTF32toSjis(u32string srcUTF8);
#endif

    // State
    enum State {
        Eisu,           // 英数入力モード
        Kana,           // かな入力モード（未変換）
        Composing       // 変換中（未確定文字列あり）
    };
    State state;

    // OSのIMEを監視して状態を同期
    void startIMEObserver();
    void stopIMEObserver();
    void syncWithSystemIME();

    // キーボードイベントのハンドラ
    void keyPressed(ofKeyEventArgs &key);

#ifdef __APPLE__
    static void onInputSourceChanged(CFNotificationCenterRef center,
                                     void *observer,
                                     CFNotificationName name,
                                     const void *object,
                                     CFDictionaryRef userInfo);

    // IME入力受け取り用のView（グローバルで1つ共有）
    static void* sharedIMEView;              // ofxIMEView*
    static void* sharedOriginalContentView;  // NSView*
    static ofxIMEBase* activeIMEInstance;    // 現在アクティブなIMEインスタンス
    static int imeViewRefCount;              // 参照カウント

    // インスタンスメソッド
    void setupIMEInputView();
    void removeIMEInputView();
    void becomeActiveIME();
#endif

#ifdef WIN32
    // WindowsでのIME状態監視用
    void checkIMEState(ofEventArgs &args);
    DWORD lastIMEConversionMode = 0;
#endif

    // カーソルの点滅タイミング用
    float cursorBlinkOffsetTime;

    // 変換候補表示位置のアニメーション (0-1)
    float movingY;
};

// テンプレート化されたメインクラス
template<typename FontType = ofTrueTypeFont>
class ofxIME : public ofxIMEBase {
public:
    ofxIME() {}
    ~ofxIME() {}

    // フォント設定
    void setFont(string path, float fontSize) {
        ofTrueTypeFontSettings settings(path, fontSize);
        settings.addRanges(ofAlphabet::Latin);
        settings.addRanges(ofAlphabet::Japanese);
        settings.addRange(ofUnicode::KatakanaHalfAndFullwidthForms);
        settings.addRange(ofUnicode::range{0x3000, 0x303F}); // CJK symbols and punctuation
        font.load(settings);
        fontPtr = nullptr;  // 自前フォントを使用
    }

    void setFont(FontType* sharedFont) {
        fontPtr = sharedFont;
    }

    // 描画（確定済み文字列+未確定文字列）
    void draw(ofPoint pos) {
        draw(pos.x, pos.y);
    }

    void draw(float x, float y) {
        FontType& f = getFont();
        if (!f.isLoaded()) {
            ofLogError("ofxIME") << "font is not loaded.";
            return;
        }

        // Store draw position for mouse click detection
        lastDrawPos = ofVec2f(x, y);

        // Animation easing effect
        movingY *= 0.7;

        float fontSize = f.getSize();
        float lineHeight = f.getLineHeight();
        float margin = fontSize * 0.1;

        // Cursor drawing function
        auto drawCursor = [=, this](float cx, float cy) {
            if (!enabled) return;
            if (fmod(ofGetElapsedTimef() - cursorBlinkOffsetTime, 0.8) < 0.4) {
                ofSetLineWidth(2);
                ofDrawLine(cx + 1, cy, cx + 1, cy - fontSize * 1.2);
            }
        };

        ofPushMatrix();
        ofTranslate(x, y);

        for (int i = 0; i < (int)line.size(); ++i) {
            // Check if this is the current input line
            if (i != cursorLine) {
                // Non-active line
                f.drawString(UTF32toUTF8(line[i]), 0, 0);
            }
            else {
                // Current input line
                ofPushMatrix();

                // Confirmed text before cursor
                string beforeCursor = getLineSubstr(cursorLine, 0, cursorPos);
                f.drawString(beforeCursor, 0, 0);
                float beforeW = f.stringWidth(beforeCursor);

                ofTranslate(beforeW, 0);

                // If there is marked (composing) text
                if (markedText.length() > 0) {
                    ofTranslate(margin, 0);

                    string markedStr = getMarkedText();
                    f.drawString(markedStr, 0, 0);
                    float markedW = f.stringWidth(markedStr);

                    // Draw underlines for marked text segments
                    // First, draw thin underline for non-selected part (before selection)
                    string selStart = getMarkedTextSubstr(0, markedSelectedLocation);
                    float selStartW = f.stringWidth(selStart);

                    if (markedSelectedLength > 0) {
                        // There is a selected range
                        string selText = getMarkedTextSubstr(markedSelectedLocation, markedSelectedLength);
                        float selW = f.stringWidth(selText);

                        // Thin underline before selection
                        if (selStartW > 0) {
                            ofSetLineWidth(1);
                            ofDrawLine(1, fontSize * 0.2, selStartW - 1, fontSize * 0.2);
                        }

                        // Thick underline for selected part
                        ofSetLineWidth(3);
                        ofDrawLine(selStartW + 1, fontSize * 0.2, selStartW + selW - 1, fontSize * 0.2);

                        // Thin underline after selection
                        if (selStartW + selW < markedW) {
                            ofSetLineWidth(1);
                            ofDrawLine(selStartW + selW + 1, fontSize * 0.2, markedW - 1, fontSize * 0.2);
                        }
                    }
                    else {
                        // No selection, draw thin underline for entire marked text
                        ofSetLineWidth(1);
                        ofDrawLine(1, fontSize * 0.2, markedW - 1, fontSize * 0.2);
                    }

                    // Draw conversion candidates
                    if (candidates.size() > 0) {
                        float lh = f.getLineHeight();
                        ofPushMatrix();
                        ofTranslate(0, lh);  // Display below marked text

                        for (int j = 0; j < (int)candidates.size(); ++j) {
                            string candStr = UTF32toUTF8(candidates[j]);

                            if (j == candidateSelectedIndex) {
                                // Highlight selected candidate with background
                                ofPushStyle();
                                ofFill();
                                ofSetColor(100, 150);
                                float candW = f.stringWidth(candStr);
                                ofDrawRectangle(-2, -fontSize, candW + 4, lh);
                                ofPopStyle();
                            }

                            f.drawString(candStr, 0, 0);
                            ofTranslate(0, lh);
                        }
                        ofPopMatrix();
                    }

                    ofTranslate(markedW + margin, 0);
                }
                else {
                    // No marked text - draw cursor
                    drawCursor(0, 0);
                }

                // Confirmed text after cursor
                string afterCursor = getLineSubstr(cursorLine, cursorPos, (int)line[cursorLine].length() - cursorPos);
                f.drawString(afterCursor, 0, 0);

                ofPopMatrix();
            }

            // Move to next line
            ofTranslate(0, lineHeight);
        }

        ofPopMatrix();
    }

    // マウスクリック位置にカーソルを移動
    void setCursorByMouse(float x, float y) {
        FontType& f = getFont();
        auto bbox = f.getStringBoundingBox(getString(), lastDrawPos.x, lastDrawPos.y);

        // Check if click is inside bbox
        if (!bbox.inside(x, y)) return;

        ofVec2f rel = ofVec2f(x, y) - bbox.position;

        int lineNumber = ofMap(rel.y, 0, bbox.height, 0, line.size(), true);
        lineNumber = MIN(lineNumber, (int)line.size() - 1);

        // Find clicked character
        auto lineBbox = f.getStringBoundingBox(UTF32toUTF8(line[lineNumber]), lastDrawPos.x, lastDrawPos.y + f.getLineHeight() * lineNumber);
        int posNumber = ofMap(x, 0, lineBbox.width, 0, line[lineNumber].size());
        posNumber = MIN(posNumber, (int)line[cursorLine].size());

        // Update cursor position
        cursorLine = lineNumber;
        cursorPos = posNumber;
        cursorBlinkOffsetTime = ofGetElapsedTimef();
    }

    // 描画位置のスクリーン座標を返す（IME候補ウィンドウ表示用）
    ofVec2f getMarkedTextScreenPosition() override {
        FontType& f = getFont();
        float x = lastDrawPos.x;
        float y = lastDrawPos.y;

        // Calculate Y coordinate for current line
        y += f.getLineHeight() * cursorLine;

        // Calculate X coordinate for cursor position
        string beforeCursor = getLineSubstr(cursorLine, 0, cursorPos);
        x += f.stringWidth(beforeCursor);

        return ofVec2f(x, y);
    }

private:
    // 描画用のフォント
    FontType font;
    FontType* fontPtr = nullptr;  // 共有フォント使用時のポインタ
    FontType& getFont() { return fontPtr ? *fontPtr : font; }

    // マウスイベントのハンドラ
    void mousePressed(ofMouseEventArgs &mouse) {
        setCursorByMouse(mouse.x, mouse.y);
    }
};

// 後方互換性のためのエイリアス（ofxIME ime; でそのまま使える）
// using ofxIMEDefault = ofxIME<ofTrueTypeFont>;
