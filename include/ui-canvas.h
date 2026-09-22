#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>
#include "display-diff.h"
#include "japanese-font.h"

// 通知用CYDと同じ16px Unifont字形で TFT_eSPI 本体またはSpriteへ描画する。
class UiCanvas {
 public:
  explicit UiCanvas(TFT_eSPI& target) : target_(&target) {}
  void use(TFT_eSPI& target) { target_ = &target; }
  void setTextColor(uint16_t fg, uint16_t bg) { fg_ = fg; bg_ = bg; }
  void setTextSize(uint8_t) {}
  void setTextWrap(bool, bool) {}
  void fillScreen(uint16_t color) { display_diff::clearFrame(*target_, color); }
  void fillRect(int x, int y, int w, int h, uint16_t color) {
    target_->fillRect(x, y, w, h, color);
  }
  void fillRoundRect(int x, int y, int w, int h, int r, uint16_t color) {
    target_->fillRoundRect(x, y, w, h, r, color);
  }
  void drawRoundRect(int x, int y, int w, int h, int r, uint16_t color) {
    target_->drawRoundRect(x, y, w, h, r, color);
  }
  int32_t textWidth(const String& value) const {
    int32_t width = 0;
    const uint8_t* p = reinterpret_cast<const uint8_t*>(value.c_str());
    while (*p) {
      const uint32_t codepoint = next(p);
      width += codepoint >= 32 && codepoint < 127 ? 8 : 16;
    }
    return width;
  }
  void drawString(const String& value, int32_t x, int32_t y) {
    const uint8_t* p = reinterpret_cast<const uint8_t*>(value.c_str());
    while (*p && x < 320) {
      const uint32_t codepoint = next(p);
      if (codepoint >= 32 && codepoint < 127) {
        target_->drawBitmap(x, y, codex_usage_font::kAsciiGlyphs[codepoint - 32],
                            8, 16, fg_);
        x += 8;
        continue;
      }
      bool found = false;
      for (const auto& glyph : codex_usage_font::kJapaneseGlyphs) {
        if (glyph.codepoint == codepoint) {
          target_->drawBitmap(x, y, glyph.bitmap, 16, 16, fg_);
          found = true;
          break;
        }
      }
      if (!found) target_->drawRect(x, y, 14, 14, fg_);
      x += 16;
    }
  }

 private:
  static uint32_t next(const uint8_t*& p) {
    uint32_t c = *p++;
    if (c < 128) return c;
    if ((c & 0xE0) == 0xC0 && *p)
      return ((c & 31) << 6) | (*p++ & 63);
    if ((c & 0xF0) == 0xE0 && p[0] && p[1]) {
      c = ((c & 15) << 12) | ((p[0] & 63) << 6) | (p[1] & 63);
      p += 2;
      return c;
    }
    if ((c & 0xF8) == 0xF0 && p[0] && p[1] && p[2]) {
      p += 3;
      return 0xFFFD;
    }
    return 0xFFFD;
  }
  TFT_eSPI* target_;
  uint16_t fg_ = TFT_WHITE, bg_ = TFT_BLACK;
};
