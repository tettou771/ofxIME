#include "ofxIME.h"

void ofxIMEBase::enable() {
    if (enabled) return;

    enabled = true;
    ofAddListener(ofEvents().keyPressed, this, &ofxIMEBase::keyPressed);

    startIMEObserver();
#ifdef __APPLE__
    setupIMEInputView();
#endif
}

void ofxIMEBase::disable() {
    if (!enabled) return;

    enabled = false;
    ofRemoveListener(ofEvents().keyPressed, this, &ofxIMEBase::keyPressed);

    stopIMEObserver();
#ifdef __APPLE__
    removeIMEInputView();
#endif
}

void ofxIMEBase::clear() {
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

void ofxIMEBase::keyPressed(ofKeyEventArgs &key) {
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

string ofxIMEBase::getString() {
    string all = "";
    for (auto &a : line) {
        all += UTF32toUTF8(a);
        if (&a != &line.back()) {
            all += '\n';
        }
    }
    return all;
}

void ofxIMEBase::setString(const string &str) {
    clear();
    u32string u32str = UTF8toUTF32(str);
    insertText(u32str);
}

u32string ofxIMEBase::getU32String() {
    u32string all = U"";
    for (auto &a : line) {
        all += a;
        if (&a != &line.back()) {
            all += U'\n';
        }
    }
    return all;
}

string ofxIMEBase::getLine(int l) {
    if (0 <= l && l < (int)line.size()) {
        return UTF32toUTF8(line[l]);
    }
    else {
        return "";
    }
}

string ofxIMEBase::getLineSubstr(int l, int begin, int end) {
    if (0 <= l && l < (int)line.size()) {
        return UTF32toUTF8(line[l].substr(begin, end));
    }
    return "";
}

string ofxIMEBase::getMarkedText() {
    return UTF32toUTF8(markedText);
}

string ofxIMEBase::getMarkedTextSubstr(int begin, int end) {
    return UTF32toUTF8(markedText.substr(begin, end));
}

// Receive confirmed text from IME
void ofxIMEBase::insertText(const u32string &str) {
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
void ofxIMEBase::setMarkedTextFromOS(const u32string &str, int selectedLocation, int selectedLength) {
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
void ofxIMEBase::unmarkText() {
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
void ofxIMEBase::setCandidates(const vector<u32string> &cands, int selectedIndex) {
    candidates = cands;
    candidateSelectedIndex = selectedIndex;
}

void ofxIMEBase::clearCandidates() {
    candidates.clear();
    candidateSelectedIndex = 0;
}

void ofxIMEBase::deleteSelected() {
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

void ofxIMEBase::newLine() {
    // Insert new line
    line.insert(line.begin() + cursorLine + 1, U"");
    // Move text after cursor to new line
    line[cursorLine + 1] = line[cursorLine].substr(cursorPos, line[cursorLine].length() - cursorPos);
    // Truncate current line at cursor
    line[cursorLine] = line[cursorLine].substr(0, cursorPos);

    cursorLine++;
    cursorPos = 0;
}

void ofxIMEBase::lineChange(int n) {
    if (n == 0) return;
    cursorLine = MAX(0, MIN(cursorLine + n, (int)line.size() - 1));
    if (cursorPos > (int)line[cursorLine].size()) {
        cursorPos = (int)line[cursorLine].length();
    }
}

void ofxIMEBase::addStr(u32string &target, const u32string &str, int &p) {
    // Insert at cursor position
    target = target.substr(0, p) + str + target.substr(p, target.length() - p);

    // Move cursor
    p += str.length();
}

void ofxIMEBase::backspaceCharacter(u32string &str, int &pos, bool lineMerge) {
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

void ofxIMEBase::deleteCharacter(u32string &str, int &pos, bool lineMerge) {
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
string ofxIMEBase::UTF32toSjis(u32string srcu32str) {
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
string ofxIMEBase::UTF32toUTF8(const u32string &u32str) {
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

string ofxIMEBase::UTF32toUTF8(const char32_t &u32char) {
    return UTF32toUTF8(u32string(1, u32char));
}

// UTF-8 to UTF-32 conversion
u32string ofxIMEBase::UTF8toUTF32(const string &str) {
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
