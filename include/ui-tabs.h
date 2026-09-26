#pragma once
#include <stdint.h>

// 使用量・設定・接続タブの表示規則を抜き出した純粋関数群。
// main.cpp の描画側と native テストで共有する。
// - 接続タブは未認証時だけでなく、認証コード待ち（再ログイン含む）にも表示する。
//   v0.4.0 は認証済みを 2 タブにしていたため、認証済みの再ログインで
//   新しいコードが LCD に出ず、状態表示の促しだけが残って見えた。
// - 接続タブを開いているときに認証が完了したら使用量タブへ戻す。
//   ただしコード待ちの間は接続タブに留め、コード確認を妨げない。
// - 取得までの残り秒数はコード待ちの促しに付けない。
//   次回使用量取得までの秒数であり、認証コードの有効期限ではないためである。
namespace ui_tabs {

inline uint8_t tabCount(bool authenticated, bool hasDeviceCode) {
  return (authenticated && !hasDeviceCode) ? 2U : 3U;
}

inline bool shouldReturnToUsage(bool authenticated, bool hasDeviceCode,
                                uint8_t tab) {
  return authenticated && !hasDeviceCode && tab == 2U;
}

inline bool shouldShowCountdown(bool fetching, uint32_t nextPollMs,
                                bool hasDeviceCode) {
  return !fetching && nextPollMs != 0U && !hasDeviceCode;
}

}  // namespace ui_tabs
