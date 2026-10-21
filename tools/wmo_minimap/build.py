import argparse
import hashlib
import multiprocessing
import os
import re
import sys
import time

import mpyq

import blp
import mpq
import wmo

TRS_PATH = "textures\\Minimap\\md5translate.trs"
WMO_PREFIX = "world\\wmo\\"
TILE_ENTRY = re.compile(r"^(WMO\\.+)_(\d{3})_\d{2}_\d{2}\.blp$", re.I)
GROUP_FILE = re.compile(r"_\d{3}\.wmo$", re.I)

_data = None


def tile_name(wmo_path, group, tx, ty):
    return "WMO\\%s_%03d_%02d_%02d.blp" % (wmo_path[len(WMO_PREFIX):-4], group, tx, ty)


def covered_groups(trs_text):
    groups = set()
    for line in trs_text.splitlines():
        if "\t" in line:
            match = TILE_ENTRY.match(line.split("\t", 1)[0])
            if match:
                groups.add((match.group(1).lower(), int(match.group(2))))
    return groups


def wmo_roots(archive_path):
    archive = mpyq.MPQArchive(archive_path, listfile=True)
    names = (f.decode("latin1") for f in archive.files or [])
    return sorted({n for n in names
                   if n.lower().startswith(WMO_PREFIX) and n.lower().endswith(".wmo") and not GROUP_FILE.search(n)})


def missing_groups(data, roots, covered):
    work = []
    for path in roots:
        raw = data.read(path)
        if not raw:
            continue
        key = ("WMO\\" + path[len(WMO_PREFIX):-4]).lower()
        groups = [g for g in wmo.Root(raw).indoor_groups() if (key, g) not in covered]
        if groups:
            work.append((path, groups))
    return work


def merged_trs(base, entries):
    newline = "\r\n" if "\r\n" in base else "\n"
    lines = base.rstrip("\r\n").split(newline)
    for folder, items in sorted(entries.items()):
        lines.append("dir: " + folder)
        for name, digest in sorted(items):
            stem = name[:-4]
            for variant in dict.fromkeys([name, stem.upper() + ".blp", stem.upper() + ".BLP"]):
                lines.append(f"{variant}\t{digest}")
    return (newline.join(lines) + newline).encode("latin1")


def _init_worker(data_dir, exclude):
    global _data
    _data = wmo.ClientData(data_dir, exclude)


def render_wmo(item):
    path, groups = item
    folder = "WMO\\" + path[len(WMO_PREFIX):-4].rsplit("\\", 1)[0]
    tiles, errors = [], []
    root_raw = _data.read(path)
    if not root_raw:
        return folder, tiles, [f"{path}: root unreadable"]
    root = wmo.Root(root_raw)
    for g in groups:
        try:
            raw = _data.read(path[:-4] + "_%03d.wmo" % g)
            if not raw:
                errors.append(f"{path}:{g} unreadable")
                continue
            color, covered, nx, ny = wmo.render_group(_data, root, wmo.Group(raw))
            for tx, ty, tile, mask in wmo.tiles_of(color, covered, nx, ny):
                if mask.any():
                    payload = blp.write_blp_dxt1(tile * 255.0, mask)
                    tiles.append((tile_name(path, g, tx, ty), hashlib.md5(payload).hexdigest() + ".blp", payload))
        except Exception as error:
            errors.append(f"{path}:{g} {type(error).__name__}: {error}")
    return folder, tiles, errors


def main(argv=None):
    parser = argparse.ArgumentParser(description="Indoor minimap tiles for WMOs that ship without them")
    parser.add_argument("--data", required=True, help="client Data folder")
    parser.add_argument("--source", nargs="+", required=True, help="archives in --data whose WMOs are covered")
    parser.add_argument("--out", required=True, help="patch archive to write, e.g. patch-ZM.MPQ")
    parser.add_argument("--only", nargs="*", default=[], help="limit to WMO paths containing these strings")
    parser.add_argument("--workers", type=int, default=max(1, (os.cpu_count() or 2) - 2))
    args = parser.parse_args(argv)

    exclude = [os.path.basename(args.out)]
    data = wmo.ClientData(args.data, exclude)
    found = data.read_with_source(TRS_PATH)
    if not found:
        print(f"no {TRS_PATH} in {args.data}", file=sys.stderr)
        return 2
    trs_source, trs_raw = found
    base = trs_raw.decode("latin1")
    covered = covered_groups(base)

    roots = [r for source in args.source for r in wmo_roots(os.path.join(args.data, source))]
    if args.only:
        roots = [r for r in roots if any(o.lower() in r.lower() for o in args.only)]
    work = missing_groups(data, roots, covered)
    print(f"{TRS_PATH} from {trs_source}; {len(work)} WMOs with {sum(len(g) for _, g in work)} indoor groups "
          f"without tiles", flush=True)

    files, entries, failures = {}, {}, []
    started = time.time()
    with multiprocessing.Pool(args.workers, initializer=_init_worker, initargs=(args.data, exclude)) as pool:
        for n, (folder, tiles, errors) in enumerate(pool.imap_unordered(render_wmo, work), 1):
            failures.extend(errors)
            for name, digest, payload in tiles:
                files["textures\\Minimap\\" + digest] = payload
                entries.setdefault(folder, []).append((name, digest))
            if n % 50 == 0:
                print(f"{n}/{len(work)} WMOs, {len(files)} tiles, {time.time() - started:.0f}s", flush=True)
    for failure in failures:
        print("failed:", failure)

    files[TRS_PATH] = merged_trs(base, entries)
    files[wmo.MARKER_PATH] = f"wmo_minimap: indoor tiles for {', '.join(args.source)} on {trs_source}\n".encode()
    mpq.write_mpq(args.out, files)
    print(f"{sum(len(v) for v in entries.values())} tiles, {len(failures)} failed groups, "
          f"{time.time() - started:.0f}s -> {args.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
