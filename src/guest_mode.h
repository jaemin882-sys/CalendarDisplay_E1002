#pragma once
#include <stdint.h>

// KEY0 wakes the E1002 through EXT1. The ROM/bootloader consumes some of
// the physical press before setup can observe it. Sample immediately at the
// start of setup; a further 900 ms hold is considered long. In practice,
// hold the button around 3 seconds to ensure reliable switching.
static constexpr uint32_t KEY0_MODE_HOLD_MS=900;
enum class Key0Action : uint8_t { Auto=0, Refresh=1, SwitchMode=2 };
inline Key0Action classifyKey0(bool keyWake,bool heldForThreshold){
  if(!keyWake)return Key0Action::Auto;
  return heldForThreshold?Key0Action::SwitchMode:Key0Action::Refresh;
}
inline bool guestModeAfterAction(bool current,Key0Action action){
  return action==Key0Action::SwitchMode?!current:current;
}
