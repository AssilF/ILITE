/**
 * @file DisplayCanvas.cpp
 * @brief Display Canvas Implementation
 */

#include "DisplayCanvas.h"
#include "IconLibrary.h"
#include <cstdarg>
#include <cstdio>
#include <cmath>

// Static instance pointer
DisplayCanvas* DisplayCanvas::instance_ = nullptr;

// ============================================================================
// Constructor
// ============================================================================

DisplayCanvas::DisplayCanvas(U8G2& u8g2)
    : u8g2_(u8g2),
      currentFont_(NORMAL)
{
    instance_ = this;
}

DisplayCanvas& DisplayCanvas::getInstance() {
    return *instance_;
}

// ============================================================================
// Display Management
// ============================================================================

void DisplayCanvas::clear() {
    u8g2_.clearBuffer();
}

void DisplayCanvas::sendBuffer() {
    u8g2_.sendBuffer();
}

void DisplayCanvas::setDrawColor(uint8_t color) {
    u8g2_.setDrawColor(color);
}

uint8_t DisplayCanvas::getDrawColor() const {
    return u8g2_.getDrawColor();
}

// ============================================================================
// Basic Shapes
// ============================================================================

void DisplayCanvas::drawPixel(int16_t x, int16_t y) {
    u8g2_.drawPixel(x, y);
}

void DisplayCanvas::drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
    u8g2_.drawLine(x0, y0, x1, y1);
}

void DisplayCanvas::drawHLine(int16_t x, int16_t y, int16_t w) {
    u8g2_.drawHLine(x, y, w);
}

void DisplayCanvas::drawVLine(int16_t x, int16_t y, int16_t h) {
    u8g2_.drawVLine(x, y, h);
}

void DisplayCanvas::drawRect(int16_t x, int16_t y, int16_t w, int16_t h, bool filled) {
    if (filled) {
        u8g2_.drawBox(x, y, w, h);
    } else {
        u8g2_.drawFrame(x, y, w, h);
    }
}

void DisplayCanvas::drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, bool filled) {
    if (filled) {
        u8g2_.drawRBox(x, y, w, h, r);
    } else {
        u8g2_.drawRFrame(x, y, w, h, r);
    }
}

void DisplayCanvas::drawCircle(int16_t x, int16_t y, int16_t r, bool filled) {
    if (filled) {
        u8g2_.drawDisc(x, y, r);
    } else {
        u8g2_.drawCircle(x, y, r);
    }
}

void DisplayCanvas::drawEllipse(int16_t x, int16_t y, int16_t rx, int16_t ry, bool filled) {
    if (filled) {
        u8g2_.drawFilledEllipse(x, y, rx, ry);
    } else {
        u8g2_.drawEllipse(x, y, rx, ry);
    }
}

void DisplayCanvas::drawTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, bool filled) {
    if (filled) {
        // U8G2 doesn't have filled triangle, so draw outline and fill
        u8g2_.drawTriangle(x0, y0, x1, y1, x2, y2);
        // Simple scanline fill (basic implementation)
        // For production, use proper triangle filling algorithm
    } else {
        u8g2_.drawTriangle(x0, y0, x1, y1, x2, y2);
    }
}

// ============================================================================
// Text
// ============================================================================

void DisplayCanvas::setFont(Font font) {
    currentFont_ = font;
    u8g2_.setFont(getFontPointer(font));
}

DisplayCanvas::Font DisplayCanvas::getFont() const {
    return currentFont_;
}

void DisplayCanvas::drawText(int16_t x, int16_t y, const char* text) {
    if (text) {
        u8g2_.setCursor(x, y);
        u8g2_.print(text);
    }
}

void DisplayCanvas::drawTextF(int16_t x, int16_t y, const char* fmt, ...) {
    if (!fmt) return;

    char buffer[128];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    drawText(x, y, buffer);
}

int16_t DisplayCanvas::getTextWidth(const char* text) const {
    return text ? u8g2_.getUTF8Width(text) : 0;
}

int16_t DisplayCanvas::getTextHeight() const {
    return u8g2_.getMaxCharHeight();
}

void DisplayCanvas::drawTextCentered(int16_t y, const char* text) {
    if (!text) return;
    int16_t w = getTextWidth(text);
    int16_t x = (getWidth() - w) / 2;
    drawText(x, y, text);
}

void DisplayCanvas::drawTextRight(int16_t x, int16_t y, const char* text) {
    if (!text) return;
    int16_t w = getTextWidth(text);
    drawText(x - w, y, text);
}

// ============================================================================
// Icons
// ============================================================================

void DisplayCanvas::drawIcon(int16_t x, int16_t y, uint16_t iconCode) {
    setFont(ICON_SMALL);
    u8g2_.setCursor(x, y);
    u8g2_.print(static_cast<char>(iconCode));
}

bool DisplayCanvas::drawIconByID(int16_t x, int16_t y, const char* iconId) {
    // Look up icon in IconLibrary
    const Icon* icon = IconLibrary::getIcon(iconId);
    if (icon == nullptr) {
        return false;
    }

    // Draw bitmap using U8G2's XBM function
    u8g2_.drawXBM(x, y, icon->width, icon->height, icon->data);
    return true;
}

// ============================================================================
// Widgets
// ============================================================================

void DisplayCanvas::drawProgressBar(int16_t x, int16_t y, int16_t w, int16_t h, float value, const char* label) {
    value = constrain(value, 0.0f, 1.0f);

    // Draw label above bar if provided
    if (label) {
        setFont(TINY);
        drawText(x, y - 2, label);
        y += 8;  // Offset bar down
    }

    // Draw frame
    drawRect(x, y, w, h, false);

    // Draw fill
    int16_t fillWidth = static_cast<int16_t>((w - 2) * value);
    if (fillWidth > 0) {
        drawRect(x + 1, y + 1, fillWidth, h - 2, true);
    }

    // Draw percentage text if bar is tall enough
    if (h >= 8) {
        setFont(TINY);
        char pctText[8];
        snprintf(pctText, sizeof(pctText), "%d%%", static_cast<int>(value * 100));
        int16_t textW = getTextWidth(pctText);
        int16_t textX = x + (w - textW) / 2;
        int16_t textY = y + h / 2 + 3;

        // XOR text so it's visible on both filled and empty parts
        setDrawColor(2);  // XOR mode
        drawText(textX, textY, pctText);
        setDrawColor(1);  // Back to normal
    }
}

void DisplayCanvas::drawGauge(int16_t x, int16_t y, int16_t r, float value, float min, float max, const char* label) {
    value = constrain(value, min, max);
    float normalized = (value - min) / (max - min);

    // Draw arc (240 degree range, -120 to +120)
    const float startAngle = -120.0f * PI / 180.0f;
    const float endAngle = 120.0f * PI / 180.0f;
    const float angleRange = endAngle - startAngle;

    // Draw arc outline (simplified - draw circle for now)
    drawCircle(x, y, r, false);

    // Draw needle
    float needleAngle = startAngle + angleRange * normalized;
    int16_t needleX = x + static_cast<int16_t>(r * 0.8f * cos(needleAngle));
    int16_t needleY = y + static_cast<int16_t>(r * 0.8f * sin(needleAngle));
    drawLine(x, y, needleX, needleY);

    // Draw center label if provided
    if (label) {
        setFont(TINY);
        int16_t textW = getTextWidth(label);
        drawText(x - textW / 2, y + r + 8, label);
    }
}

void DisplayCanvas::drawGraph(int16_t x, int16_t y, int16_t w, int16_t h, const float* values, size_t count, float min, float max) {
    if (!values || count == 0 || max == min) return;

    // Draw frame
    drawRect(x, y, w, h, false);

    // Plot values
    int16_t prevPlotX = x;
    int16_t prevPlotY = y + h - 1;

    for (size_t i = 0; i < count; ++i) {
        float value = constrain(values[i], min, max);
        float normalized = (value - min) / (max - min);

        int16_t plotX = x + 1 + (w - 2) * i / (count - 1);
        int16_t plotY = y + h - 1 - static_cast<int16_t>((h - 2) * normalized);

        if (i > 0) {
            drawLine(prevPlotX, prevPlotY, plotX, plotY);
        }

        prevPlotX = plotX;
        prevPlotY = plotY;
    }
}

void DisplayCanvas::drawJoystickViz(int16_t x, int16_t y, int16_t r, float joyX, float joyY) {
    joyX = constrain(joyX, -1.0f, 1.0f);
    joyY = constrain(joyY, -1.0f, 1.0f);

    // Draw outer circle
    drawCircle(x, y, r, false);

    // Draw crosshair at center
    drawLine(x - 3, y, x + 3, y);
    drawLine(x, y - 3, x, y + 3);

    // Draw joystick position
    int16_t joyPosX = x + static_cast<int16_t>(joyX * r * 0.8f);
    int16_t joyPosY = y - static_cast<int16_t>(joyY * r * 0.8f);  // Invert Y
    drawCircle(joyPosX, joyPosY, 2, true);
}

void DisplayCanvas::drawBatteryIcon(int16_t x, int16_t y, uint8_t percent) {
    percent = constrain(percent, 0, 100);

    const int16_t w = 20;
    const int16_t h = 10;
    const int16_t tipW = 2;
    const int16_t tipH = 4;

    // Draw main body
    drawRect(x, y, w, h, false);

    // Draw positive terminal
    drawRect(x + w, y + (h - tipH) / 2, tipW, tipH, true);

    // Draw fill level
    int16_t fillW = (w - 4) * percent / 100;
    if (fillW > 0) {
        drawRect(x + 2, y + 2, fillW, h - 4, true);
    }

    // Draw percentage text
    setFont(TINY);
    char pctText[8];
    snprintf(pctText, sizeof(pctText), "%d%%", percent);
    drawText(x + w + tipW + 2, y + 7, pctText);
}

void DisplayCanvas::drawSignalStrength(int16_t x, int16_t y, uint8_t strength) {
    strength = constrain(strength, 0, 4);

    const int16_t barWidth = 3;
    const int16_t barSpacing = 2;
    const int16_t maxHeight = 10;

    for (uint8_t i = 0; i < 4; ++i) {
        int16_t barHeight = (i + 1) * maxHeight / 4;
        int16_t barX = x + i * (barWidth + barSpacing);
        int16_t barY = y + maxHeight - barHeight;

        bool filled = i < strength;
        drawRect(barX, barY, barWidth, barHeight, filled);
    }
}

// ============================================================================
// Layout Helpers
// ============================================================================

void DisplayCanvas::drawHeader(const char* title) {
    if (!title) return;

    setFont(NORMAL);
    drawText(2, 10, title);
    drawHLine(0, 12, getWidth());
}

void DisplayCanvas::drawStatusBar(bool connected, uint8_t battery, uint32_t txCount, uint32_t rxCount) {
    setFont(TINY);

    // Connection indicator
    if (connected) {
        drawCircle(2, 2, 2, true);
    } else {
        drawCircle(2, 2, 2, false);
    }

    // Battery
    char battText[8];
    snprintf(battText, sizeof(battText), "%d%%", battery);
    drawText(8, 6, battText);

    // TX/RX counts
    char txrxText[32];
    snprintf(txrxText, sizeof(txrxText), "TX:%lu RX:%lu", txCount, rxCount);
    drawTextRight(getWidth() - 2, 6, txrxText);

    // Separator line
    drawHLine(0, 8, getWidth());
}

void DisplayCanvas::drawFooter(const char* leftText, const char* centerText, const char* rightText) {
    int16_t footerY = getHeight() - 1;
    int16_t lineY = getHeight() - 8;

    // Separator line
    drawHLine(0, lineY, getWidth());

    setFont(TINY);

    if (leftText) {
        drawText(2, footerY, leftText);
    }

    if (centerText) {
        drawTextCentered(footerY, centerText);
    }

    if (rightText) {
        drawTextRight(getWidth() - 2, footerY, rightText);
    }
}

// ============================================================================
// Advanced
// ============================================================================

void DisplayCanvas::setClipRect(int16_t x, int16_t y, int16_t w, int16_t h) {
    u8g2_.setClipWindow(x, y, x + w - 1, y + h - 1);
}

void DisplayCanvas::clearClipRect() {
    u8g2_.setMaxClipWindow();
}

int16_t DisplayCanvas::getWidth() const {
    return u8g2_.getDisplayWidth();
}

int16_t DisplayCanvas::getHeight() const {
    return u8g2_.getDisplayHeight();
}

U8G2& DisplayCanvas::getU8G2() {
    return u8g2_;
}

// ============================================================================
// Font Mapping
// ============================================================================

const uint8_t* DisplayCanvas::getFontPointer(Font font) const {
    switch (font) {
        case TINY:
            return u8g2_font_4x6_tf;
        case SMALL:
            return u8g2_font_5x8_tf;
        case NORMAL:
            return u8g2_font_torussansbold8_8r;
        case LARGE:
            return u8g2_font_inb38_mr;
        case ICON_SMALL:
            return u8g2_font_open_iconic_all_1x_t;
        case ICON_LARGE:
            return u8g2_font_open_iconic_all_2x_t;
        default:
            return u8g2_font_5x8_tf;
    }
}
