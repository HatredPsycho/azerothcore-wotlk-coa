import io
import math
import os
import struct

import mpyq
import numpy as np
from PIL import Image

TILE_YARDS = 128.0
PIXELS_PER_YARD = 2.0
TILE_PIXELS = 256
GROUP_INDOOR = 0x2000
COLLISION_ONLY = 0xFF
UPWARD_NORMAL = 0.05
MARKER_PATH = "textures\\Minimap\\wmo_minimap.txt"


def archive_order(names):
    base = sorted((n for n in names if not n.lower().startswith("patch")), key=str.lower)
    patches = sorted((n for n in names if n.lower().startswith("patch")),
                     key=lambda n: "" if n.lower() == "patch.mpq" else n.lower())
    return list(reversed(base + patches))


class ClientData:
    def __init__(self, data_dir, exclude=()):
        excluded = {name.lower() for name in exclude}
        names = [n for n in os.listdir(data_dir) if n.lower().endswith(".mpq") and n.lower() not in excluded]
        self.archives = []
        for name in archive_order(names):
            try:
                archive = mpyq.MPQArchive(os.path.join(data_dir, name), listfile=False)
            except Exception:
                continue
            if _has_file(archive, MARKER_PATH):
                archive.file.close()
                continue
            self.archives.append((name, archive))
        self.textures = {}

    def close(self):
        for _, archive in self.archives:
            archive.file.close()
        self.archives = []

    def read(self, path):
        found = self.read_with_source(path)
        return found[1] if found else None

    def read_with_source(self, path):
        for name, archive in self.archives:
            try:
                data = archive.read_file(path)
            except Exception:
                data = None
            if data:
                return name, data
        return None

    def texture(self, path):
        if path not in self.textures:
            self.textures[path] = texture_levels(self.read(path))
        return self.textures[path]


def _has_file(archive, path):
    try:
        return bool(archive.read_file(path))
    except Exception:
        return False


def texture_levels(raw):
    if not raw:
        return None
    try:
        image = Image.open(io.BytesIO(raw)).convert("RGBA")
    except Exception:
        return None
    if min(image.size) < 1:
        return None
    levels = [np.asarray(image, dtype=np.float32) / 255.0]
    while min(image.size) > 4:
        image = image.resize((max(1, image.size[0] // 2), max(1, image.size[1] // 2)), Image.BOX)
        levels.append(np.asarray(image, dtype=np.float32) / 255.0)
    return levels


def chunks(data, start=0, end=None):
    end = len(data) if end is None else min(end, len(data))
    pos = start
    while pos + 8 <= end:
        tag = data[pos:pos + 4][::-1].decode("latin1")
        size = struct.unpack_from("<I", data, pos + 4)[0]
        yield tag, pos + 8, size
        pos += 8 + size


class Root:
    def __init__(self, data):
        self.texture_names = b""
        self.materials = []
        self.groups = []
        for tag, body, size in chunks(data):
            if tag == "MOTX":
                self.texture_names = data[body:body + size]
            elif tag == "MOMT":
                for i in range(size // 64):
                    self.materials.append(struct.unpack_from("<I", data, body + 64 * i + 12)[0])
            elif tag == "MOGI":
                for i in range(size // 32):
                    self.groups.append(struct.unpack_from("<I", data, body + 32 * i)[0])

    def texture_name(self, material_id):
        offset = self.materials[material_id]
        end = self.texture_names.find(b"\0", offset)
        return self.texture_names[offset:end if end >= 0 else None].decode("latin1")

    def indoor_groups(self):
        return [i for i, flags in enumerate(self.groups) if flags & GROUP_INDOOR]


class Group:
    def __init__(self, data):
        for tag, body, size in chunks(data):
            if tag != "MOGP":
                continue
            self.flags = struct.unpack_from("<I", data, body + 8)[0]
            self.box = struct.unpack_from("<6f", data, body + 12)
            self.vertices = self.indices = self.normals = self.uvs = self.colors = self.poly = None
            self.batches = []
            for sub, sbody, ssize in chunks(data, body + 0x44, body + size):
                raw = data[sbody:sbody + ssize]
                if sub == "MOPY":
                    self.poly = np.frombuffer(raw, dtype=np.uint8).reshape(-1, 2)
                elif sub == "MOVI":
                    self.indices = np.frombuffer(raw, dtype="<u2")[:len(raw) // 6 * 3].reshape(-1, 3)
                elif sub == "MOVT":
                    self.vertices = np.frombuffer(raw, dtype="<f4").reshape(-1, 3)
                elif sub == "MONR":
                    self.normals = np.frombuffer(raw, dtype="<f4").reshape(-1, 3)
                elif sub == "MOTV" and self.uvs is None:
                    self.uvs = np.frombuffer(raw, dtype="<f4").reshape(-1, 2)
                elif sub == "MOCV" and self.colors is None:
                    self.colors = np.frombuffer(raw, dtype=np.uint8).reshape(-1, 4)
                elif sub == "MOBA":
                    for i in range(ssize // 24):
                        start, count = struct.unpack_from("<IH", raw, 24 * i + 12)
                        self.batches.append((start, count, raw[24 * i + 23]))
            self.triangle_materials = self._triangle_materials()
            return
        raise ValueError("no MOGP chunk")

    def _triangle_materials(self):
        if self.indices is None:
            return None
        count = len(self.indices)
        materials = np.full(count, COLLISION_ONLY, dtype=np.int32)
        poly_matches = self.poly is not None and len(self.poly) == count
        if self.batches:
            for start, length, material in self.batches:
                materials[start // 3:min(count, (start + length) // 3)] = material
        elif poly_matches:
            materials[:] = self.poly[:, 1]
        if poly_matches:
            materials[self.poly[:, 1] == COLLISION_ONLY] = COLLISION_ONLY
        return materials


def tile_count(group):
    x0, y0, _, x1, y1, _ = group.box
    return (max(1, math.ceil((x1 - x0) / TILE_YARDS - 1e-6)),
            max(1, math.ceil((y1 - y0) / TILE_YARDS - 1e-6)))


def tile_pixels(extent, count):
    if count > 1:
        return TILE_PIXELS
    wanted = max(4.0, extent * PIXELS_PER_YARD)
    size = 4
    while size < wanted and size < TILE_PIXELS:
        size *= 2
    return size


def tile_size(group):
    nx, ny = tile_count(group)
    x0, y0, _, x1, y1, _ = group.box
    return tile_pixels(x1 - x0, nx), tile_pixels(y1 - y0, ny)


def _shade(texture, uv, weights, px, py):
    w0, w1, w2 = weights
    base = texture[0]
    span_uv = max(np.ptp(uv[:, 0]) * base.shape[1], np.ptp(uv[:, 1]) * base.shape[0])
    span_px = max(np.ptp(px), np.ptp(py), 1e-3)
    level = texture[int(np.clip(np.floor(np.log2(max(span_uv / span_px, 1.0))), 0, len(texture) - 1))]
    height, width = level.shape[:2]
    u = w0 * uv[0, 0] + w1 * uv[1, 0] + w2 * uv[2, 0]
    v = w0 * uv[0, 1] + w1 * uv[1, 1] + w2 * uv[2, 1]
    return level[(np.mod(v, 1.0) * height).astype(int) % height, (np.mod(u, 1.0) * width).astype(int) % width, :3]


def render_group(data, root, group):
    nx, ny = tile_count(group)
    tw, th = tile_size(group)
    x0 = group.box[0]
    top = group.box[1] + ny * th / PIXELS_PER_YARD
    width, height = nx * tw, ny * th
    color = np.zeros((height, width, 3), dtype=np.float32)
    depth = np.full((height, width), -np.inf, dtype=np.float32)
    covered = np.zeros((height, width), dtype=bool)
    if group.indices is None or group.vertices is None:
        return color, covered, nx, ny

    for tri_index, tri in enumerate(group.indices):
        material_id = int(group.triangle_materials[tri_index])
        if material_id == COLLISION_ONLY or material_id >= len(root.materials):
            continue
        if group.normals is not None and group.normals[tri].mean(axis=0)[2] <= UPWARD_NORMAL:
            continue
        p = group.vertices[tri]
        px = (p[:, 0] - x0) * PIXELS_PER_YARD
        py = (top - p[:, 1]) * PIXELS_PER_YARD
        lo_x, hi_x = int(max(0, np.floor(px.min()))), int(min(width - 1, np.ceil(px.max())))
        lo_y, hi_y = int(max(0, np.floor(py.min()))), int(min(height - 1, np.ceil(py.max())))
        if lo_x > hi_x or lo_y > hi_y:
            continue
        d = (py[1] - py[2]) * (px[0] - px[2]) + (px[2] - px[1]) * (py[0] - py[2])
        if abs(d) < 1e-9:
            continue
        gx, gy = np.meshgrid(np.arange(lo_x, hi_x + 1) + 0.5, np.arange(lo_y, hi_y + 1) + 0.5)
        w0 = ((py[1] - py[2]) * (gx - px[2]) + (px[2] - px[1]) * (gy - py[2])) / d
        w1 = ((py[2] - py[0]) * (gx - px[2]) + (px[0] - px[2]) * (gy - py[2])) / d
        w2 = 1.0 - w0 - w1
        inside = (w0 >= -1e-4) & (w1 >= -1e-4) & (w2 >= -1e-4)
        if not inside.any():
            continue
        z = w0 * p[0, 2] + w1 * p[1, 2] + w2 * p[2, 2]
        region = depth[lo_y:hi_y + 1, lo_x:hi_x + 1]
        front = inside & (z > region)
        if not front.any():
            continue

        texture = data.texture(root.texture_name(material_id)) if data else None
        if texture is not None and group.uvs is not None:
            rgb = _shade(texture, group.uvs[tri], (w0, w1, w2), px, py)
        else:
            rgb = np.full(front.shape + (3,), 0.5, dtype=np.float32)
        if group.colors is not None:
            vertex = group.colors[tri][:, [2, 1, 0]].astype(np.float32) / 127.0
            light = w0[..., None] * vertex[0] + w1[..., None] * vertex[1] + w2[..., None] * vertex[2]
            rgb = rgb * np.clip(light, 0.0, 2.0)

        region[front] = z[front]
        color[lo_y:hi_y + 1, lo_x:hi_x + 1][front] = rgb[front]
        covered[lo_y:hi_y + 1, lo_x:hi_x + 1][front] = True
    return np.clip(color, 0.0, 1.0), covered, nx, ny


def tiles_of(color, covered, nx, ny):
    tw, th = color.shape[1] // nx, color.shape[0] // ny
    for tx in range(nx):
        for ty in range(ny):
            top = (ny - 1 - ty) * th
            yield (tx, ty, color[top:top + th, tx * tw:(tx + 1) * tw],
                   covered[top:top + th, tx * tw:(tx + 1) * tw])
