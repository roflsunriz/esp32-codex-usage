#include <Arduino.h>

#include <time.h>

#include "app-model.h"
#include "notification-display.h"
#include "boot-button.h"
#include "diagnostic-driver.h"
#include "display-diff.h"
#include "display-state.h"
#include "reset-format.h"
#include "ui-canvas.h"

namespace {

CodexUsageDisplay gDisplay;
TFT_eSprite gCanvas(&gDisplay);
UiCanvas gFrame(gDisplay);
display_diff::Bands gBandDiff;
DisplayState gDisplayState;
BootButton gBootButton;
usage::Snapshot gSnapshot;

constexpr uint16_t kFrameWidth = 320U;
constexpr uint16_t kFrameHeight = 240U;
constexpr size_t kFrameBufferBytes = static_cast<size_t>(kFrameWidth) * kFrameHeight;
bool gCanvasReady = false;
bool gCanvasFailure = false;

UiCanvas& frameTarget() {
  gFrame.use(gCanvasReady ? static_cast<TFT_eSPI&>(gCanvas)
                          : static_cast<TFT_eSPI&>(gDisplay));
  return gFrame;
}

constexpr uint32_t kSnapshotPeriodMs = 250U;
constexpr uint32_t kStaleAfterMs = 120000U;
constexpr uint8_t kBootPin = 0U;

// Touch positions are calibrated to screen 24..295 (x) and 24..215 (y),
// so all touch targets must live inside that field.
constexpr uint16_t kSliderX0 = 24U;
constexpr uint16_t kSliderX1 = 275U;
constexpr uint16_t kSliderMinutesY = 112U;
constexpr uint16_t kSliderHoursY = 148U;
constexpr uint16_t kSliderPollY = 184U;
constexpr uint16_t kSliderHalfH = 14U;
// Settings content below the fixed title (y38). The action row at the
// content bottom scrolls with the sliders; the scrollbar stays fixed.
constexpr int32_t kSettingsContentH = 250;
constexpr int32_t kSettingsVisibleTop = 38;
constexpr int32_t kSettingsVisibleBottom = 208;
constexpr int32_t kSettingsScrollMax =
    kSettingsContentH - (kSettingsVisibleBottom - kSettingsVisibleTop);
constexpr uint16_t kSettingsScrollBarX0 = 283U;
constexpr uint16_t kSettingsScrollBarY0 = 44;
constexpr uint16_t kSettingsScrollBarY1 = 204;
constexpr int32_t kSettingsScrollPage = 40;
constexpr uint16_t kSettingsActionsY = 214U;

uint32_t sliderValueFromX(uint16_t x, uint32_t minV, uint32_t maxV,
                          uint32_t step) {
  if (maxV <= minV || step == 0U) return minV;
  const uint32_t trackW = kSliderX1 - kSliderX0;
  uint32_t offset = x < kSliderX0 ? 0U : x - kSliderX0;
  if (offset > trackW) offset = trackW;
  const uint32_t steps = (maxV - minV) / step;
  uint32_t index = (offset * steps + trackW / 2U) / trackW;
  if (index > steps) index = steps;
  return minV + index * step;
}

uint16_t sliderXFromValue(uint32_t value, uint32_t minV, uint32_t maxV) {
  if (maxV <= minV) return kSliderX0;
  if (value < minV) value = minV;
  if (value > maxV) value = maxV;
  const uint32_t trackW = kSliderX1 - kSliderX0;
  const uint32_t range = maxV - minV;
  return static_cast<uint16_t>(kSliderX0 +
                               (value - minV) * trackW / range);
}

int32_t clampSettingsScroll(int32_t scroll) {
  if (scroll < 0) return 0;
  if (scroll > kSettingsScrollMax) return kSettingsScrollMax;
  return scroll;
}

struct Rect {
  uint16_t x;
  uint16_t y;
  uint16_t width;
  uint16_t height;
};

// Content-space Y; add the scroll offset handling at each use site.
constexpr Rect kSettingsActionRects[] = {
    {6U, kSettingsActionsY, 308U, 30U},
};
constexpr Rect kConnectionActionRects[] = {
    {6U, 198U, 150U, 30U},
    {164U, 198U, 150U, 30U},
};

enum class Tab : uint8_t { Usage = 0, Settings = 1, Connection = 2 };

Tab gTab = Tab::Usage;
bool gDirty = true;
bool gLastAwake = false;
uint32_t gLastSnapshotAt = 0U;
int32_t gLastCountdownKey = INT32_MIN;
int32_t gSettingsScroll = 0;
bool gPrevPressed = false;
enum class DragKind : uint8_t {
  None, SleepMinutes, SleepHours, PollInterval, Scroll
};
DragKind gDragKind = DragKind::None;
int32_t gDragStartX = 0;
int32_t gDragStartY = 0;
int32_t gDragStartScroll = 0;
String gLocalNotice;
uint32_t gLocalNoticeUntil = 0U;
bool gLastLocalNoticeActive = false;
bool gLastStale = true;
uint32_t gLastResetMinute = 0xFFFFFFFFU;
bool gDisplayOrientationKnown = false;
bool gAppliedDisplayFlipped = false;

constexpr uint16_t kBackground = 0x1082;
constexpr uint16_t kPanel = 0x18E3;
constexpr uint16_t kBarBackground = 0x4208;
constexpr uint16_t kInactiveTab = 0x4208;
constexpr uint16_t kAccent = 0x07FF;
constexpr uint16_t kGood = 0x07E0;
constexpr uint16_t kCaution = 0xFFE0;
constexpr uint16_t kWarning = 0xFD20;
constexpr uint16_t kText = 0xFFFF;
constexpr uint16_t kMuted = 0xBDF7;

uint16_t usageBarColor(float used) {
  if (used >= 90.0f) return kWarning;
  if (used >= 75.0f) return kCaution;
  return kGood;
}

String percentText(const usage::Window& window) {
  if (!window.available) {
    return String("--");
  }

  String text = String(window.used, 1);
  text += "%";
  return text;
}

uint16_t percentColor(const usage::Window& window) {
  if (!window.available) {
    return kMuted;
  }
  return usageBarColor(window.used);
}

// Seconds until the next usage fetch for the footer. Returns INT32_MIN
// while the cadence is unknown or a fetch is in flight.
int32_t pollCountdownSec(const usage::Snapshot& snapshot, uint32_t now) {
  if (snapshot.fetching || snapshot.nextPollMs == 0U) {
    return INT32_MIN;
  }
  const int32_t remaining =
      static_cast<int32_t>(snapshot.nextPollMs - now);
  if (remaining <= 0) {
    return INT32_MIN;
  }
  return (remaining + 999) / 1000;
}

String resetLine(const usage::Window& window) {
  const int64_t now = static_cast<int64_t>(time(nullptr));
  return String(usage::resetLineText(window.resetsAt, now).c_str());
}

bool contains(const Rect& rect, uint16_t x, uint16_t y) {
  return x >= rect.x && y >= rect.y && static_cast<uint32_t>(x - rect.x) < rect.width &&
         static_cast<uint32_t>(y - rect.y) < rect.height;
}

// The Connection tab only shows while unauthenticated. Authenticated tabs
// are wider (2 tabs), setup tabs are narrower (3 tabs).
uint8_t tabCount() {
  return gSnapshot.authenticated ? 2U : 3U;
}

Rect tabRectAt(uint8_t index) {
  if (tabCount() == 2U) {
    return index == 0U ? Rect{0U, 2U, 158U, 28U} : Rect{162U, 2U, 158U, 28U};
  }
  if (index == 0U) return Rect{0U, 2U, 104U, 28U};
  if (index == 1U) return Rect{106U, 2U, 104U, 28U};
  return Rect{212U, 2U, 106U, 28U};
}

int tabHitIndex(uint16_t x, uint16_t y) {
  const uint8_t count = tabCount();
  for (uint8_t i = 0U; i < count; ++i) {
    if (contains(tabRectAt(i), x, y)) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

template <size_t N>
int hitIndex(const Rect (&rects)[N], uint16_t x, uint16_t y) {
  for (size_t i = 0U; i < N; ++i) {
    if (contains(rects[i], x, y)) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

size_t nextUtf8Index(const String& text, size_t index) {
  const size_t length = text.length();
  if (index >= length) {
    return length;
  }

  const uint8_t first = static_cast<uint8_t>(text[index]);
  size_t width = 1U;
  if ((first & 0x80U) == 0U) {
    return index + 1U;
  }
  if ((first & 0xE0U) == 0xC0U) {
    width = 2U;
  } else if ((first & 0xF0U) == 0xE0U) {
    width = 3U;
  } else if ((first & 0xF8U) == 0xF0U) {
    width = 4U;
  } else {
    return index + 1U;
  }

  if (index + width > length) {
    return index + 1U;
  }
  for (size_t i = 1U; i < width; ++i) {
    if ((static_cast<uint8_t>(text[index + i]) & 0xC0U) != 0x80U) {
      return index + 1U;
    }
  }
  return index + width;
}

String fitText(const String& text, int32_t maxWidth) {
  if (maxWidth <= 0) {
    return String();
  }
  if (frameTarget().textWidth(text) <= maxWidth) {
    return text;
  }

  const String ellipsis = "...";
  if (frameTarget().textWidth(ellipsis) > maxWidth) {
    return String();
  }

  String result;
  size_t index = 0U;
  while (index < text.length()) {
    const size_t next = nextUtf8Index(text, index);
    const String part =
        text.substring(static_cast<unsigned int>(index), static_cast<unsigned int>(next));
    const String candidate = result + part + ellipsis;
    if (frameTarget().textWidth(candidate) > maxWidth) {
      break;
    }
    result += part;
    index = next;
  }
  return result + ellipsis;
}

void drawFittedText(const String& text, uint16_t x, uint16_t y, uint16_t maxWidth,
                    uint16_t color = kText, uint16_t background = kBackground) {
  frameTarget().setTextColor(color, background);
  frameTarget().drawString(fitText(text, maxWidth), x, y);
}

bool localNoticeActive(uint32_t now) {
  return !gLocalNotice.isEmpty() && static_cast<int32_t>(now - gLocalNoticeUntil) < 0;
}

void setLocalNotice(const String& notice, uint32_t now) {
  gLocalNotice = notice;
  gLocalNoticeUntil = now + 3000U;
  gDirty = true;
}

String visibleStatus(uint32_t now) {
  if (gCanvasFailure) {
    return String("描画用メモリ不足。再起動してください");
  }
  String status = localNoticeActive(now) ? gLocalNotice : gSnapshot.status;
  const int32_t countdown = pollCountdownSec(gSnapshot, now);
  if (countdown != INT32_MIN) {
    status += String(" あと") + String(countdown) + String("秒");
  }
  return status;
}

bool snapshotIsStale(const usage::Snapshot& snapshot, uint32_t now) {
  return snapshot.updatedAt == 0U ||
         static_cast<uint32_t>(now - snapshot.updatedAt) > kStaleAfterMs;
}

bool windowChanged(const usage::Window& before, const usage::Window& after) {
  return before.available != after.available || before.used != after.used ||
         before.resetsAt != after.resetsAt;
}

bool snapshotDisplayChanged(const usage::Snapshot& before, const usage::Snapshot& after) {
  return windowChanged(before.fiveHour, after.fiveHour) ||
         windowChanged(before.weekly, after.weekly) || before.status != after.status ||
         before.fetching != after.fetching ||
         before.pollIntervalMs != after.pollIntervalMs ||
         before.nextPollMs != after.nextPollMs ||
         before.deviceCode != after.deviceCode || before.apName != after.apName ||
         before.apPassword != after.apPassword || before.address != after.address ||
         before.connected != after.connected || before.authenticated != after.authenticated ||
         before.setupActive != after.setupActive || before.timeoutMs != after.timeoutMs ||
         before.displayFlipped != after.displayFlipped;
}

void drawButton(const Rect& rect, const String& label, bool selected = false,
                uint16_t selectedText = kBackground,
                uint16_t idleFill = kPanel) {
  const uint16_t fill = selected ? kAccent : idleFill;
  const uint16_t border = selected ? kAccent : kMuted;
  const uint16_t text = selected ? selectedText : kText;
  frameTarget().fillRoundRect(rect.x, rect.y, rect.width, rect.height, 4, fill);
  frameTarget().drawRoundRect(rect.x, rect.y, rect.width, rect.height, 4,
                              border);
  const uint16_t maxWidth = rect.width > 12U ? rect.width - 12U : rect.width;
  const String visible = fitText(label, maxWidth);
  const int32_t textWidth = frameTarget().textWidth(visible);
  const uint16_t textX =
      static_cast<uint16_t>(rect.x + (textWidth < rect.width ? (rect.width - textWidth) / 2 : 0));
  const uint16_t textY =
      static_cast<uint16_t>(rect.y + (rect.height > 16U ? (rect.height - 16U) / 2U : 0U));
  frameTarget().setTextColor(text, fill);
  frameTarget().drawString(visible, textX, textY);
}

void drawTabs() {
  const uint8_t count = tabCount();
  const Tab selected = gTab;

  for (uint8_t i = 0U; i < count; ++i) {
    const String label =
        i == 0U ? String("使用量") : (i == 1U ? String("設定") : String("接続"));
    drawButton(tabRectAt(i), label, static_cast<Tab>(i) == selected,
               kBackground, kInactiveTab);
  }
}

void drawProgress(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                  const usage::Window& window) {
  frameTarget().drawRect(x, y, width, height, kText);
  const uint16_t innerX = x + 1U;
  const uint16_t innerY = y + 1U;
  const uint16_t innerW = width > 2U ? width - 2U : 0U;
  const uint16_t innerH = height > 2U ? height - 2U : 0U;
  frameTarget().fillRect(innerX, innerY, innerW, innerH, kBarBackground);
  if (!window.available) {
    return;
  }

  float used = window.used;
  if (used < 0.0f) {
    used = 0.0f;
  } else if (used > 100.0f) {
    used = 100.0f;
  }

  const uint16_t fillWidth = static_cast<uint16_t>(innerW * used / 100.0f);
  if (fillWidth > 0U) {
    frameTarget().fillRect(innerX, innerY, fillWidth, innerH,
                           usageBarColor(used));
  }
}

void drawUsage(uint32_t now) {
  frameTarget().setTextColor(kText, kBackground);
  frameTarget().drawString(String("Codex 使用量"), 8U, 38U);

  frameTarget().fillRoundRect(2U, 62U, 316U, 64U, 3, kPanel);
  frameTarget().setTextColor(kText, kPanel);
  frameTarget().drawString(String("5時間 使用率"), 10U, 68U);
  drawProgress(10U, 86U, 300U, 20U, gSnapshot.fiveHour);
  frameTarget().setTextColor(percentColor(gSnapshot.fiveHour), kPanel);
  frameTarget().drawString(percentText(gSnapshot.fiveHour), 256U, 68U);
  drawFittedText(resetLine(gSnapshot.fiveHour), 10U, 110U, 300U, kMuted, kPanel);

  frameTarget().fillRoundRect(2U, 130U, 316U, 64U, 3, kPanel);
  frameTarget().setTextColor(kText, kPanel);
  frameTarget().drawString(String("週間 使用率"), 10U, 136U);
  drawProgress(10U, 154U, 300U, 20U, gSnapshot.weekly);
  frameTarget().setTextColor(percentColor(gSnapshot.weekly), kPanel);
  frameTarget().drawString(percentText(gSnapshot.weekly), 256U, 136U);
  drawFittedText(resetLine(gSnapshot.weekly), 10U, 178U, 300U, kMuted, kPanel);

  String status = visibleStatus(now);
  if (snapshotIsStale(gSnapshot, now)) {
    status += gSnapshot.updatedAt == 0U ? String("  (未取得)") : String("  (古い)");
  }
  drawFittedText(status, 8U, 210U, 304U, snapshotIsStale(gSnapshot, now) ? kWarning : kMuted);
}

void drawSettings(uint32_t now) {
  frameTarget().setTextColor(kText, kBackground);
  frameTarget().drawString(String("消灯時間"), 8U, 38U);

  const uint32_t timeoutMs = gDisplayState.timeout();
  const uint32_t minutes = DisplayState::sleepMinutesPart(timeoutMs);
  const uint32_t hours = DisplayState::sleepHoursPart(timeoutMs);
  const uint32_t pollSec = gSnapshot.pollIntervalMs / 1000U;
  const int32_t scroll = clampSettingsScroll(gSettingsScroll);
  auto contentY = [scroll](int32_t y) -> int16_t {
    return static_cast<int16_t>(y - scroll);
  };
  auto drawContentLine = [&](int32_t y, const String& text, uint16_t color = kText) {
    const int16_t visible = contentY(y);
    if (visible < 56 || visible > 192) return;
    drawFittedText(text, 8U, static_cast<uint16_t>(visible), 290U, color);
  };
  auto drawContentSlider = [&](uint16_t centerY, uint32_t value, uint32_t minV,
                               uint32_t maxV) {
    const int16_t y = contentY(centerY);
    // Keep the title and tab zones free from scrolled content.
    if (y < 64 || y > 200) return;
    frameTarget().drawRect(kSliderX0, static_cast<uint16_t>(y - 2U),
                           kSliderX1 - kSliderX0, 5U, kText);
    const uint16_t thumbX = sliderXFromValue(value, minV, maxV);
    const uint16_t fillW = thumbX > kSliderX0 ? thumbX - kSliderX0 : 0U;
    if (fillW > 0U) {
      frameTarget().fillRect(kSliderX0, static_cast<uint16_t>(y - 2U), fillW,
                             5U, kAccent);
    }
    frameTarget().fillRect(thumbX > 6U ? thumbX - 6U : 0U,
                           static_cast<uint16_t>(y - 6U), 12U, 13U, kText);
    frameTarget().fillRect(thumbX > 4U ? thumbX - 4U : 0U,
                           static_cast<uint16_t>(y - 4U), 8U, 9U, kPanel);
  };

  if (timeoutMs == 0U) {
    drawContentLine(72, String("消灯: 常にオン"));
  } else {
    drawContentLine(72, String("消灯: ") + String(hours) + String("時間") +
                           String(minutes) + String("分"));
  }
  drawContentLine(90, String("分 0-59: ") + String(minutes) + String("分"),
                  kMuted);
  drawContentSlider(kSliderMinutesY, minutes, 0U,
                    DisplayState::kSleepMinutesMax);
  drawContentLine(126, String("時間 0-24: ") + String(hours) + String("時間"),
                  kMuted);
  drawContentSlider(kSliderHoursY, hours, 0U, DisplayState::kSleepHoursMax);
  drawContentLine(162, String("取得期間 60-600秒: ") + String(pollSec) +
                           String("秒"),
                  kMuted);
  drawContentSlider(kSliderPollY, pollSec,
                    DisplayState::kPollSliderMinSec,
                    DisplayState::kPollSliderMaxSec);
  drawContentLine(200, String("0分0時間は常にオン"), kMuted);
  for (size_t i = 0U; i < 1U; ++i) {
    const int16_t y = contentY(kSettingsActionsY);
    if (y < 56 || y > 178) continue;
    const Rect rect = {kSettingsActionRects[i].x, static_cast<uint16_t>(y),
                       kSettingsActionRects[i].width,
                       kSettingsActionRects[i].height};
    drawButton(rect, String("初期設定"));
  }
  // Scrollbar on the right edge.
  const int32_t trackH = kSettingsScrollBarY1 - kSettingsScrollBarY0;
  const int32_t thumbH =
      (kSettingsVisibleBottom - kSettingsVisibleTop) * trackH /
      kSettingsContentH;
  const int32_t travel = trackH - thumbH;
  const int32_t thumbY = travel <= 0 || kSettingsScrollMax <= 0
                             ? kSettingsScrollBarY0
                             : kSettingsScrollBarY0 +
                                   scroll * travel / kSettingsScrollMax;
  frameTarget().drawRoundRect(kSettingsScrollBarX0,
                              static_cast<uint16_t>(kSettingsScrollBarY0),
                              12U, static_cast<uint16_t>(trackH), 2, kMuted);
  frameTarget().fillRect(kSettingsScrollBarX0 + 2U,
                         static_cast<uint16_t>(thumbY), 8U,
                         static_cast<uint16_t>(thumbH), kText);

  drawContentLine(56, String("状態: ") + visibleStatus(now),
                  snapshotIsStale(gSnapshot, now) ? kWarning : kMuted);
}

void drawConnection(uint32_t now) {
  frameTarget().setTextColor(kText, kBackground);
  frameTarget().drawString(String("接続とセットアップ"), 8U, 38U);

  String line = String("状態: ") + visibleStatus(now);
  frameTarget().setTextColor(gSnapshot.connected ? kGood : kWarning, kBackground);
  drawFittedText(line, 8U, 66U, 304U, gSnapshot.connected ? kGood : kWarning);

  line = String("AP: ");
  line += gSnapshot.apName.length() == 0U ? String("--") : gSnapshot.apName;
  drawFittedText(line, 8U, 91U, 304U);

  line = String("パスワード: ");
  line += gSnapshot.apPassword.length() == 0U ? String("--") : gSnapshot.apPassword;
  drawFittedText(line, 8U, 116U, 304U);

  line = String("認証コード: ");
  line += gSnapshot.deviceCode.length() == 0U ? String("--") : gSnapshot.deviceCode;
  drawFittedText(line, 8U, 141U, 304U);

  line = gSnapshot.deviceCode.length() != 0U
             ? String("auth.openai.com/codex/device")
             : String("アドレス: ") +
                   (gSnapshot.address.length() == 0U ? String("--") : gSnapshot.address);
  drawFittedText(line, 8U, 166U, 304U);

  drawButton(kConnectionActionRects[0], String("初期設定"));
  drawButton(kConnectionActionRects[1], String("ログイン"));
}

void drawFrame(uint32_t now) {
  if (!gDisplayState.awake()) {
    return;
  }

  frameTarget().fillScreen(kBackground);
  frameTarget().setTextSize(1U);
  frameTarget().setTextWrap(false, false);
  drawTabs();

  switch (gTab) {
    case Tab::Usage:
      drawUsage(now);
      break;
    case Tab::Settings:
      drawSettings(now);
      break;
    case Tab::Connection:
      drawConnection(now);
      break;
  }
  if (gCanvasReady) {
    const uint16_t changed = gBandDiff.update(
        static_cast<const uint8_t*>(gCanvas.getPointer()));
    if (!display_diff::eachRun(changed, [](size_t top, size_t height) {
          return gCanvas.pushSprite(0, static_cast<int32_t>(top), 0,
                                    static_cast<int32_t>(top),
                                    static_cast<int32_t>(display_diff::kWidth),
                                    static_cast<int32_t>(height));
        })) {
      gCanvas.pushSprite(0, 0);
      gBandDiff.invalidate();
    }
  }
  gDirty = false;
}

bool sendCommand(usage::CommandType type, uint32_t value = 0U) {
  usage::Command command;
  command.type = type;
  command.value = value;
  if (!usage::sendCommand(command)) {
    setLocalNotice(String("通信キューへ送れません"), millis());
    return false;
  }
  gDirty = true;
  return true;
}

void applyTimeout(uint32_t value, uint32_t now) {
  if (!sendCommand(usage::CommandType::Timeout, value)) {
    return;
  }

  // worker がNVSへ保存し、Snapshot.timeoutMsで確認できた後にだけ反映する。
  setLocalNotice(String("消灯設定を保存中"), now);
}

void applyPollInterval(uint32_t pollSec, uint32_t now) {
  if (!sendCommand(usage::CommandType::PollInterval, pollSec)) {
    return;
  }

  // worker がNVSへ保存し、Snapshot.pollIntervalMsで確認できた後にだけ反映する。
  setLocalNotice(String("取得期間を保存中"), now);
}

void applySleepParts(uint32_t minutes, uint32_t hours, uint32_t now) {
  applyTimeout(DisplayState::sleepTimeoutFromParts(minutes, hours), now);
}

int32_t settingsContentY(uint16_t y) {
  return static_cast<int32_t>(y) + clampSettingsScroll(gSettingsScroll);
}

// Settings slider tap in content coordinates. Returns true when a slider
// handled the tap (and fixes the drag gesture mode).
bool handleSettingsSlider(uint16_t x, int32_t contentY, uint32_t now) {
  if (x < 16U || x > 283U) {
    return false;
  }
  if (contentY >= static_cast<int32_t>(kSliderMinutesY) - 14 &&
      contentY < static_cast<int32_t>(kSliderMinutesY) + 14) {
    const uint32_t minutes = sliderValueFromX(
        x, 0U, DisplayState::kSleepMinutesMax, 1U);
    gDragKind = DragKind::SleepMinutes;
    applySleepParts(minutes, DisplayState::sleepHoursPart(gDisplayState.timeout()),
                    now);
    return true;
  }
  if (contentY >= static_cast<int32_t>(kSliderHoursY) - 14 &&
      contentY < static_cast<int32_t>(kSliderHoursY) + 14) {
    const uint32_t hours = sliderValueFromX(
        x, 0U, DisplayState::kSleepHoursMax, 1U);
    gDragKind = DragKind::SleepHours;
    applySleepParts(DisplayState::sleepMinutesPart(gDisplayState.timeout()), hours,
                    now);
    return true;
  }
  if (contentY >= static_cast<int32_t>(kSliderPollY) - 14 &&
      contentY < static_cast<int32_t>(kSliderPollY) + 14) {
    const uint32_t pollSec = sliderValueFromX(
        x, DisplayState::kPollSliderMinSec, DisplayState::kPollSliderMaxSec,
        DisplayState::kPollSliderStepSec);
    gDragKind = DragKind::PollInterval;
    applyPollInterval(pollSec, now);
    return true;
  }
  return false;
}

bool handleSettingsScrollBar(uint16_t x, uint16_t y) {
  // Scrollbar wins over the slider end zone.
  if (x < 281U || y < kSettingsScrollBarY0 || y >= kSettingsScrollBarY1) {
    return false;
  }
  const int32_t trackH = kSettingsScrollBarY1 - kSettingsScrollBarY0;
  const int32_t thumbH =
      (kSettingsVisibleBottom - kSettingsVisibleTop) * trackH /
      kSettingsContentH;
  const int32_t travel = trackH - thumbH;
  const int32_t thumbY = travel <= 0 || kSettingsScrollMax <= 0
                             ? kSettingsScrollBarY0
                             : kSettingsScrollBarY0 +
                                   clampSettingsScroll(gSettingsScroll) *
                                       travel / kSettingsScrollMax;
  int32_t target = clampSettingsScroll(gSettingsScroll);
  if (static_cast<int32_t>(y) < thumbY) {
    target -= kSettingsScrollPage;
  } else if (static_cast<int32_t>(y) >= thumbY + thumbH) {
    target += kSettingsScrollPage;
  } else {
    gDragKind = DragKind::Scroll;
    return true;
  }
  gSettingsScroll = clampSettingsScroll(target);
  gDragKind = DragKind::Scroll;
  gDirty = true;
  return true;
}

int settingsActionAt(uint16_t x, int32_t contentY) {
  if (contentY < kSettingsActionsY ||
      contentY >= static_cast<int32_t>(kSettingsActionsY) + 30) {
    return -1;
  }
  if (x >= 6U && x < 314U) return 0;
  return -1;
}

void handleAction(uint16_t x, uint16_t y, uint32_t now) {
  const int tabIndex = tabHitIndex(x, y);
  if (tabIndex >= 0) {
    gTab = static_cast<Tab>(tabIndex);
    gDragKind = DragKind::None;
    gDirty = true;
    return;
  }

  if (gTab == Tab::Usage) {
    gDragKind = DragKind::None;
    return;
  }

  if (gTab == Tab::Settings) {
    const int32_t contentY = settingsContentY(y);
    gDragKind = DragKind::None;
    gDragStartX = x;
    gDragStartY = y;
    gDragStartScroll = clampSettingsScroll(gSettingsScroll);
    if (handleSettingsSlider(x, contentY, now)) {
      return;
    }
    if (handleSettingsScrollBar(x, y)) {
      return;
    }
    gDragKind = DragKind::Scroll;
    const int actionIndex = settingsActionAt(x, contentY);
    if (actionIndex == 0) {
      sendCommand(usage::CommandType::Setup);
    }
    return;
  }

  const int actionIndex = hitIndex(kConnectionActionRects, x, y);
  if (actionIndex == 0) {
    sendCommand(usage::CommandType::Setup);
  } else if (actionIndex == 1) {
    sendCommand(usage::CommandType::Login);
  }
}

// Contact-continuation drag on the Settings tab. Sliders adjust their
// value from the finger X, the scrollbar and empty content scroll
// relatively. Tab-switch taps never reach here with a scroll effect.
void handleDragMove(uint16_t x, uint16_t y, uint32_t now) {
  if (gTab != Tab::Settings) {
    return;
  }
  if (gDragKind == DragKind::SleepMinutes ||
      gDragKind == DragKind::SleepHours ||
      gDragKind == DragKind::PollInterval) {
    // Keep adjusting the same slider while the contact continues, even if
    // the finger drifts off its row.
    if (x < 16U || x > 283U) {
      return;
    }
    if (gDragKind == DragKind::SleepMinutes) {
      const uint32_t minutes = sliderValueFromX(
          x, 0U, DisplayState::kSleepMinutesMax, 1U);
      if (minutes !=
          DisplayState::sleepMinutesPart(gDisplayState.timeout())) {
        applySleepParts(minutes,
                        DisplayState::sleepHoursPart(gDisplayState.timeout()),
                        now);
      }
    } else if (gDragKind == DragKind::SleepHours) {
      const uint32_t hours = sliderValueFromX(
          x, 0U, DisplayState::kSleepHoursMax, 1U);
      if (hours != DisplayState::sleepHoursPart(gDisplayState.timeout())) {
        applySleepParts(DisplayState::sleepMinutesPart(gDisplayState.timeout()),
                        hours, now);
      }
    } else {
      const uint32_t pollSec = sliderValueFromX(
          x, DisplayState::kPollSliderMinSec, DisplayState::kPollSliderMaxSec,
          DisplayState::kPollSliderStepSec);
      if (pollSec != gSnapshot.pollIntervalMs / 1000U) {
        applyPollInterval(pollSec, now);
      }
    }
    return;
  }
  if (gDragKind == DragKind::Scroll) {
    const int32_t delta = gDragStartY - static_cast<int32_t>(y);
    if (delta < 6 && delta > -6) {
      return;
    }
    const int32_t target = clampSettingsScroll(gDragStartScroll + delta);
    if (target != clampSettingsScroll(gSettingsScroll)) {
      gSettingsScroll = target;
      gDirty = true;
    }
  }
}

void updateLocalNotice(uint32_t now) {
  const bool active = localNoticeActive(now);
  if (active != gLastLocalNoticeActive) {
    gLastLocalNoticeActive = active;
    gDirty = true;
  }
}

void updateSnapshot(uint32_t now) {
  if (static_cast<uint32_t>(now - gLastSnapshotAt) < kSnapshotPeriodMs) {
    return;
  }
  gLastSnapshotAt = now;
  usage::Snapshot next;
  if (!usage::networkSnapshot(next)) {
    const bool stale = snapshotIsStale(gSnapshot, now);
    if (stale != gLastStale && gDisplayState.awake()) gDirty = true;
    gLastStale = stale;
    return;  // Retain the last confirmed orientation, timeout and reading on a transient lock
             // timeout.
  }
  const bool stale = snapshotIsStale(next, now);
  const bool changed = snapshotDisplayChanged(gSnapshot, next) || stale != gLastStale;
  gLastStale = stale;
  gSnapshot = next;
  // The Connection tab only shows while unauthenticated. Return to Usage
  // when authentication completes while it is open.
  bool tabHidden = false;
  if (gSnapshot.authenticated && gTab == Tab::Connection) {
    gTab = Tab::Usage;
    tabHidden = true;
  }
  bool orientationChanged = false;
  if (!gDisplayOrientationKnown || gAppliedDisplayFlipped != gSnapshot.displayFlipped) {
    // TFT_eSPIのrotation 1/3が横長の表裏になる。
    gDisplay.setRotation(gSnapshot.displayFlipped ? 3U : 1U);
    gBandDiff.invalidate();
    gAppliedDisplayFlipped = gSnapshot.displayFlipped;
    gDisplayOrientationKnown = true;
    orientationChanged = true;
  }
  // 起動時に worker が読み込んだ保存済み設定を画面側にも反映する。
  // 変更がない場合はタイマーをリセットしない。
  bool timeoutChanged = false;
  if (gSnapshot.timeoutMs != gDisplayState.timeout()) {
    if (gDisplayState.setTimeout(gSnapshot.timeoutMs, now)) {
      timeoutChanged = true;
    }
  }
  if ((changed || timeoutChanged || orientationChanged || tabHidden) &&
      gDisplayState.awake()) {
    gDirty = true;
  }
  // 取得残り秒数を毎秒進めるため、取得がなくても秒の変わり目で再描画する。
  const int32_t countdownKey = pollCountdownSec(gSnapshot, now);
  if (countdownKey != gLastCountdownKey) {
    gLastCountdownKey = countdownKey;
    if (gDisplayState.awake()) {
      gDirty = true;
    }
  }
  // 残り時間の分表示を進めるため、取得がなくても分の変わり目で再描画する。
  const bool hasReset = (gSnapshot.fiveHour.available && gSnapshot.fiveHour.resetsAt > 0) ||
                        (gSnapshot.weekly.available && gSnapshot.weekly.resetsAt > 0);
  const int64_t epochNow = static_cast<int64_t>(time(nullptr));
  if (hasReset && epochNow >= usage::kResetClockReadyEpoch) {
    const uint32_t bucket = static_cast<uint32_t>(epochNow / 60);
    if (bucket != gLastResetMinute) {
      gLastResetMinute = bucket;
      if (gTab == Tab::Usage && gDisplayState.awake()) {
        gDirty = true;
      }
    }
  }
}

void syncAwake() {
  const bool awake = gDisplayState.awake();
  if (awake == gLastAwake) {
    return;
  }

  gLastAwake = awake;
  gDisplay.setBrightness(awake ? 255U : 0U);
  if (awake) {
    gBandDiff.invalidate();
    gDirty = true;
  }
}

}  // namespace

void setup() {
#ifdef USAGE_DIAGNOSTICS
  usage::diagnostics::begin();
  Serial.println("{\"phase\":\"display-init\"}");
#endif
  const uint32_t now = millis();
  gDisplay.init();
  gDisplay.setRotation(1);
  gDisplay.beginTouch();
  gCanvas.setColorDepth(8U);
  gCanvasReady = gCanvas.createSprite(kFrameWidth, kFrameHeight) != nullptr;
  gCanvasFailure = !gCanvasReady;
  if (gCanvasReady) {
    gCanvas.setTextSize(1U);
    gCanvas.setTextWrap(false, false);
  }
#ifdef USAGE_DIAGNOSTICS
  Serial.print("{\"phase\":\"framebuffer-ready\",\"ok\":");
  Serial.print(gCanvasReady ? "true" : "false");
  Serial.print(",\"bytes\":");
  Serial.print(gCanvasReady ? kFrameBufferBytes : 0U);
  Serial.println("}");
#endif
  // TFT_eSPIのrotation 1で320x240の横長を選ぶ。
  gDisplay.setBrightness(255U);
  gDisplayState.setTimeout(gDisplayState.timeout(), now);
  pinMode(kBootPin, INPUT_PULLUP);
  gBootButton.begin(digitalRead(kBootPin) == LOW, now);
  gLastAwake = true;
  gLastSnapshotAt = static_cast<uint32_t>(now - kSnapshotPeriodMs);
  usage::startNetwork();
#ifdef USAGE_DIAGNOSTICS
  Serial.println("{\"phase\":\"ready\"}");
#endif
  drawFrame(now);
}

void loop() {
#ifdef USAGE_DIAGNOSTICS
  usage::diagnostics::poll();
  const uint32_t now = usage::diagnostics::now();
#else
  const uint32_t now = millis();
#endif
  gDisplayState.tick(now);

  bool bootPressed = digitalRead(kBootPin) == LOW;
#ifdef USAGE_DIAGNOSTICS
  usage::diagnostics::bootOverride(bootPressed);
#endif
  const BootAction bootAction = gBootButton.update(bootPressed, now);
  if (bootAction != BootAction::None) {
    gDisplayState.wake(now);
    if (bootAction == BootAction::Calibrate) {
      gDisplay.calibrateTouch();
      gBandDiff.invalidate();
      gDisplayState.wake(millis());
      gDirty = true;
    } else if (sendCommand(usage::CommandType::FlipDisplay)) {
      setLocalNotice(String("表示向きを切替中"), now);
    }
  }

  uint16_t touchX = 0U;
  uint16_t touchY = 0U;
  bool pressed = gDisplay.getTouch(&touchX, &touchY);
#ifdef USAGE_DIAGNOSTICS
  usage::diagnostics::touchOverride(touchX, touchY, pressed);
  if (usage::diagnostics::redrawRequested()) gDirty = true;
#endif
  if (gDisplayState.touch(pressed, now)) {
    handleAction(touchX, touchY, now);
  }
  // Contact continuation drags on the Settings tab. Taps are unaffected:
  // only a held contact following a handled tap reaches here.
  if (pressed && gPrevPressed && gDisplayState.awake()) {
    uint16_t dragX = 0U;
    uint16_t dragY = 0U;
    if (gDisplay.getDragPoint(&dragX, &dragY)) {
      handleDragMove(dragX, dragY, now);
    }
  }
  if (!pressed) {
    gDragKind = DragKind::None;
  }
  gPrevPressed = pressed;

  syncAwake();
  updateSnapshot(now);
  updateLocalNotice(now);
  if (gDirty && gDisplayState.awake()) {
    drawFrame(now);
  }
#ifdef USAGE_DIAGNOSTICS
  usage::diagnostics::finishFrame(gDisplay, gDisplayState, gSnapshot, gCanvasReady);
#endif
  yield();
}
