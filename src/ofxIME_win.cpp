// Windows固有の実装

#ifdef WIN32

#include "ofxIME.h"
#include <codecvt>

void ofxIME::startIMEObserver() {
    // 初期状態を取得
    syncWithSystemIME();

    // updateイベントでポーリング
    ofAddListener(ofEvents().update, this, &ofxIME::checkIMEState);
}

void ofxIME::stopIMEObserver() {
    ofRemoveListener(ofEvents().update, this, &ofxIME::checkIMEState);
}

void ofxIME::checkIMEState(ofEventArgs &args) {
    syncWithSystemIME();

    // IMEの未確定文字列を取得
    HWND hwnd = GetForegroundWindow();
    if (!hwnd) return;

    HIMC hIMC = ImmGetContext(hwnd);
    if (!hIMC) return;

    // 未確定文字列の長さを取得
    LONG compStrLen = ImmGetCompositionString(hIMC, GCS_COMPSTR, NULL, 0);
    if (compStrLen > 0) {
        // 未確定文字列を取得
        wchar_t* compStr = new wchar_t[compStrLen / sizeof(wchar_t) + 1];
        ImmGetCompositionStringW(hIMC, GCS_COMPSTR, compStr, compStrLen);
        compStr[compStrLen / sizeof(wchar_t)] = L'\0';

        // wstringからu32stringに変換
        u32string u32str;
        for (int i = 0; compStr[i] != L'\0'; ++i) {
            wchar_t c = compStr[i];
            // サロゲートペアの処理
            if (c >= 0xD800 && c <= 0xDBFF && compStr[i + 1] != L'\0') {
                wchar_t low = compStr[++i];
                if (low >= 0xDC00 && low <= 0xDFFF) {
                    char32_t codepoint = 0x10000 + ((c - 0xD800) << 10) + (low - 0xDC00);
                    u32str += codepoint;
                }
            } else {
                u32str += (char32_t)c;
            }
        }
        delete[] compStr;

        // カーソル位置を取得
        LONG cursorPos = ImmGetCompositionString(hIMC, GCS_CURSORPOS, NULL, 0);

        // ofxIMEに未確定文字列を渡す
        setMarkedTextFromOS(u32str, cursorPos, 0);

        // 変換候補を取得
        DWORD candListSize = ImmGetCandidateList(hIMC, 0, NULL, 0);
        if (candListSize > 0) {
            CANDIDATELIST* candList = (CANDIDATELIST*)new char[candListSize];
            if (ImmGetCandidateList(hIMC, 0, candList, candListSize) > 0) {
                vector<u32string> cands;
                for (DWORD i = 0; i < candList->dwCount && i < 9; ++i) {
                    wchar_t* candStr = (wchar_t*)((char*)candList + candList->dwOffset[i]);
                    u32string candU32;
                    for (int j = 0; candStr[j] != L'\0'; ++j) {
                        candU32 += (char32_t)candStr[j];
                    }
                    cands.push_back(candU32);
                }
                setCandidates(cands, candList->dwSelection);
            }
            delete[] (char*)candList;
        } else {
            clearCandidates();
        }
    } else {
        // 未確定文字列がない場合
        if (markedText.length() > 0) {
            // 確定された可能性があるので、確定文字列を取得
            LONG resultStrLen = ImmGetCompositionString(hIMC, GCS_RESULTSTR, NULL, 0);
            if (resultStrLen > 0) {
                wchar_t* resultStr = new wchar_t[resultStrLen / sizeof(wchar_t) + 1];
                ImmGetCompositionStringW(hIMC, GCS_RESULTSTR, resultStr, resultStrLen);
                resultStr[resultStrLen / sizeof(wchar_t)] = L'\0';

                // wstringからu32stringに変換
                u32string u32str;
                for (int i = 0; resultStr[i] != L'\0'; ++i) {
                    u32str += (char32_t)resultStr[i];
                }
                delete[] resultStr;

                // 確定文字列を挿入
                insertText(u32str);
            } else {
                // 未確定文字列をクリア
                setMarkedTextFromOS(U"", 0, 0);
            }
        }
        clearCandidates();
    }

    ImmReleaseContext(hwnd, hIMC);
}

void ofxIME::syncWithSystemIME() {
    // フォアグラウンドウィンドウのIMEコンテキストを取得
    HWND hwnd = GetForegroundWindow();
    if (!hwnd) return;

    HIMC hIMC = ImmGetContext(hwnd);
    if (!hIMC) return;

    DWORD dwConvMode = 0, dwSentMode = 0;
    ImmGetConversionStatus(hIMC, &dwConvMode, &dwSentMode);

    // 前回と変化があった場合のみ処理
    if (dwConvMode != lastIMEConversionMode) {
        lastIMEConversionMode = dwConvMode;

        // 日本語モードかどうかを判定
        bool isJapanese = (dwConvMode & IME_CMODE_NATIVE) != 0;

        if (isJapanese) {
            // 日本語モードに切り替え
            if (state == Eisu) {
                state = Kana;
            }
        } else {
            // 英数モードに切り替え
            if (state == Composing) {
                unmarkText();
            }
            state = Eisu;
        }
    }

    ImmReleaseContext(hwnd, hIMC);
}

#endif
