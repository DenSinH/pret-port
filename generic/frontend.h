#pragma once

#include "helpers.h"

namespace frontend {

static constexpr int Scale = 2;
static constexpr int GbaWidth = 240;
static constexpr int GbaHeight = 160;

extern u16 Keypad;

void InitFrontend();
void RunFrame();

}