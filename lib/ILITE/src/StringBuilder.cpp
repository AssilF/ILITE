#include "StringBuilder.h"

#include "ScreenRegistry.h"
#include "DisplayCanvas.h"
#include "AudioRegistry.h"
#include "InputManager.h"
#include "IconLibrary.h"

#include <vector>
#include <string>
#include <cctype>
#include <cstring>
#include <algorithm>

namespace {

enum class KeyAction {
    Character,
    Space,
    Backspace,
    Enter,
    Cancel,
    Shift,
    ToggleLayout
};

struct KeyDef {
    std::string label;
    KeyAction action = KeyAction::Character;
    char value = '\0';
    uint8_t width = 12;
};

using KeyRow = std::vector<KeyDef>;

KeyDef makeKey(const char* label, KeyAction action, char value = '\0', uint8_t width = 12) {
    KeyDef key;
    key.label = label ? label : "";
    key.action = action;
    key.value = value;
    key.width = width;
    return key;
}

struct BuilderState {
    bool active = false;
    bool uppercase = true;
    bool symbols = false;
    size_t maxLength = 32;
    std::string title;
    std::string subtitle;
    std::string text;
    size_t cursorIndex = 0;
    size_t selectedRow = 0;
    size_t selectedCol = 0;
    uint32_t lastJoystickMoveMs = 0;
    std::function<void(const char*)> onSubmit;
    std::function<void()> onCancel;
};

BuilderState g_state;
uint32_t g_cursorBlink = 0;

KeyRow commandRow() {
    KeyRow row;
    row.push_back(makeKey("Shift", KeyAction::Shift, 0, 18));
    row.push_back(makeKey("Sym", KeyAction::ToggleLayout, 0, 18));
    row.push_back(makeKey("Space", KeyAction::Space, 0, 36));
    row.push_back(makeKey("Bksp", KeyAction::Backspace, 0, 20));
    row.push_back(makeKey("Enter", KeyAction::Enter, 0, 20));
    row.push_back(makeKey("Exit", KeyAction::Cancel, 0, 18));
    return row;
}

KeyRow makeCharRow(const char* chars) {
    KeyRow row;
    for (const char* p = chars; *p; ++p) {
        KeyDef key;
        key.label.assign(1, *p);
        key.action = KeyAction::Character;
        key.value = *p;
        key.width = 12;
        row.push_back(key);
    }
    return row;
}

const std::vector<KeyRow>& letterLayout() {
    static std::vector<KeyRow> layout = [] {
        std::vector<KeyRow> rows;
        rows.push_back(makeCharRow("qwertyuiop"));
        rows.push_back(makeCharRow("asdfghjkl"));
        rows.push_back(makeCharRow("zxcvbnm"));
        rows.push_back(commandRow());
        return rows;
    }();
    return layout;
}

const std::vector<KeyRow>& symbolLayout() {
    static std::vector<KeyRow> layout = [] {
        std::vector<KeyRow> rows;
        rows.push_back(makeCharRow("1234567890"));
        rows.push_back(makeCharRow("!@#$%^&*?"));
        rows.push_back(makeCharRow("-_=+/:.;,"));
        rows.push_back(commandRow());
        return rows;
    }();
    return layout;
}

const std::vector<KeyRow>& activeLayout() {
    return g_state.symbols ? symbolLayout() : letterLayout();
}

void clampCursor() {
    if (g_state.cursorIndex > g_state.text.size()) {
        g_state.cursorIndex = g_state.text.size();
    }
}

void applyKey(KeyAction action, char value);

void ensureSelectionInBounds() {
    const auto& rows = activeLayout();
    if (rows.empty()) {
        g_state.selectedRow = 0;
        g_state.selectedCol = 0;
        return;
    }
    if (g_state.selectedRow >= rows.size()) {
        g_state.selectedRow = rows.size() - 1;
    }
    const KeyRow& row = rows[g_state.selectedRow];
    if (row.empty()) {
        g_state.selectedCol = 0;
        return;
    }
    if (g_state.selectedCol >= row.size()) {
        g_state.selectedCol = row.size() - 1;
    }
}

void moveSelection(int dx, int dy) {
    const auto& rows = activeLayout();
    if (rows.empty()) {
        return;
    }

    int row = static_cast<int>(g_state.selectedRow);
    int col = static_cast<int>(g_state.selectedCol);

    if (dy != 0) {
        row += dy;
        row = std::max(0, std::min(row, static_cast<int>(rows.size()) - 1));
        col = std::min(col, static_cast<int>(rows[row].size()) - 1);
    }

    if (dx != 0) {
        const int rowLen = static_cast<int>(rows[row].size());
        if (rowLen > 0) {
            col = (col + dx) % rowLen;
            if (col < 0) col += rowLen;
        } else {
            col = 0;
        }
    }

    g_state.selectedRow = static_cast<size_t>(row);
    g_state.selectedCol = static_cast<size_t>(std::max(col, 0));
}

void applyKey(KeyAction action, char value) {
    switch (action) {
    case KeyAction::Enter:
        if (g_state.onSubmit) {
            g_state.onSubmit(g_state.text.c_str());
        }
        g_state.active = false;
        ScreenRegistry::back();
        AudioRegistry::play("edit_save");
        return;

    case KeyAction::Cancel:
        if (g_state.onCancel) {
            g_state.onCancel();
        }
        g_state.active = false;
        ScreenRegistry::back();
        AudioRegistry::play("edit_cancel");
        return;

    case KeyAction::Backspace:
        if (g_state.cursorIndex > 0 && !g_state.text.empty()) {
            g_state.text.erase(g_state.cursorIndex - 1, 1);
            g_state.cursorIndex--;
            AudioRegistry::play("menu_back");
        }
        return;

    case KeyAction::Space:
        if (g_state.text.size() < g_state.maxLength) {
            g_state.text.insert(g_state.cursorIndex, 1, ' ');
            g_state.cursorIndex++;
            AudioRegistry::play("menu_select");
        }
        return;

    case KeyAction::Shift:
        g_state.uppercase = !g_state.uppercase;
        AudioRegistry::play("toggle");
        return;

    case KeyAction::ToggleLayout:
        g_state.symbols = !g_state.symbols;
        g_state.selectedRow = 0;
        g_state.selectedCol = 0;
        AudioRegistry::play("toggle");
        return;

    case KeyAction::Character: {
        if (g_state.text.size() >= g_state.maxLength) {
            return;
        }
        char ch = value;
        if (!g_state.symbols) {
            ch = g_state.uppercase ? static_cast<char>(toupper(ch))
                                   : static_cast<char>(tolower(ch));
        }
        g_state.text.insert(g_state.cursorIndex, 1, ch);
        g_state.cursorIndex++;
        AudioRegistry::play("menu_select");
        return;
    }
    }
}

void drawKey(DisplayCanvas& canvas, const KeyDef& key, int16_t x, int16_t y, bool selected) {
    const int16_t keyHeight = 12;
    const int16_t width = key.width ? key.width : 12;

    canvas.drawRect(x, y, width, keyHeight, false);
    if (selected) {
        canvas.drawRect(x + 1, y + 1, width - 2, keyHeight - 2, true);
        canvas.setDrawColor(0);
    }

    canvas.setFont(DisplayCanvas::TINY);
    canvas.drawText(x + 2, y + 8, key.label.c_str());

    if (selected) {
        canvas.setDrawColor(1);
    }
}

void drawTextArea(DisplayCanvas& canvas) {
    canvas.setFont(DisplayCanvas::SMALL);
    int16_t boxX = 4;
    int16_t boxY = 18;
    int16_t boxW = 120;
    int16_t boxH = 16;
    canvas.drawRect(boxX, boxY, boxW, boxH, false);

    std::string preview = g_state.text;
    if (preview.size() > g_state.maxLength) {
        preview.resize(g_state.maxLength);
    }

    size_t maxChars = static_cast<size_t>(boxW / 6);
    size_t start = 0;
    if (preview.size() > maxChars) {
        if (g_state.cursorIndex > maxChars) {
            start = g_state.cursorIndex - maxChars;
        }
        preview = preview.substr(start, maxChars);
    }

    canvas.drawText(boxX + 2, boxY + 12, preview.c_str());

    bool cursorVisible = ((millis() - g_cursorBlink) % 1000) < 500;
    if (cursorVisible) {
        size_t cursorPos = g_state.cursorIndex >= start ? g_state.cursorIndex - start : 0;
        if (cursorPos > preview.size()) {
            cursorPos = preview.size();
        }
        int16_t cursorX = boxX + 2 + static_cast<int16_t>(cursorPos * 6);
        canvas.drawVLine(cursorX, boxY + 4, boxH - 8);
    }
}

void drawKeyboardScreen(DisplayCanvas& canvas) {
    canvas.clear();

    canvas.setFont(DisplayCanvas::SMALL);
    const char* title = g_state.title.empty() ? "Text Input" : g_state.title.c_str();
    canvas.drawTextCentered(8, title);

    if (!g_state.subtitle.empty()) {
        canvas.setFont(DisplayCanvas::TINY);
        canvas.drawTextCentered(16, g_state.subtitle.c_str());
    }

    canvas.setFont(DisplayCanvas::TINY);
    canvas.drawText(100, 10, g_state.symbols ? "SYM" : (g_state.uppercase ? "ABC" : "abc"));

    drawTextArea(canvas);

    const auto& layout = activeLayout();
    const int16_t startX = 4;
    int16_t y = 40;
    const int16_t keyHeight = 12;
    const int16_t rowSpacing = 2;

    for (size_t rowIndex = 0; rowIndex < layout.size(); ++rowIndex) {
        const KeyRow& row = layout[rowIndex];
        int16_t x = startX;
        for (size_t col = 0; col < row.size(); ++col) {
            bool selected = (rowIndex == g_state.selectedRow && col == g_state.selectedCol);
            drawKey(canvas, row[col], x, y, selected);
            x += (row[col].width ? row[col].width : 12) + 2;
        }
        y += keyHeight + rowSpacing;
    }
}

void handleJoystickNavigation() {
    InputManager& inputs = InputManager::getInstance();
    const float threshold = 0.5f;
    uint32_t now = millis();

    if (now - g_state.lastJoystickMoveMs < 160) {
        return;
    }

    float x = inputs.getJoystickA_X();
    float y = inputs.getJoystickA_Y();
    bool moved = false;

    if (x > threshold) {
        moveSelection(1, 0);
        moved = true;
    } else if (x < -threshold) {
        moveSelection(-1, 0);
        moved = true;
    }

    if (y > threshold) {
        moveSelection(0, -1);
        moved = true;
    } else if (y < -threshold) {
        moveSelection(0, 1);
        moved = true;
    }

    if (moved) {
        g_state.lastJoystickMoveMs = now;
    }

    if (inputs.getJoystickButtonA_Pressed() || inputs.getJoystickButtonB_Pressed()) {
        const auto& rows = activeLayout();
        if (!rows.empty() && g_state.selectedRow < rows.size() &&
            g_state.selectedCol < rows[g_state.selectedRow].size()) {
            const KeyDef& key = rows[g_state.selectedRow][g_state.selectedCol];
            applyKey(key.action, key.value);
        }
    }
}

void updateKeyboard() {
    handleJoystickNavigation();
}

void onEncoderRotate(int delta) {
    if (delta > 0) {
        for (int i = 0; i < delta; ++i) moveSelection(1, 0);
    } else {
        for (int i = 0; i < -delta; ++i) moveSelection(-1, 0);
    }
}

void onEncoderPress() {
    const auto& rows = activeLayout();
    if (rows.empty()) return;
    if (g_state.selectedRow >= rows.size()) return;
    const KeyRow& row = rows[g_state.selectedRow];
    if (row.empty() || g_state.selectedCol >= row.size()) return;
    applyKey(row[g_state.selectedCol].action, row[g_state.selectedCol].value);
}

void onButton(uint8_t index) {
    switch (index) {
        case 0: applyKey(KeyAction::Backspace, 0); break;
        case 1: applyKey(KeyAction::Shift, 0); break;
        case 2: applyKey(KeyAction::Enter, 0); break;
    }
}

void registerScreen() {
    static bool registered = false;
    if (registered) {
        return;
    }

    Screen screen;
    screen.id = "framework.string_builder";
    screen.title = "Keyboard";
    screen.icon = ICON_TUNING;
    screen.drawFunc = [](DisplayCanvas& canvas) {
        drawKeyboardScreen(canvas);
    };
    screen.updateFunc = []() { updateKeyboard(); };
    screen.onEncoderRotate = [](int delta) { onEncoderRotate(delta); };
    screen.onEncoderPress = []() { onEncoderPress(); };
    screen.onButton1 = []() { onButton(0); };
    screen.onButton2 = []() { onButton(1); };
    screen.onButton3 = []() { onButton(2); };
    screen.isModal = false;
    screen.onShow = []() {
        g_cursorBlink = millis();
        g_state.lastJoystickMoveMs = 0;
    };
    ScreenRegistry::registerScreen(screen);
    registered = true;
}

const KeyDef* currentKey() {
    const auto& rows = activeLayout();
    if (rows.empty() || g_state.selectedRow >= rows.size()) {
        return nullptr;
    }
    const KeyRow& row = rows[g_state.selectedRow];
    if (row.empty() || g_state.selectedCol >= row.size()) {
        return nullptr;
    }
    return &row[g_state.selectedCol];
}

} // namespace

bool StringBuilder::begin(const StringBuilderConfig& config) {
    if (g_state.active) {
        return false;
    }
    registerScreen();

    g_state.active = true;
    g_state.uppercase = true;
    g_state.symbols = false;
    g_state.maxLength = config.maxLength > 0 ? config.maxLength : 1;
    g_state.title = config.title ? config.title : "Text Input";
    g_state.subtitle = config.subtitle ? config.subtitle : "";
    g_state.onSubmit = config.onSubmit;
    g_state.onCancel = config.onCancel;
    g_state.text = config.initialValue ? config.initialValue : "";
    if (g_state.text.size() > g_state.maxLength) {
        g_state.text.resize(g_state.maxLength);
    }
    g_state.cursorIndex = g_state.text.size();
    g_state.selectedRow = 0;
    g_state.selectedCol = 0;
    g_cursorBlink = millis();
    g_state.lastJoystickMoveMs = 0;

    ScreenRegistry::show("framework.string_builder");
    return true;
}

bool StringBuilder::isActive() {
    return g_state.active;
}

void StringBuilder::cancel() {
    if (!g_state.active) {
        return;
    }
    g_state.active = false;
    if (g_state.onCancel) {
        g_state.onCancel();
    }
    ScreenRegistry::back();
}

const char* StringBuilder::getText() {
    return g_state.text.c_str();
}
