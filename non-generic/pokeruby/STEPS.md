## Some extra steps

There were some occurrences of `.endc` in assembly files instead of `.endif`.

A major change is finding and replacing `\.equ\s+(.*?)_grp\s*,` with `#define $1_grp` in `sound/*.s` files.
The problem is that the `.equ` directive does not seem to work for text replacements.


TODO: in `battle_7.s` a bunch of stuff is commented out relating to `gSharedMem`.