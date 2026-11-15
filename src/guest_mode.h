#pragma once
#include <stdint.h>

// KEY0 wakes the E1002 through EXT1; duration is sampled after startup.
// Hold roughly 2–3 seconds to switch modes; a tap always refreshes.
static constexpr uint32_t KEY0_MODE_HOLD_MS=1800;
enum class Key0Action : uint8_t { Auto=0, Refresh=1, SwitchMode=2 };
inline Key0Action classifyKey0(bool keyWake,bool heldForThreshold){
  if(!keyWake)return Key0Action::Auto;
  return heldForThreshold?Key0Action::SwitchMode:Key0Action::Refresh;
}
inline bool guestModeAfterAction(bool current,Key0Action action){
  return action==Key0Action::SwitchMode?!current:current;
}
