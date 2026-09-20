#pragma once

#include "msxgl.h"

void Splash_Detect(void);
void Splash_Show(u8 unapi_miss);
void Splash_Pause(void);
#if defined(TT_ROM_DBG)
void Splash_Dbg(u8 code);
#endif
