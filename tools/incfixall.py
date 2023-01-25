from pymakeparser.pymakeparser import Makefile
import sys
import os
import re
import subprocess
import shutil
import shlex
import concurrent.futures as fut
import threading
import json
import datetime
from contextlib import nullcontext

# setup environment and global arguments
PARALLEL = True
_, gbagfx, rsfont, jsonproc, mapjson, aif2pcm, mid2agb, base_dir, dest_dir, mk_file, incbin_list_file = sys.argv


env = dict(os.environ)
# assert "RSFONT" not in env
# env["RSFONT"] = "RSFONT"
# assert "GBAGFX" not in env
# env["GBAGFX"] = "GBAGFX"
# assert "JSONPROC" not in env
# env["JSONPROC"] = "JSONPROC"
# assert "MAPJSON" not in env
# env["MAPJSON"] = "MAPJSON"
assert "MAKE" not in env
env["MAKE"] = "MAKE"

GFXFILES = re.compile(r"^(.*)\.(1bpp|4bpp|8bpp|gbapal|lz|rl)$")
IGNORE_TARGET_NAMES = [
    "tidy",
    "tools",
    "mostlyclean",
    "clean",
    ".PHONY",
]
IGNORE_TARGETS = re.compile(f"^((.*)\\.(o|ld|elf|gba|sym)|{'|'.join(IGNORE_TARGET_NAMES)})$")

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


def is_out_of_date(target, *sources):
    if not sources:
        return True

    return max(os.path.getmtime(source) for source in sources) > os.path.getmtime(os.path.join(dest_dir, target))


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
    if os.path.exists(os.path.join(base_dir, src)):
        # source file can just be read from base_dir
        pass
    elif os.path.exists(os.path.join(dest_dir, src)):
        # have to read source file from dest_dir (binary dir)
        src = os.path.join(dest_dir, src)
    elif re.match(GFXFILES, src):
        # no variables given for src, assume empty variables
        run_gbagfx(src, {})
        src = os.path.join(dest_dir, src)

    # check if source files have been modified
    if not is_out_of_date(target, src):
        return

    cmd = [gbagfx, src, out_file]
    if GFX_OPTS:
        cmd += GFX_OPTS.split(" ")

    run_proc(cmd, cwd=base_dir)


def run_aif2pcm(target, compress):
    base, ext = os.path.splitext(target)
    command = [aif2pcm, find_file(f"{base}.aif"), get_out_file(target)]

    # check if source files have been modified
    if not is_out_of_date(target, command[1]):
        return

    if compress:
        command.append("--compress")
    run_proc(command, cwd=base_dir)


def _run_command(target, command, cwd=base_dir):
    if command[0] == "cat":
        if command[-2] != ">" and not command[-1].startswith(">"):
            raise Exception("Expected cat <file>+ > <dest>")

        files = command[1:-1]
        if files[-1] == ">":
            files.pop()
        files = [find_file(file) for file in files]

        if not is_out_of_date(target.name, *files):
            return

        dest = get_out_file(command[-1].strip("> "))

        with open(dest, "wb+") as dest:
            for gpath in files:
                with open(gpath, "rb") as g:
                    dest.write(g.read())
    elif "gbagfx" in command[0]:
        # fix parameters
        command[0] = gbagfx
        if not os.path.exists(os.path.join(base_dir, command[1])):
            # source file not in base directory
            if os.path.exists(os.path.join(dest_dir, command[1])):
                command[1] = os.path.join(dest_dir, command[1])
            else:
                raise FileNotFoundError(f"Cannot find graphics source file {command[1]}")
        command[2] = get_out_file(command[2])

        if not is_out_of_date(target.name, command[1]):
            return
        run_proc(command, cwd=cwd)
    elif "rsfont" in command[0]:
        command[0] = rsfont

        # resolve source file
        command[1] = find_file(command[1])
        command[2] = get_out_file(command[2])
        if not is_out_of_date(target.name, command[1]):
            return
        run_proc(command, cwd=cwd)
    elif "jsonproc" in command[0]:
        command[0] = jsonproc
        command[1] = find_file(command[1])
        command[2] = find_file(command[2])
        command[3] = get_out_file(command[3])
        if not is_out_of_date(target.name, command[1], command[2]):
            return
        run_proc(command, cwd=cwd)
    elif "mapjson" in command[0]:
        command[0] = mapjson
        command[3] = find_file(command[3])
        # if not is_out_of_date(target.name, command[3]):
        #     return
        run_proc(command, cwd=cwd)
    elif "mid2agb" in command[0]:
        command[0] = mid2agb
        if not os.path.exists(os.path.join(cwd, command[1])):
            try:
                find_file(target.name)

                # output file already exists
                return
            except FileNotFoundError:
                raise FileNotFoundError(f"Input file for mid2agb not found while target {target.name} does not exist:"
                                        f" {command[1]}")
        if not is_out_of_date(target.name, command[1]):
            return
        run_proc(command, cwd=cwd)
    elif command[0] == "MAKE":
        print(f"Skipping make target {target.name}")
    elif command[0] == "cd":
        cwd = os.path.join(base_dir, command[1])
        if not os.path.exists(cwd):
            raise FileNotFoundError(f"Invalid cwd: {cwd} while processing target {target.name}")

        if command[2] == "&&":
            _run_command(target, command[3:], cwd=cwd)
    else:
        print(command)
        raise ValueError(f"Unknown command for processing file {target.name} rule: {command}")


def fix_rules_target(target):
    """
    Generate a target with a rule
    """
    if len(target.rules) > 1:
        raise Exception("Expected only one rule for graphics targets")
    rule = target.rules[0]
    if len(rule.commands) == 0:
        # simply a dependency, no rule
        return
    if len(rule.commands) > 1:
        raise Exception(f"Expected 1 command for target {target.name} rules (got {len(rule.commands)})")

    command = shlex.split(rule.commands[0])
    _run_command(target, command)


def fix_makefile_targets(fpath):
    print(f"Processing targets from {fpath}")

    # set cwd to decomp base dir for pathsubst wildcards to work
    os.chdir(base_dir)
    mkfile = Makefile(fpath, env=env)

    def _fix_rules_target(target):
        fix_rules_target(target)
        return target

    def _fix_norules_target(target):
        run_gbagfx(target.name, target.variables)
        return target

    targets_fixed = set()

    with fut.ThreadPoolExecutor(max_workers=16) if PARALLEL else nullcontext() as pool:

        for layer in mkfile.topsort():
            futures = []
            for target in layer:
                print(f"Makefile target {target.name}")

                if re.match(IGNORE_TARGETS, target.name):
                    print(f"Ignoring target {target.name}")
                    continue

                if target.rules:
                    if PARALLEL:
                        futures.append(pool.submit(_fix_rules_target, target))
                    else:
                        targets_fixed.add(_fix_rules_target(target))
                elif os.path.exists(os.path.join(base_dir, target.name)):
                    print(f"Target {target.name} already exists as a source")
                    targets_fixed.add(target.name)
                # graphics might have been overwritten, so don't check this
                # elif os.path.exists(os.path.join(dest_dir, target.name)):
                #     print(f"Target {target.name} has already been processed")
                else:
                    # only parse gfx file types without rules
                    if not re.match(GFXFILES, target.name):
                        print(f"Target has no rules and is not a graphics file: {target.name}")
                        continue

                    if PARALLEL:
                        futures.append(pool.submit(_fix_norules_target, target))
                    else:
                        targets_fixed.add(_fix_norules_target(target))

            if PARALLEL:
                for future in fut.as_completed(futures):
                    if future.exception():
                        raise future.exception()
                    target = future.result()
                    print(f"Created makefile target {target.name}")
                    targets_fixed.add(target.name)

    return targets_fixed


if __name__ == '__main__':
    """ Fix makefile targets """

    targets_fixed = fix_makefile_targets(mk_file)

    """ Fix direct incbin targets """
    with open(incbin_list_file, "r") as f:
        incbins = [line.strip() for line in f.readlines() if not line.startswith("#")]

    futures = []
    with fut.ThreadPoolExecutor(max_workers=16) as pool:
        for fpath in incbins:
            if fpath in targets_fixed:
                continue

            src_file = os.path.join(base_dir, fpath)
            if os.path.exists(src_file):
                shutil.copyfile(src_file, get_out_file(fpath))
                continue

            base, ext = os.path.splitext(fpath)
            if ext.lower() == ".bin":
                print(f"Missing include .bin file: {fpath}")

            if not re.match(GFXFILES, fpath):
                if re.match(r"sound/direct_sound_samples/cries/cry_(.*)\.bin", fpath) and \
                        os.path.exists(os.path.join(base_dir, f"{base}.aif")):
                    print(f"Incbin aif2pcm compressed target {fpath}")
                    futures.append(pool.submit(run_aif2pcm, fpath, True))
                elif re.match(r"sound/(.*)\.bin", fpath) and os.path.exists(os.path.join(base_dir, f"{base}.aif")):
                    print(f"Incbin aif2pcm target {fpath}")
                    futures.append(pool.submit(run_aif2pcm, fpath, False))
            else:
                print(f"Incbin gfx target {fpath}")
                futures.append(pool.submit(run_gbagfx, fpath, {}))

        for future in fut.as_completed(futures):
            if future.exception():
                raise future.exception()
            future.result()
