import re

UNDEFINED_SYMBOL = re.compile("undefined symbol: (\w+)")

skip_txt = ""
for file in ["sym_ewram.txt", "sym_bss.txt"]:
    with open(f"../../decomp/pokeruby/{file}", "r") as f:
        skip_txt += f.read()


with open("linker.out", "r") as f:
    for line in f.readlines():
        match = re.search(UNDEFINED_SYMBOL, line)
        if not match:
            continue
        symbol = match.group(1)
        if symbol.startswith("_"):
            symbol = symbol[1:]

        if symbol in skip_txt:
            continue

        print(f"#define {symbol} _{symbol}")

        