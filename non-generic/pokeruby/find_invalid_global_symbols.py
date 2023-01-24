import os
import re


INVALID_GLOBAL_SYMBOL_PATTERN = re.compile(r"\.global\s+(L[\w]*)")

if __name__ == '__main__':
    base = r"D:\Projects\CProjects\pokeruby-port\cmake-build-debug-visual-studio---x86\asm-data-preproc"
    preproc_ext = ".s.tmp.s"

    invalid_labels = []

    for root, dir, files in os.walk(base, topdown=True):
        for file in files:
            if not file.endswith(preproc_ext):
                continue

            with open(os.path.join(root, file), "r") as f:
                lines = f.readlines()

            for line in lines:
                for match in re.findall(INVALID_GLOBAL_SYMBOL_PATTERN, line):
                    print(match)
                    invalid_labels.append(match)

    with open("./invalid_labels_fix.h", "w+") as f:
        f.write("#pragma once\n\n")
        for label in invalid_labels:
            f.write(f"#define {label} _{label}\n")
