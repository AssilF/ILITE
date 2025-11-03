/**
 * @file UIComponents.cpp
 * @brief Modern UI Components Implementation
 */

#include "UIComponents.h"
#include <cstdio>
#include <cstring>

// ============================================================================
// Modern Header
// ============================================================================

void UIComponents::drawModernHeader(DisplayCanvas& canvas,
                                    const char* title,
                                    IconID icon,
                                    uint8_t batteryPercent,
                                    uint8_t signalStrength,
                                    bool showDivider) {
    int16_t x = 0;
    int16_t y = 0;

    // Draw icon if provided (8x8 icon on left)
    if (icon != nullptr) {
        canvas.drawIconByID(x, y + 2, icon);
        x += 10;  // Icon width + spacing
    }

    // Draw title
    canvas.setFont(DisplayCanvas::NORMAL);
    canvas.drawText(x, y + 10, title);

    // Draw battery indicator (right side)
    int16_t rightX = canvas.getWidth();
    if (batteryPercent > 0) {
        rightX -= drawBatteryIndicator(canvas, rightX, y, batteryPercent);
        rightX -= 2;  // Spacing
    }

    // Draw signal indicator (right side, before battery)
    if (signalStrength > 0) {
        rightX -= drawSignalIndicator(canvas, rightX, y, signalStrength);
    }

    // Draw divider line
    if (showDivider) {
        canvas.drawLine(0, HEADER_HEIGHT - 1, canvas.getWidth(), HEADER_HEIGHT - 1);
    }
}

int16_t UIComponents::drawBatteryIndicator(DisplayCanvas& canvas,
                                           int16_t x, int16_t y,
                                           uint8_t percent) {
    // Select icon based on charge level
    IconID batteryIcon;
    if (percent >= 75) {
        batteryIcon = ICON_BATTERY_FULL;
    } else if (percent >= 30) {
        batteryIcon = ICON_BATTERY_MED;
    } else {
        batteryIcon = ICON_BATTERY_LOW;
    }

    // Draw percentage text
    char percentText[5];
    snprintf(percentText, sizeof(percentText), "%u%%", percent);

    canvas.setFont(DisplayCanvas::TINY);
    int16_t textWidth = canvas.getTextWidth(percentText);
    int16_t totalWidth = 8 + 1 + textWidth;  // icon + spacing + text

    canvas.drawIconByID(x - totalWidth, y, batteryIcon);
    canvas.drawText(x - textWidth, y + 7, percentText);

    return totalWidth;
}

int16_t UIComponents::drawSignalIndicator(DisplayCanvas& canvas,
                                          int16_t x, int16_t y,
                                          uint8_t strength) {
    // Select icon based on signal strength
    IconID signalIcon;
    switch (strength) {
        case 4:  signalIcon = ICON_SIGNAL_FULL; break;
        case 3:  signalIcon = ICON_SIGNAL_MED; break;
        case 2:  signalIcon = ICON_SIGNAL_LOW; break;
        default: signalIcon = ICON_SIGNAL_NONE; break;
    }

    canvas.drawIconByID(x - 8, y, signalIcon);
    return 8;
}

// ============================================================================
// Modern Footer
// ============================================================================

void UIComponents::drawModernFooter(DisplayCanvas& canvas,
                                    const char* button1Label,
                                    const char* button2Label,
                                    const char* button3Label,
                                    const char* statLabel,
                                    bool showDivider) {
    int16_t y = canvas.getHeight() - FOOTER_HEIGHT;

    // Draw divider line
    if (showDivider) {
        canvas.drawLine(0, y, canvas.getWidth(), y);
    }

    y += 2;  // Padding from divider

    // Draw button labels
    canvas.setFont(DisplayCanvas::TINY);
    int16_t x = 2;

    if (button1Label) {
        canvas.drawTextF(x, y + 7, "[1]%s", button1Label);
        x += canvas.getTextWidth("[1]") + canvas.getTextWidth(button1Label) + 4;
    }

    if (button2Label) {
        canvas.drawTextF(x, y + 7, "[2]%s", button2Label);
        x += canvas.getTextWidth("[2]") + canvas.getTextWidth(button2Label) + 4;
    }

    if (button3Label) {
        canvas.drawTextF(x, y + 7, "[3]%s", button3Label);
    }

    // Draw stat label on right side
    if (statLabel) {
        int16_t statWidth = canvas.getTextWidth(statLabel);
        canvas.drawText(canvas.getWidth() - statWidth - 2, y + 7, statLabel);
    }
}

// ============================================================================
// Breadcrumb Navigation
// ============================================================================

void UIComponents::drawBreadcrumb(DisplayCanvas& canvas,
                                  const char* breadcrumb,
                                  int16_t y,
                                  bool showBackIcon) {
    int16_t x = 0;

    // Draw back icon
    if (showBackIcon) {
        canvas.drawIconByID(x, y + 1, ICON_BACK);
        x += 10;  // Icon width + spacing
    }

    // Draw breadcrumb text
    canvas.setFont(DisplayCanvas::SMALL);
    canvas.drawText(x, y + 8, breadcrumb);

    // Draw divider line
    canvas.drawLine(0, y + BREADCRUMB_HEIGHT - 1, canvas.getWidth(), y + BREADCRUMB_HEIGHT - 1);
}

// ============================================================================
// Menu Rendering
// ============================================================================

void UIComponents::drawModernMenu(DisplayCanvas& canvas,
                                  const MenuItem* items,
                                  size_t itemCount,
                                  int focusedIndex,
                                  int scrollOffset,
                                  int16_t y,
                                  int maxVisible) {
    if (items == nullptr || itemCount == 0) {
        return;
    }

    canvas.setFont(DisplayCanvas::SMALL);

    // Calculate visible range
    int startIndex = scrollOffset;
    int endIndex = min(scrollOffset + maxVisible, (int)itemCount);

    int16_t itemY = y;

    for (int i = startIndex; i < endIndex; ++i) {
        const MenuItem& item = items[i];
        bool isFocused = (i == focusedIndex);

        int16_t x = 0;

        // Draw focus indicator (arrow)
        if (isFocused) {
            canvas.drawIconByID(x, itemY + 3, ICON_RIGHT);
        }
        x += 10;  // Space for focus indicator

        // Draw icon
        if (item.icon != nullptr) {
            canvas.drawIconByID(x, itemY + 3, item.icon);
        }
        x += 10;  // Icon width + spacing

        // Draw label
        if (item.isReadOnly) {
            // Gray out read-only items (use XOR mode)
            canvas.setDrawColor(2);
            canvas.drawText(x, itemY + 10, item.label);
            canvas.setDrawColor(1);
        } else {
            canvas.drawText(x, itemY + 10, item.label);
        }

        // Draw indicators on right side
        int16_t rightX = canvas.getWidth() - 2;

        // Submenu arrow
        if (item.hasSubmenu) {
            canvas.drawIconByID(rightX - 8, itemY + 3, ICON_RIGHT);
            rightX -= 10;
        }

        // Toggle checkmark
        if (item.isToggle && item.toggleState) {
            canvas.drawIconByID(rightX - 8, itemY + 3, ICON_CHECK);
            rightX -= 10;
        }

        // Value text
        if (item.value != nullptr) {
            int16_t valueWidth = canvas.getTextWidth(item.value);
            canvas.drawText(rightX - valueWidth, itemY + 10, item.value);
        }

        itemY += MENU_ITEM_HEIGHT;
    }

    // Draw scroll indicator if needed
    if (itemCount > maxVisible) {
        drawScrollIndicator(canvas, focusedIndex + 1, itemCount);
    }
}

void UIComponents::drawScrollIndicator(DisplayCanvas& canvas,
                                       int currentIndex,
                                       int totalCount,
                                       int16_t y) {
    char indicator[16];
    snprintf(indicator, sizeof(indicator), "%d / %d", currentIndex, totalCount);

    canvas.setFont(DisplayCanvas::TINY);
    int16_t width = canvas.getTextWidth(indicator);
    int16_t x = (canvas.getWidth() - width) / 2;

    canvas.drawText(x, y + 6, indicator);
}

int UIComponents::calculateScrollOffset(int focusedIndex,
                                        int scrollOffset,
                                        int maxVisible) {
    // Scroll down if focused item is below visible area
    if (focusedIndex >= scrollOffset + maxVisible) {
        return focusedIndex - maxVisible + 1;
    }

    // Scroll up if focused item is above visible area
    if (focusedIndex < scrollOffset) {
        return focusedIndex;
    }

    return scrollOffset;
}

// ============================================================================
// Module Selector
// ============================================================================

void UIComponents::drawModuleCard(DisplayCanvas& canvas,
                                  const ModuleCard& card,
                                  int16_t x, int16_t y,
                                  int16_t width, int16_t height,
                                  bool focused) {
    // Draw card frame
    canvas.drawRoundRect(x, y, width, height, 6, false);
    if (focused) {
        canvas.drawRoundRect(x + 1, y + 1, width - 2, height - 2, 6, false);
    }

    // Draw large icon (scaled if 8x8)
    if (card.icon != nullptr) {
        canvas.drawIconByID(x + 6, y + 6, card.icon);
    }

    // Draw module name
    canvas.setFont(DisplayCanvas::NORMAL);
    canvas.drawText(x + 22, y + 12, card.name);

    // Draw type
    canvas.setFont(DisplayCanvas::SMALL);
    canvas.drawText(x + 22, y + 22, card.type);

    // Draw version
    canvas.setFont(DisplayCanvas::TINY);
    canvas.drawText(x + 22, y + 30, card.version);

    // Draw active indicator
    if (card.isActive) {
        canvas.drawIconByID(x + 22, y + 32, ICON_CHECK);
        canvas.drawText(x + 32, y + 38, "ACTIVE");
    }
}

// ============================================================================
// Dashboard Widgets
// ============================================================================

void UIComponents::drawLabeledValue(DisplayCanvas& canvas,
                                    int16_t x, int16_t y,
                                    IconID icon,
                                    const char* label,
                                    const char* value) {
    // Draw icon
    if (icon != nullptr) {
        canvas.drawIconByID(x, y, icon);
        x += 10;
    }

    // Draw label
    canvas.setFont(DisplayCanvas::SMALL);
    canvas.drawTextF(x, y + 7, "%s: %s", label, value);
}

void UIComponents::drawMiniGauge(DisplayCanvas& canvas,
                                 int16_t x, int16_t y,
                                 int16_t radius,
                                 float value, float min, float max,
                                 IconID icon,
                                 const char* label) {
    // Draw gauge using existing drawGauge widget
    canvas.drawGauge(x, y, radius, value, min, max, nullptr);

    // Draw icon above gauge
    if (icon != nullptr) {
        canvas.drawIconByID(x - 4, y - radius - 10, icon);
    }

    // Draw label below gauge
    if (label != nullptr) {
        canvas.setFont(DisplayCanvas::TINY);
        int16_t labelWidth = canvas.getTextWidth(label);
        canvas.drawText(x - labelWidth / 2, y + radius + 8, label);
    }
}

void UIComponents::drawConnectionStatus(DisplayCanvas& canvas,
                                        int16_t x, int16_t y,
                                        bool isPaired,
                                        bool isSearching) {
    IconID statusIcon;
    const char* statusText;

    if (isPaired) {
        statusIcon = ICON_CHECK;
        statusText = "Connected";
    } else if (isSearching) {
        statusIcon = ICON_INFO;
        statusText = "Searching...";
    } else {
        statusIcon = ICON_CROSS;
        statusText = "No Connection";
    }

    // Draw icon
    canvas.drawIconByID(x, y, statusIcon);

    // Draw status text
    canvas.setFont(DisplayCanvas::SMALL);
    canvas.drawText(x + 10, y + 7, statusText);
}

// ============================================================================
// Modal/Dialog
// ============================================================================

void UIComponents::drawModal(DisplayCanvas& canvas,
                             IconID icon,
                             const char* message,
                             const char* button1Label,
                             const char* button2Label,
                             int focusedButton) {
    // Modal dimensions
    int16_t modalWidth = 100;
    int16_t modalHeight = 50;
    int16_t x = (canvas.getWidth() - modalWidth) / 2;
    int16_t y = (canvas.getHeight() - modalHeight) / 2;

    // Clear area behind modal
    canvas.setDrawColor(0);
    canvas.drawRect(x - 2, y - 2, modalWidth + 4, modalHeight + 4, true);
    canvas.setDrawColor(1);

    // Draw modal frame
    canvas.drawRoundRect(x, y, modalWidth, modalHeight, 6, false);

    // Draw icon at top
    if (icon != nullptr) {
        int16_t iconX = x + (modalWidth - 8) / 2;
        canvas.drawIconByID(iconX, y + 6, icon);
    }

    // Draw message
    canvas.setFont(DisplayCanvas::SMALL);
    int16_t messageWidth = canvas.getTextWidth(message);
    int16_t messageX = x + (modalWidth - messageWidth) / 2;
    canvas.drawText(messageX, y + 22, message);

    // Draw buttons
    int16_t buttonY = y + modalHeight - 12;
    int16_t button1X = x + 10;
    int16_t button2X = x + modalWidth - 10 - canvas.getTextWidth(button2Label);

    canvas.setFont(DisplayCanvas::SMALL);

    // Button 1
    if (focusedButton == 0) {
        canvas.drawRoundRect(button1X - 2, buttonY - 2,
                            canvas.getTextWidth(button1Label) + 4, 10, 2, false);
    }
    canvas.drawText(button1X, buttonY + 7, button1Label);

    // Button 2
    if (focusedButton == 1) {
        canvas.drawRoundRect(button2X - 2, buttonY - 2,
                            canvas.getTextWidth(button2Label) + 4, 10, 2, false);
    }
    canvas.drawText(button2X, buttonY + 7, button2Label);
}
