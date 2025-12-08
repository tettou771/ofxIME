#include "ofxIME.h"

ofxIME::ofxIME() {
    ofSetEscapeQuitsApp(false);
    state = Eisu;
    clear();
    // 静的メンバはofxIME_mac.mmで初期化済み
}

ofxIME::~ofxIME() {
    disable();
}

void ofxIME::enable() {
    if (enabled) return;

    enabled = true;
    ofAddListener(ofEvents().keyPressed, this, &ofxIME::keyPressed);
    ofAddListener(ofEvents().mousePressed, this, &ofxIME::mousePressed);

    startIMEObserver();
#ifdef __APPLE__
    setupIMEInputView();
#endif
}

void ofxIME::disable() {
    if (!enabled) return;

    enabled = false;
    ofRemoveListener(ofEvents().keyPressed, this, &ofxIME::keyPressed);
    ofRemoveListener(ofEvents().mousePressed, this, &ofxIME::mousePressed);

    stopIMEObserver();
#ifdef __APPLE__
    removeIMEInputView();
#endif
}

void ofxIME::clear() {
    markedText = U"";
    markedSelectedLocation = 0;
    markedSelectedLength = 0;
    line.clear();
    line.push_back(U"");
    candidates.clear();
    candidateSelectedIndex = 0;
    movingY = 0;
    cursorBlinkOffsetTime = ofGetElapsedTimef();
    cursorLine = cursorPos = 0;
    selectCancel();

    if (state == Composing) {
        state = Kana;
    }
}

void ofxIME::keyPressed(ofKeyEventArgs &key) {
    // Modifier key handling
#ifdef TARGET_OS_MAC
    char ctrl = OF_KEY_COMMAND;
#else
    char ctrl = OF_KEY_CONTROL;
#endif

    // Ctrl + key
    if (ofGetKeyPressed(ctrl)) {
        switch (key.key) {
        case 'c':
            // TODO: copy
            break;
        case 'v':
            // paste
            {
                string clip = ofGetClipboardString();
                u32string u32clip = UTF8toUTF32(clip);
                for (auto c : u32clip) {
                    if (c == U'\n') newLine();
                    else {
                        u32string s(1, c);
                        addStr(line[cursorLine], s, cursorPos);
                    }
                }
            }
            break;
        case 'a':
            selectAll();
            break;
        default:
            break;
        }
        return;
    }

    // If IME has marked text, let OS handle it
    if (markedText.length() > 0) {
        // During composing, let OS handle most keys
        return;
    }

    // Handle confirmed text operations
    switch (key.key) {
    case OF_KEY_ESC:
        // ESC is passed to OS IME
        break;

    case OF_KEY_BACKSPACE:
        deleteSelected();
        backspaceCharacter(line[cursorLine], cursorPos, true);
        break;

    case OF_KEY_DEL:
        deleteSelected();
        deleteCharacter(line[cursorLine], cursorPos, true);
        break;

    case OF_KEY_RETURN:
        newLine();
        break;

    case OF_KEY_UP:
        lineChange(-1);
        break;

    case OF_KEY_DOWN:
        lineChange(1);
        break;

    case OF_KEY_LEFT:
        if (cursorPos > 0) {
            cursorPos--;
        }
        break;

    case OF_KEY_RIGHT:
        cursorPos++;
        if (cursorPos > (int)line[cursorLine].length()) {
            cursorPos = (int)line[cursorLine].length();
        }
        break;

    default:
        // Normal character input comes via OS IME
        break;
    }

    // Reset cursor blink
    cursorBlinkOffsetTime = ofGetElapsedTimef();
}

void ofxIME::mousePressed(ofMouseEventArgs &mouse) {
    setCursorByMouse(mouse.x, mouse.y);
}

void ofxIME::setCursorByMouse(float x, float y) {
    ofTrueTypeFont& f = getFont();
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

string ofxIME::getString() {
    string all = "";
    for (auto &a : line) {
        all += UTF32toUTF8(a);
        if (&a != &line.back()) {
            all += '\n';
        }
    }
    return all;
}

u32string ofxIME::getU32String() {
    u32string all = U"";
    for (auto &a : line) {
        all += a;
        if (&a != &line.back()) {
            all += U'\n';
        }
    }
    return all;
}

string ofxIME::getLine(int l) {
    if (0 <= l && l < (int)line.size()) {
        return UTF32toUTF8(line[l]);
    }
    else {
        return "";
    }
}

string ofxIME::getLineSubstr(int l, int begin, int end) {
    if (0 <= l && l < (int)line.size()) {
        return UTF32toUTF8(line[l].substr(begin, end));
    }
    return "";
}

string ofxIME::getMarkedText() {
    return UTF32toUTF8(markedText);
}

string ofxIME::getMarkedTextSubstr(int begin, int end) {
    return UTF32toUTF8(markedText.substr(begin, end));
}

void ofxIME::setFont(string path, float fontSize) {
    ofTrueTypeFontSettings settings(path, fontSize);
    settings.addRanges(ofAlphabet::Latin);
    settings.addRanges(ofAlphabet::Japanese);
    settings.addRange(ofUnicode::KatakanaHalfAndFullwidthForms);
    settings.addRange(ofUnicode::range{0x3000, 0x303F}); // CJK symbols and punctuation
    font.load(settings);
    fontPtr = nullptr;  // 自前フォントを使用
}

void ofxIME::setFont(ofTrueTypeFont* sharedFont) {
    fontPtr = sharedFont;
}

void ofxIME::draw(ofPoint pos) {
    draw(pos.x, pos.y);
}

void ofxIME::draw(float x, float y) {
    ofTrueTypeFont& f = getFont();
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

// Receive confirmed text from IME
void ofxIME::insertText(const u32string &str) {
    // Clear marked text
    markedText = U"";
    markedSelectedLocation = 0;
    markedSelectedLength = 0;
    candidates.clear();
    candidateSelectedIndex = 0;

    // Process newlines
    for (auto c : str) {
        if (c == U'\n' || c == U'\r') {
            newLine();
        }
        else {
            u32string s(1, c);
            addStr(line[cursorLine], s, cursorPos);
        }
    }

    state = (state == Composing) ? Kana : state;
}

// Receive marked text from IME
void ofxIME::setMarkedTextFromOS(const u32string &str, int selectedLocation, int selectedLength) {
    markedText = str;
    markedSelectedLocation = selectedLocation;
    markedSelectedLength = selectedLength;

    if (str.length() > 0) {
        state = Composing;
    }
    else {
        state = Kana;
    }
}

// Confirm marked text
void ofxIME::unmarkText() {
    if (markedText.length() > 0) {
        // Add marked text as confirmed
        addStr(line[cursorLine], markedText, cursorPos);
        markedText = U"";
        markedSelectedLocation = 0;
        markedSelectedLength = 0;
    }
    candidates.clear();
    candidateSelectedIndex = 0;
    state = Kana;
}

// Set conversion candidates
void ofxIME::setCandidates(const vector<u32string> &cands, int selectedIndex) {
    candidates = cands;
    candidateSelectedIndex = selectedIndex;
}

void ofxIME::clearCandidates() {
    candidates.clear();
    candidateSelectedIndex = 0;
}

// Return screen coordinates for IME candidate window positioning
ofVec2f ofxIME::getMarkedTextScreenPosition() {
    ofTrueTypeFont& f = getFont();
    float x = lastDrawPos.x;
    float y = lastDrawPos.y;

    // Calculate Y coordinate for current line
    y += f.getLineHeight() * cursorLine;

    // Calculate X coordinate for cursor position
    string beforeCursor = getLineSubstr(cursorLine, 0, cursorPos);
    x += f.stringWidth(beforeCursor);

    return ofVec2f(x, y);
}

void ofxIME::deleteSelected() {
    if (!isSelected()) return;

    int bl, bn, el, en;
    tie(bl, bn) = selectBegin;
    tie(el, en) = selectEnd;

    // Swap if order is reversed
    if (bl > el || (bl == el && bn > en)) {
        tie(el, en) = selectBegin;
        tie(bl, bn) = selectEnd;
    }

    int blen = (int)line[bl].length();
    if (blen < bn) bn = blen;

    int elen = (int)line[el].length();
    if (elen < en) en = elen;

    // Delete within same line or merge lines
    line[bl] = line[bl].substr(0, bn) + line[el].substr(en, elen - en);

    // Delete intermediate lines
    int delNum = el - bl;
    for (int i = 0; i < delNum; ++i) {
        line.erase(line.begin() + bl + 1);
    }

    cursorLine = bl;
    cursorPos = bn;
    selectCancel();
}

void ofxIME::newLine() {
    // Insert new line
    line.insert(line.begin() + cursorLine + 1, U"");
    // Move text after cursor to new line
    line[cursorLine + 1] = line[cursorLine].substr(cursorPos, line[cursorLine].length() - cursorPos);
    // Truncate current line at cursor
    line[cursorLine] = line[cursorLine].substr(0, cursorPos);

    cursorLine++;
    cursorPos = 0;
}

void ofxIME::lineChange(int n) {
    if (n == 0) return;
    cursorLine = MAX(0, MIN(cursorLine + n, (int)line.size() - 1));
    if (cursorPos > (int)line[cursorLine].size()) {
        cursorPos = (int)line[cursorLine].length();
    }
}

void ofxIME::addStr(u32string &target, const u32string &str, int &p) {
    // Insert at cursor position
    target = target.substr(0, p) + str + target.substr(p, target.length() - p);

    // Move cursor
    p += str.length();
}

void ofxIME::backspaceCharacter(u32string &str, int &pos, bool lineMerge) {
    // If cursor at beginning, merge with previous line
    if (pos == 0) {
        if (lineMerge && cursorLine > 0) {
            cursorPos = (int)line[cursorLine - 1].length();
            line[cursorLine - 1] += line[cursorLine];
            line.erase(line.begin() + cursorLine);
            cursorLine--;
        }
    }
    // Delete character before cursor
    else {
        if ((int)str.length() < pos) pos = (int)str.length();

        // Delete character at cursor position
        str = str.substr(0, pos - 1) + str.substr(pos, str.length() - pos);

        // Move cursor back
        pos--;
    }
}

void ofxIME::deleteCharacter(u32string &str, int &pos, bool lineMerge) {
    if ((int)str.length() < pos) {
        pos = (int)str.length();
    }
    if ((int)str.length() == pos) {
        // If cursor at end, merge with next line
        if (lineMerge && cursorLine + 1 < (int)line.size()) {
            line[cursorLine] += line[cursorLine + 1];
            line.erase(line.begin() + cursorLine + 1);
        }
    }
    else {
        // Delete character at cursor position
        str = str.substr(0, pos) + str.substr(pos + 1, str.length() - pos - 1);
    }
}

#ifdef WIN32
string ofxIME::UTF32toSjis(u32string srcu32str) {
    string str = UTF32toUTF8(srcu32str);

    wstring_convert<codecvt_utf8<wchar_t>, wchar_t> cv;
    wstring wstr = cv.from_bytes(str);

    static_assert(sizeof(wchar_t) == 2, "this function is windows only");
    const int len = WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    string re(len * 2, '\0');
    if (!WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), -1, &re[0], len, nullptr, nullptr)) {
        const auto ec = GetLastError();
        switch (ec) {
        case ERROR_INSUFFICIENT_BUFFER:
            throw runtime_error("WideCharToMultiByte fail: ERROR_INSUFFICIENT_BUFFER");
        case ERROR_INVALID_FLAGS:
            throw runtime_error("WideCharToMultiByte fail: ERROR_INVALID_FLAGS");
        case ERROR_INVALID_PARAMETER:
            throw runtime_error("WideCharToMultiByte fail: ERROR_INVALID_PARAMETER");
        default:
            throw runtime_error("WideCharToMultiByte fail: unknown(" + to_string(ec) + ')');
        }
    }
    const size_t real_len = strlen(re.c_str());
    re.resize(real_len);
    re.shrink_to_fit();
    return re;
}
#endif

// UTF-32 to UTF-8 conversion
string ofxIME::UTF32toUTF8(const u32string &u32str) {
    string result;
    for (char32_t c : u32str) {
        if (c < 0x80) {
            result += static_cast<char>(c);
        }
        else if (c < 0x800) {
            result += static_cast<char>(0xC0 | (c >> 6));
            result += static_cast<char>(0x80 | (c & 0x3F));
        }
        else if (c < 0x10000) {
            result += static_cast<char>(0xE0 | (c >> 12));
            result += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
            result += static_cast<char>(0x80 | (c & 0x3F));
        }
        else {
            result += static_cast<char>(0xF0 | (c >> 18));
            result += static_cast<char>(0x80 | ((c >> 12) & 0x3F));
            result += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
            result += static_cast<char>(0x80 | (c & 0x3F));
        }
    }
    return result;
}

string ofxIME::UTF32toUTF8(const char32_t &u32char) {
    return UTF32toUTF8(u32string(1, u32char));
}

// UTF-8 to UTF-32 conversion
u32string ofxIME::UTF8toUTF32(const string &str) {
    u32string result;
    size_t i = 0;
    while (i < str.size()) {
        unsigned char c = str[i];
        char32_t cp;
        if ((c & 0x80) == 0) {
            cp = c;
            i += 1;
        }
        else if ((c & 0xE0) == 0xC0) {
            cp = (c & 0x1F) << 6;
            cp |= (str[i + 1] & 0x3F);
            i += 2;
        }
        else if ((c & 0xF0) == 0xE0) {
            cp = (c & 0x0F) << 12;
            cp |= (str[i + 1] & 0x3F) << 6;
            cp |= (str[i + 2] & 0x3F);
            i += 3;
        }
        else {
            cp = (c & 0x07) << 18;
            cp |= (str[i + 1] & 0x3F) << 12;
            cp |= (str[i + 2] & 0x3F) << 6;
            cp |= (str[i + 3] & 0x3F);
            i += 4;
        }
        result += cp;
    }
    return result;
}
