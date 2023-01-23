from pymakeparser.pymakeparser import Makefile
import sys
import os
import re
import subprocess
import shutil
import shlex
import concurrent.futures as fut

# setup environment and global arguments
_, gbagfx, rsfont, jsonproc, mapjson, base_dir, dest_dir, mkfile_list_file, incbin_list_file = sys.argv

env = dict(os.environ)
assert "RSFONT" not in env
env["RSFONT"] = "RSFONT"
assert "GBAGFX" not in env
env["GBAGFX"] = "GBAGFX"
assert "JSONPROC" not in env
env["JSONPROC"] = "JSONPROC"
assert "MAPJSON" not in env
env["MAPJSON"] = "MAPJSON"

# todo: make this nongeneric
# missing variables in makefile
env["DATA_SRC_SUBDIR"] = "src/data"

GFXFILES = re.compile(r"^(.*)\.(1bpp|4bpp|8bpp|gbapal|lz|rl|bin)$")

# todo: non-generic?
EXT_SOURCE = {
    "1bpp": "png",
    "4bpp": "png",
    "8bpp": "png",
    "gbapal": ["pal", "png"],  # prioritize .pal files for .gbapal output
    "lz": "",
    "rl": "",
    # bin is not valid for GFXFIX, but it can be a makefile target for cat
}


def get_out_file(target):
    out_file = os.path.join(dest_dir, target)
    if not os.path.exists(os.path.dirname(out_file)):
        os.makedirs(os.path.dirname(out_file), exist_ok=True)
    return out_file


def ext_if(base, ext):
    if ext:
        return f"{base}.{ext}"
    else:
        # rl / lz
        return base


def find_file(file):
    if os.path.exists(os.path.join(base_dir, file)):
        return os.path.join(base_dir, file)
    elif os.path.exists(os.path.join(dest_dir, file)):
        return os.path.join(dest_dir, file)
    else:
        raise FileNotFoundError(f"Could not find file in sources or intermediates: {file}")



def run_proc(cmd, cwd):
    proc = subprocess.Popen(
        cmd,
        stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        cwd=cwd
    )

    out, err = proc.communicate()

    if out:
        print(out.decode())

    if proc.returncode or err:
        raise Exception(f"Error running command {cmd[0]} (returncode {proc.returncode}):\n\n{err.decode()}")


def run_gbagfx(target, variables):
    # get source file
    match = re.match(GFXFILES, target)
    base, ext = match.groups()
    if isinstance(EXT_SOURCE[ext], str):
        src = ext_if(base, EXT_SOURCE[ext])
    elif isinstance(EXT_SOURCE[ext], list):
        if len(EXT_SOURCE[ext]) == 1:
            src = ext_if(base, EXT_SOURCE[ext][0])
        else:
            for src_ext in EXT_SOURCE[ext]:
                if os.path.exists(os.path.join(base_dir, f"{base}.{src_ext}")):
                    src = ext_if(base, src_ext)
                    break
            else:
                raise FileNotFoundError(f"No source file found for {target}")
    else:
        raise TypeError(f"Invalid source extension type for target: {target} (got {type(EXT_SOURCE[ext])})")

    # call GBAGFX
    GFX_OPTS = variables.get("GFX_OPTS", "").strip()
    out_file = get_out_file(target)

    # files might have to be recursively generated
    if re.match(GFXFILES, src):
        if os.path.exists(os.path.join(base_dir, src)):
            # source file can just be read from base_dir
            pass
        elif os.path.exists(os.path.join(dest_dir, src)):
            # have to read source file from dest_dir (binary dir)
            src = os.path.join(dest_dir, src)
        else:
            # no variables given for src, assume empty variables
            run_gbagfx(src, {})
            src = os.path.join(dest_dir, src)

    cmd = [gbagfx, src, out_file]
    if GFX_OPTS:
        cmd += GFX_OPTS.split(" ")

    run_proc(cmd, cwd=base_dir)
    print(f"Finished processing {target}")


def fix_rules_target(target):
    """
    Generate a target with a rule
    """
    if len(target.rules) > 1:
        raise Exception("Expected only one rule for graphics targets")
    rule = target.rules[0]
    if len(rule.commands) != 1:
        raise Exception(f"Expected 1 command for graphics target rules (got {len(rule.commands)}")

    command = shlex.split(rule.commands[0])

    if command[0] == "cat":
        if command[-2] != ">" and not command[-1].startswith(">"):
            raise Exception("Expected cat <file>+ > <dest>")

        files = command[1:-1]
        if files[-1] == ">":
            files.pop()

        dest = get_out_file(command[-1].strip("> "))

        with open(dest, "wb+") as dest:
            for file in files:
                gpath = find_file(file)

                with open(gpath, "rb") as g:
                    dest.write(g.read())
    elif command[0] == "GBAGFX":
        # fix parameters
        command[0] = gbagfx
        if not os.path.exists(os.path.join(base_dir, command[1])):
            # source file not in base directory
            if os.path.exists(os.path.join(dest_dir, command[1])):
                command[1] = os.path.join(dest_dir, command[1])
            else:
                raise FileNotFoundError(f"Cannot find graphics source file {command[1]}")
        command[2] = get_out_file(command[2])

        run_proc(command, cwd=base_dir)
    elif command[0] == "RSFONT":
        command[0] = rsfont

        # resolve source file
        command[1] = find_file(command[1])
        command[2] = get_out_file(command[2])
        run_proc(command, cwd=base_dir)
    elif command[0] == "JSONPROC":
        command[0] = jsonproc
        command[1] = find_file(command[1])
        command[2] = find_file(command[2])
        command[3] = get_out_file(command[3])
        run_proc(command, cwd=base_dir)
    elif command[0] == "MAPJSON":
        command[0] = mapjson
        command[3] = find_file(command[3])
        print(" ".join(command))
        run_proc(command, cwd=base_dir)
    else:
        print(command)
        raise ValueError(f"Unknown command for processing file {target.name} rule: {command}")


def fix_makefile_targets(fpath):
    print(f"Processing targets from {fpath}")
    mkfile = Makefile(fpath, env=env)

    def _fix_rules_target(target):
        fix_rules_target(target)
        return target

    def _fix_norules_target(target):
        run_gbagfx(target.name, target.variables)
        return target


    targets_fixed = set()
    with fut.ThreadPoolExecutor(max_workers=4) as pool:

        for layer in mkfile.topsort():
            futures = []
            for target in layer:
                print(f"Makefile target {target.name}")

                if target.rules:
                    futures.append(pool.submit(_fix_rules_target, target))
                elif os.path.exists(os.path.join(base_dir, target.name)):
                    print(f"Target {target.name} already exists as a source")
                # graphics might have been overwritten, so don't check this
                # elif os.path.exists(os.path.join(dest_dir, target.name)):
                #     print(f"Target {target.name} has already been processed")
                else:
                    # only parse gfx file types without rules
                    match = re.match(GFXFILES, target.name)
                    if not match:
                        continue
                    futures.append(pool.submit(_fix_norules_target, target))

            for future in fut.as_completed(futures):
                if future.exception():
                    raise future.exception()
                target = future.result()
                targets_fixed.add(target.name)

    return targets_fixed


if __name__ == '__main__':
    """ Fix makefile targets """
    with open(mkfile_list_file, "r") as f:
        makefiles = [line.strip() for line in f.readlines() if not line.startswith("#")]

    targets_fixed = set()
    futures = []
    with fut.ProcessPoolExecutor(max_workers=4) as pool:
        for fpath in makefiles:
            futures.append(pool.submit(fix_makefile_targets, fpath))

    for future in fut.as_completed(futures):
        if future.exception():
            raise future.exception()
        targets_fixed |= future.result()

    """ Fix direct incbin targets """
    with open(incbin_list_file, "r") as f:
        incbins = [line.strip() for line in f.readlines() if not line.startswith("#")]

    futures = []
    with fut.ThreadPoolExecutor(max_workers=4) as pool:
        for fpath in incbins:
            print(f"Incbin target {fpath}")
            if fpath in targets_fixed:
                continue

            src_file = os.path.join(base_dir, fpath)
            if os.path.exists(src_file):
                shutil.copyfile(src_file, get_out_file(fpath))
                continue

            if not re.match(GFXFILES, fpath):
                continue
            futures.append(pool.submit(run_gbagfx, fpath, {}))

        for future in fut.as_completed(futures):
            if future.exception():
                raise future.exception()
            future.result()
