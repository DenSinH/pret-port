import sys
import re

INCBIN_TYPES = ["INCBIN_S8", "INCBIN_U8", "INCBIN_S16", "INCBIN_U16", "INCBIN_S32", "INCBIN_U32"]


if __name__ == '__main__':
    _, source_list_file, header_list_file = sys.argv
    # source_list_file, header_list_file = [
    #     r"D:\Projects\CProjects\pokeruby-port\cmake-build-debug\temp\source_list.txt",
    #     r"D:\Projects\CProjects\pokeruby-port\cmake-build-debug\temp\header_list.txt"
    # ]

    # read source and header files
    with open(source_list_file, "r") as f:
        sources = [line.strip() for line in f.readlines() if not line.startswith("#")]

    with open(header_list_file, "r") as f:
        headers = [line.strip() for line in f.readlines() if not line.startswith("#")]

    for file in sources + headers:
        print(f"#{file}")
        with open(file, "r", errors="ignore") as f:
            text = f.read()

        for type in INCBIN_TYPES:
            for match in re.findall(f"{type}\s*\(\s*\"(.*?)\"\s*\)", text):
                for file in re.split(r"\"\s*,\s*\"", match):
                    print(file)
