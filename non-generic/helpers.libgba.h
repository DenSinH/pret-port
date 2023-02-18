#pragma once

#include "helpers.h"

namespace nongeneric {

void HandleInterrupt(Interrupt index);
bool HasHBlankCallback();
void DoHBlankCallback();
bool HasVCountCallback();
void DoVCountCallback();

}