#pragma once
#include <cstdint>

// RGB color structure for LEDs
struct Color {
    uint8_t r, g, b;
    Color(uint8_t red = 0, uint8_t green = 0, uint8_t blue = 0) : r(red), g(green), b(blue) {}
    
    // Predefined colors
    static const Color BLACK;
    static const Color WHITE;
    static const Color RED;
    static const Color GREEN;
    static const Color BLUE;
    static const Color YELLOW;
    static const Color CYAN;
    static const Color MAGENTA;
    static const Color ORANGE;
    static const Color PURPLE;
    static const Color DIM_WHITE;
    static const Color DIM_GREEN;
    static const Color DIM_RED;
    
    // Helper method to create color from HSV
    static Color fromHSV(float hue, float saturation, float value) {
        hue = fmod(hue, 360.0f);
        if (hue < 0) hue += 360.0f;
        
        saturation = (std::max)(0.0f, (std::min)(1.0f, saturation));
        value = (std::max)(0.0f, (std::min)(1.0f, value));
        
        float c = value * saturation;
        float x = c * (1.0f - abs(fmod(hue / 60.0f, 2.0f) - 1.0f));
        float m = value - c;
        
        float r, g, b;
        if (hue < 60) {
            r = c; g = x; b = 0;
        } else if (hue < 120) {
            r = x; g = c; b = 0;
        } else if (hue < 180) {
            r = 0; g = c; b = x;
        } else if (hue < 240) {
            r = 0; g = x; b = c;
        } else if (hue < 300) {
            r = x; g = 0; b = c;
        } else {
            r = c; g = 0; b = x;
        }
        
        return Color(
            static_cast<uint8_t>((r + m) * 255),
            static_cast<uint8_t>((g + m) * 255),
            static_cast<uint8_t>((b + m) * 255)
        );
    }
};

// Static color definitions
const Color Color::BLACK(0, 0, 0);
const Color Color::WHITE(255, 255, 255);
const Color Color::RED(255, 0, 0);
const Color Color::GREEN(0, 255, 0);
const Color Color::BLUE(0, 0, 255);
const Color Color::YELLOW(255, 255, 0);
const Color Color::CYAN(0, 255, 255);
const Color Color::MAGENTA(255, 0, 255);
const Color Color::ORANGE(255, 128, 0);
const Color Color::PURPLE(128, 0, 255);
const Color Color::DIM_WHITE(64, 64, 64);
const Color Color::DIM_GREEN(0, 64, 0);
const Color Color::DIM_RED(64, 0, 0);