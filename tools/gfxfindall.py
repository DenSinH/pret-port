import sys
import re
import os
import concurrent.futures as fut


C_INCBIN_TYPES = ["INCBIN_S8", "INCBIN_U8", "INCBIN_S16", "INCBIN_U16", "INCBIN_S32", "INCBIN_U32"]
ASM_INCBIN_TYPES = [".incbin"]


def search_file(fpath):
    incbins_found = []

    with open(fpath, "r", errors="ignore") as f:
        text = f.read()

    ext = os.path.splitext(fpath)[1].lower()
    if ext in {".c", ".cpp", ".h"}:
        for type in C_INCBIN_TYPES:
            for match in re.findall(f"{type}\s*\(\s*\"(.*?)\"\s*\)", text):
                for file in re.split(r"\"\s*,\s*\"", match):
                    incbins_found.append(file)
    elif ext in {".s", ".inc"}:
        for type in ASM_INCBIN_TYPES:
            for match in re.findall(f"{type}\s*\"(.*?)\"", text):
                for file in re.split(r"\"\s*,\s*\"", match):
                    incbins_found.append(file)
    else:
        return fpath, []

    return fpath, incbins_found


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

    futures = []
    with fut.ProcessPoolExecutor() as pool:
        for file in sources + headers:
            futures.append(pool.submit(search_file, file))

        for future in fut.as_completed(futures):
            if future.exception() is not None:
                raise future.exception()
            fpath, incbins_found = future.result()

            if incbins_found:
                print(f"#{fpath}")
                for incbin in incbins_found:
                    print(incbin)
