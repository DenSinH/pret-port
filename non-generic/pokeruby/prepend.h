#pragma once

// there is a struct field named errno in
// src/pokedex_area_screen.c:53:12
#include <string.h>
#include <stdlib.h>
#include <limits.h>

#undef errno
