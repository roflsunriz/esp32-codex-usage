#ifndef CODEX_USAGE_BOARD_DISPLAY_H
#define CODEX_USAGE_BOARD_DISPLAY_H

// LovyanGFX 1.2.28 の LGFX_Device 設定。
// 対象: ESP32-2432S028R (CYD), ILI9341, XPT2046。
#include <LovyanGFX.hpp>

namespace codex_usage_board {

constexpr int kTftSck = 14;
constexpr int kTftMiso = 12;
constexpr int kTftMosi = 13;
constexpr int kTftCs = 15;
constexpr int kTftDc = 2;
constexpr int kTftReset = -1;  // ボードの EN/RST に接続されているため GPIO は使わない。
constexpr int kTftBacklight = 21;

constexpr int kTouchSck = 25;
constexpr int kTouchMiso = 39;
constexpr int kTouchMosi = 32;
constexpr int kTouchCs = 33;
constexpr int kTouchIrq = 36;

// 表示とタッチは異なる配線のSPIバスを使う。
inline void configureDisplaySpi(lgfx::Bus_SPI::config_t& cfg) {
  cfg.spi_host = SPI2_HOST;
  cfg.spi_mode = 0;
  cfg.freq_write = 40000000;
  cfg.freq_read = 16000000;
  cfg.spi_3wire = false;  // MOSI と MISO が別配線の通常の 4 線 SPI。
  cfg.use_lock = true;
  cfg.pin_sclk = kTftSck;
  cfg.pin_mosi = kTftMosi;
  cfg.pin_miso = kTftMiso;
  cfg.pin_dc = kTftDc;
}

inline void configureTouchSpi(lgfx::Touch_XPT2046::config_t& cfg) {
  cfg.spi_host = SPI3_HOST;
  cfg.freq = 1000000;
  cfg.x_min = 300;
  cfg.x_max = 3900;
  cfg.y_min = 3700;
  cfg.y_max = 200;
  cfg.pin_int = kTouchIrq;
  cfg.pin_sclk = kTouchSck;
  cfg.pin_mosi = kTouchMosi;
  cfg.pin_miso = kTouchMiso;
  cfg.pin_cs = kTouchCs;
  cfg.offset_rotation = 0;
  cfg.bus_shared = false;
}

}  // namespace codex_usage_board

// display.width()/height() は初期状態で 320x240（offset_rotation=1）になる。
// 2432S028R は個体差で RGB/BGR が異なることがあるため、色が逆なら panel の
// rgb_order だけを false/true で切り替えて確認する。
class CodexUsageDisplay : public lgfx::LGFX_Device {
 public:
  CodexUsageDisplay() {
    {
      auto cfg = bus_.config();
      codex_usage_board::configureDisplaySpi(cfg);
      bus_.config(cfg);
      panel_.setBus(&bus_);
    }

    {
      auto cfg = panel_.config();
      cfg.pin_cs = codex_usage_board::kTftCs;
      cfg.pin_rst = codex_usage_board::kTftReset;
      cfg.pin_busy = -1;
      cfg.memory_width = 240;
      cfg.memory_height = 320;
      cfg.panel_width = 240;
      cfg.panel_height = 320;
      cfg.offset_x = 0;
      cfg.offset_y = 0;
      cfg.offset_rotation = 1;
      cfg.dummy_read_pixel = 8;
      cfg.dummy_read_bits = 1;
      cfg.readable = true;
      cfg.invert = false;
      cfg.rgb_order = false;
      cfg.dlen_16bit = false;
      cfg.bus_shared = false;
      panel_.config(cfg);
    }

    {
      auto cfg = light_.config();
      cfg.pin_bl = codex_usage_board::kTftBacklight;
      cfg.invert = false;
      // The CYD backlight MOSFET is reliable at low PWM frequencies. The
      // default 1200 Hz is also suitable for full-off/full-on idle control.
      cfg.freq = 1200;
      cfg.pwm_channel = 7;
      light_.config(cfg);
      panel_.setLight(&light_);
    }

    {
      auto cfg = touch_.config();
      codex_usage_board::configureTouchSpi(cfg);
      touch_.config(cfg);
      panel_.setTouch(&touch_);
    }

    setPanel(&panel_);
  }

 private:
  lgfx::Panel_ILI9341 panel_;
  lgfx::Bus_SPI bus_;
  lgfx::Light_PWM light_;
  lgfx::Touch_XPT2046 touch_;
};

#endif  // CODEX_USAGE_BOARD_DISPLAY_H
