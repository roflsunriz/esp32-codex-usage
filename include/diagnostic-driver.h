#ifndef CODEX_USAGE_DIAGNOSTIC_DRIVER_H
#define CODEX_USAGE_DIAGNOSTIC_DRIVER_H

#ifdef USAGE_DIAGNOSTICS

#include <Arduino.h>
#include <stdint.h>

#include <LovyanGFX.hpp>

#include "app-model.h"
#include "display-state.h"

namespace usage {
namespace diagnostics {

void begin();
void poll();
uint32_t now();

// x/y/pressed は呼び出し前に実機タッチの値を渡す。合成タッチが有効なら
// 値を置き換えて true を返し、実機の押下が観測された場合は合成値を解除する。
bool touchOverride(uint16_t& x, uint16_t& y, bool& pressed);

// 実機押下を優先し、合成 BOOT 入力が有効なら pressed を置き換える。
bool bootOverride(bool& pressed);

// redraw コマンドで立てられたフラグを一度だけ返す。
bool redrawRequested();

// 画面描画後に、保留中コマンドへの応答を送信する。
void finishFrame(lgfx::LGFX_Device& display, const DisplayState& state,
                 const usage::Snapshot& snapshot, bool buffered);

}  // namespace diagnostics
}  // namespace usage

#endif  // USAGE_DIAGNOSTICS

#endif  // CODEX_USAGE_DIAGNOSTIC_DRIVER_H
