/**
 * @file DisplayCanvas.h
 * @brief ILITE Framework - Display Canvas API
 *
 * High-level drawing API abstracting U8G2 library. Provides:
 * - Basic shapes (lines, rectangles, circles, triangles)
 * - Text rendering with multiple fonts
 * - Icon library
 * - Widget library (progress bars, gauges, graphs)
 * - Layout helpers (headers, footers, status bars)
 *
 * ## Design Goals
 * - Hide U8G2 complexity from module developers
 * - Provide common UI patterns as widgets
 * - Allow future display hardware changes without breaking modules
 * - Thread-safe rendering (framebuffer-based)
 *
 * @author ILITE Team
 * @date 2025
 */

#pragma once
#include <Arduino.h>
#include <U8g2lib.h>

/**
 * @brief High-level drawing canvas for OLED display
 *
 * Wraps U8G2 library with cleaner API and adds widget library.
 * All drawing happens to an internal framebuffer, then rendered
 * to hardware via sendBuffer().
 *
 * ## Usage Example
 * ```cpp
 * void drawDashboard(DisplayCanvas& canvas) {
 *     canvas.clear();
 *     canvas.drawHeader("My Robot");
 *
 *     canvas.setFont(DisplayCanvas::NORMAL);
 *     canvas.drawText(10, 20, "Battery:");
 *     canvas.drawProgressBar(10, 25, 100, 8, batteryPercent);
 *
 *     canvas.drawGauge(64, 45, 15, speed, 0, 100, "km/h");
 * }
 * ```
 *
 * Coordinates are in pixels, origin (0,0) at top-left.
 */
class DisplayCanvas {
public:
    // ========================================================================
    // Font Enumeration
    // ========================================================================

    /**
     * @brief Available font sizes
     */
    enum Font {
        TINY,       ///< 4x6 - Very small, for dense information
        SMALL,      ///< 5x8 - Small, for secondary text
        NORMAL,     ///< 8x8 - Default, readable size
        LARGE,      ///< 13pt - Headers, emphasis
        ICON_SMALL, ///< 8x8 icon font
        ICON_LARGE  ///< 16x16 icon font
    };

    // ========================================================================
    // Constructor / Singleton
    // ========================================================================

    /**
     * @brief Constructor - wraps U8G2 instance
     * @param u8g2 Reference to U8G2 display instance
     */
    explicit DisplayCanvas(U8G2& u8g2);

    /**
     * @brief Get singleton instance (after begin() called)
     * @return Reference to canvas instance
     */
    static DisplayCanvas& getInstance();

    // ========================================================================
    // Display Management
    // ========================================================================

    /**
     * @brief Clear framebuffer (fill with black)
     */
    void clear();

    /**
     * @brief Send framebuffer to physical display
     *
     * Call after all drawing complete for current frame.
     * Framework calls this automatically after drawDashboard().
     */
    void sendBuffer();

    /**
     * @brief Set draw color
     * @param color 0=black (clear), 1=white (draw), 2=XOR
     */
    void setDrawColor(uint8_t color);

    /**
     * @brief Get current draw color
     * @return Color value (0, 1, or 2)
     */
    uint8_t getDrawColor() const;

    // ========================================================================
    // Basic Shapes
    // ========================================================================

    /**
     * @brief Draw single pixel
     * @param x X coordinate
     * @param y Y coordinate
     */
    void drawPixel(int16_t x, int16_t y);

    /**
     * @brief Draw line
     * @param x0 Start X
     * @param y0 Start Y
     * @param x1 End X
     * @param y1 End Y
     */
    void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1);

    /**
     * @brief Draw horizontal line (optimized)
     * @param x Start X
     * @param y Y coordinate
     * @param w Width
     */
    void drawHLine(int16_t x, int16_t y, int16_t w);

    /**
     * @brief Draw vertical line (optimized)
     * @param x X coordinate
     * @param y Start Y
     * @param h Height
     */
    void drawVLine(int16_t x, int16_t y, int16_t h);

    /**
     * @brief Draw rectangle
     * @param x Top-left X
     * @param y Top-left Y
     * @param w Width
     * @param h Height
     * @param filled true=filled, false=outline
     */
    void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, bool filled = false);

    /**
     * @brief Draw rounded rectangle
     * @param x Top-left X
     * @param y Top-left Y
     * @param w Width
     * @param h Height
     * @param r Corner radius
     * @param filled true=filled, false=outline
     */
    void drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, bool filled = false);

    /**
     * @brief Draw circle
     * @param x Center X
     * @param y Center Y
     * @param r Radius
     * @param filled true=filled, false=outline
     */
    void drawCircle(int16_t x, int16_t y, int16_t r, bool filled = false);

    /**
     * @brief Draw ellipse
     * @param x Center X
     * @param y Center Y
     * @param rx Horizontal radius
     * @param ry Vertical radius
     * @param filled true=filled, false=outline
     */
    void drawEllipse(int16_t x, int16_t y, int16_t rx, int16_t ry, bool filled = false);

    /**
     * @brief Draw triangle
     * @param x0 Point 1 X
     * @param y0 Point 1 Y
     * @param x1 Point 2 X
     * @param y1 Point 2 Y
     * @param x2 Point 3 X
     * @param y2 Point 3 Y
     * @param filled true=filled, false=outline
     */
    void drawTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, bool filled = false);

    // ========================================================================
    // Text
    // ========================================================================

    /**
     * @brief Set current font
     * @param font Font size enum
     */
    void setFont(Font font);

    /**
     * @brief Get current font
     * @return Current font enum
     */
    Font getFont() const;

    /**
     * @brief Draw text string
     * @param x X coordinate (baseline left)
     * @param y Y coordinate (baseline)
     * @param text Null-terminated string
     */
    void drawText(int16_t x, int16_t y, const char* text);

    /**
     * @brief Draw formatted text (printf-style)
     * @param x X coordinate
     * @param y Y coordinate
     * @param fmt Format string
     * @param ... Format arguments
     */
    void drawTextF(int16_t x, int16_t y, const char* fmt, ...);

    /**
     * @brief Get text width in pixels
     * @param text String to measure
     * @return Width in pixels
     */
    int16_t getTextWidth(const char* text) const;

    /**
     * @brief Get current font height in pixels
     * @return Height in pixels
     */
    int16_t getTextHeight() const;

    /**
     * @brief Draw text centered horizontally
     * @param y Y coordinate
     * @param text String to draw
     */
    void drawTextCentered(int16_t y, const char* text);

    /**
     * @brief Draw text right-aligned
     * @param x Right edge X
     * @param y Y coordinate
     * @param text String to draw
     */
    void drawTextRight(int16_t x, int16_t y, const char* text);

    // ========================================================================
    // Icons
    // ========================================================================

    /**
     * @brief Draw icon from icon font
     * @param x X coordinate
     * @param y Y coordinate
     * @param iconCode Unicode code point (e.g., 0x0094 for lightbulb)
     */
    void drawIcon(int16_t x, int16_t y, uint16_t iconCode);

    /**
     * @brief Draw icon from IconLibrary by ID
     *
     * Looks up icon in IconLibrary and draws bitmap at specified position.
     *
     * @param x X coordinate (top-left)
     * @param y Y coordinate (top-left)
     * @param iconId Icon identifier (e.g., ICON_DRONE, ICON_BATTERY_FULL)
     * @return true if icon found and drawn, false if not found
     *
     * @example
     * ```cpp
     * canvas.drawIconByID(0, 0, ICON_DRONE);
     * canvas.drawIconByID(112, 0, ICON_BATTERY_FULL);
     * ```
     */
    bool drawIconByID(int16_t x, int16_t y, const char* iconId);

    // ========================================================================
    // Widgets
    // ========================================================================

    /**
     * @brief Draw horizontal progress bar
     *
     * @param x Top-left X
     * @param y Top-left Y
     * @param w Width
     * @param h Height
     * @param value Progress (0.0 to 1.0)
     * @param label Optional label above bar (nullptr = no label)
     */
    void drawProgressBar(int16_t x, int16_t y, int16_t w, int16_t h, float value, const char* label = nullptr);

    /**
     * @brief Draw circular gauge (arc with needle)
     *
     * @param x Center X
     * @param y Center Y
     * @param r Radius
     * @param value Current value
     * @param min Minimum value (arc start)
     * @param max Maximum value (arc end)
     * @param label Optional center label (nullptr = no label)
     */
    void drawGauge(int16_t x, int16_t y, int16_t r, float value, float min, float max, const char* label = nullptr);

    /**
     * @brief Draw line graph (time series)
     *
     * @param x Top-left X
     * @param y Top-left Y
     * @param w Width
     * @param h Height
     * @param values Array of values
     * @param count Number of values
     * @param min Y-axis minimum
     * @param max Y-axis maximum
     */
    void drawGraph(int16_t x, int16_t y, int16_t w, int16_t h, const float* values, size_t count, float min, float max);

    /**
     * @brief Draw joystick visualization (crosshair in circle)
     *
     * @param x Center X
     * @param y Center Y
     * @param r Radius
     * @param joyX Joystick X (-1.0 to +1.0)
     * @param joyY Joystick Y (-1.0 to +1.0)
     */
    void drawJoystickViz(int16_t x, int16_t y, int16_t r, float joyX, float joyY);

    /**
     * @brief Draw battery icon with percentage
     *
     * @param x Top-left X
     * @param y Top-left Y
     * @param percent Battery percentage (0-100)
     */
    void drawBatteryIcon(int16_t x, int16_t y, uint8_t percent);

    /**
     * @brief Draw signal strength bars
     *
     * @param x Top-left X
     * @param y Top-left Y
     * @param strength Signal strength (0-4 bars)
     */
    void drawSignalStrength(int16_t x, int16_t y, uint8_t strength);

    // ========================================================================
    // Layout Helpers
    // ========================================================================

    /**
     * @brief Draw standard header bar (title + underline)
     *
     * @param title Header text (max 20 chars for readability)
     */
    void drawHeader(const char* title);

    /**
     * @brief Draw standard status bar (connection + battery + packet counts)
     *
     * @param connected Connection status
     * @param battery Battery percentage (0-100)
     * @param txCount TX packet count
     * @param rxCount RX packet count
     */
    void drawStatusBar(bool connected, uint8_t battery, uint32_t txCount, uint32_t rxCount);

    /**
     * @brief Draw footer with left/center/right text
     *
     * @param leftText Left-aligned text (nullptr = skip)
     * @param centerText Centered text (nullptr = skip)
     * @param rightText Right-aligned text (nullptr = skip)
     */
    void drawFooter(const char* leftText, const char* centerText, const char* rightText);

    // ========================================================================
    // Advanced
    // ========================================================================

    /**
     * @brief Set clipping rectangle
     *
     * All subsequent drawing clipped to this region.
     *
     * @param x Clip region X
     * @param y Clip region Y
     * @param w Clip region width
     * @param h Clip region height
     */
    void setClipRect(int16_t x, int16_t y, int16_t w, int16_t h);

    /**
     * @brief Clear clipping rectangle (reset to full screen)
     */
    void clearClipRect();

    /**
     * @brief Get display width in pixels
     * @return Width (typically 128)
     */
    int16_t getWidth() const;

    /**
     * @brief Get display height in pixels
     * @return Height (typically 64)
     */
    int16_t getHeight() const;

    /**
     * @brief Get reference to underlying U8G2 instance (advanced use)
     *
     * For direct U8G2 access when canvas API insufficient.
     * Use sparingly to maintain abstraction.
     *
     * @return Reference to U8G2 instance
     */
    U8G2& getU8G2();

private:
    U8G2& u8g2_;           ///< Underlying U8G2 instance
    Font currentFont_;     ///< Current font setting

    static DisplayCanvas* instance_;  ///< Singleton instance

    // Font mapping (U8G2 font pointers)
    const uint8_t* getFontPointer(Font font) const;
};
