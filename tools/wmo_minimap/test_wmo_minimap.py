import io
import os
import struct
import tempfile
import unittest

import mpyq
import numpy as np
from PIL import Image

import blp
import build
import mpq
import wmo


def chunk(tag, payload):
    return tag[::-1].encode("latin1") + struct.pack("<I", len(payload)) + payload


def group_file(box, vertices, normals, triangles, batches, poly=None, flags=wmo.GROUP_INDOOR):
    header = struct.pack("<III6f", 0, 0, flags, *box) + b"\0" * (0x44 - 36)
    body = header
    if poly is not None:
        body += chunk("MOPY", bytes(v for pair in poly for v in pair))
    body += chunk("MOVI", struct.pack("<%dH" % (3 * len(triangles)), *(i for t in triangles for i in t)))
    body += chunk("MOVT", struct.pack("<%df" % (3 * len(vertices)), *(c for v in vertices for c in v)))
    body += chunk("MONR", struct.pack("<%df" % (3 * len(normals)), *(c for n in normals for c in n)))
    moba = b"".join(b"\0" * 12 + struct.pack("<IHHHBB", start, count, 0, 0, 0, material)
                    for start, count, material in batches)
    body += chunk("MOBA", moba)
    return chunk("MVER", struct.pack("<I", 17)) + chunk("MOGP", body)


def root_file(group_flags, materials=1):
    momt = b"".join(struct.pack("<16I", *([0] * 16)) for _ in range(materials))
    mogi = b"".join(struct.pack("<I6fi", flags, 0, 0, 0, 0, 0, 0, -1) for flags in group_flags)
    return chunk("MVER", struct.pack("<I", 17)) + chunk("MOTX", b"\0\0\0\0") + chunk("MOMT", momt) + chunk("MOGI", mogi)


def floor_and_ceiling():
    floor = [(0, 0, 0), (10, 0, 0), (10, 6, 0), (0, 6, 0)]
    ceiling = [(0, 0, 3), (10, 0, 3), (10, 6, 3), (0, 6, 3)]
    vertices = floor + ceiling
    normals = [(0, 0, 1)] * 4 + [(0, 0, -1)] * 4
    triangles = [(0, 1, 2), (0, 2, 3), (4, 6, 5), (4, 7, 6)]
    return vertices, normals, triangles


class TileGeometryTests(unittest.TestCase):
    def test_single_tile_axes_use_the_next_power_of_two_at_two_pixels_per_yard(self):
        self.assertEqual(wmo.tile_pixels(45.1, 1), 128)
        self.assertEqual(wmo.tile_pixels(18.9, 1), 64)
        self.assertEqual(wmo.tile_pixels(15.7, 1), 32)
        self.assertEqual(wmo.tile_pixels(91.8, 1), 256)
        self.assertEqual(wmo.tile_pixels(0.5, 1), 4)

    def test_multi_tile_axes_keep_full_tiles(self):
        self.assertEqual(wmo.tile_pixels(187.1, 2), 256)

    def test_tile_count_follows_128_yard_tiles(self):
        group = wmo.Group(group_file((0, 0, 0, 187.1, 77.6, 5), *floor_and_ceiling(), [(0, 12, 0)]))
        self.assertEqual(wmo.tile_count(group), (2, 1))


class RenderTests(unittest.TestCase):
    def test_floor_sits_at_the_bottom_left_and_the_ceiling_is_left_out(self):
        vertices, normals, triangles = floor_and_ceiling()
        group = wmo.Group(group_file((0, 0, 0, 10, 6, 3), vertices, normals, triangles, [(0, 12, 0)]))
        root = wmo.Root(root_file([wmo.GROUP_INDOOR]))
        color, covered, nx, ny = wmo.render_group(None, root, group)
        self.assertEqual((nx, ny), (1, 1))
        self.assertEqual(covered.shape, (16, 32))
        rows, cols = np.nonzero(covered)
        self.assertEqual((cols.min(), cols.max(), rows.min(), rows.max()), (0, 19, 4, 15))
        self.assertTrue(np.allclose(color[covered], 0.5))

    def test_triangles_outside_render_batches_are_collision_only(self):
        vertices, normals, triangles = floor_and_ceiling()
        group = wmo.Group(group_file((0, 0, 0, 10, 6, 3), vertices, normals, triangles, [(0, 3, 0)]))
        color, covered, _, _ = wmo.render_group(None, wmo.Root(root_file([wmo.GROUP_INDOOR])), group)
        self.assertGreater(covered.sum(), 0)
        self.assertLess(covered.sum(), 20 * 12)

    def test_polygon_flags_mark_collision_triangles(self):
        vertices, normals, triangles = floor_and_ceiling()
        poly = [(0, 0), (0, wmo.COLLISION_ONLY), (0, 0), (0, 0)]
        group = wmo.Group(group_file((0, 0, 0, 10, 6, 3), vertices, normals, triangles, [(0, 12, 0)], poly))
        self.assertEqual(list(group.triangle_materials), [0, wmo.COLLISION_ONLY, 0, 0])

    def test_tiles_are_cut_with_tile_row_zero_at_the_bottom(self):
        color = np.zeros((512, 256, 3), dtype=np.float32)
        covered = np.zeros((512, 256), dtype=bool)
        covered[300, 10] = True
        tiles = {(tx, ty): mask for tx, ty, _, mask in wmo.tiles_of(color, covered, 1, 2)}
        self.assertTrue(tiles[(0, 0)][44, 10])
        self.assertFalse(tiles[(0, 1)].any())

    def test_indoor_groups_come_from_the_root_group_flags(self):
        root = wmo.Root(root_file([0, wmo.GROUP_INDOOR, wmo.GROUP_INDOOR | 0x8]))
        self.assertEqual(root.indoor_groups(), [1, 2])

    def test_truncated_group_chunk_is_read_to_the_end_of_the_file(self):
        vertices, normals, triangles = floor_and_ceiling()
        raw = bytearray(group_file((0, 0, 0, 10, 6, 3), vertices, normals, triangles, [(0, 12, 0)]))
        mogp = raw.find(b"PGOM")
        struct.pack_into("<I", raw, mogp + 4, struct.unpack_from("<I", raw, mogp + 4)[0] + 4)
        group = wmo.Group(bytes(raw))
        self.assertEqual(len(group.indices), 4)


class FormatTests(unittest.TestCase):
    def test_blp_round_trips_through_pillow_with_one_bit_alpha(self):
        rng = np.random.default_rng(7)
        rgb = rng.integers(0, 256, size=(32, 64, 3)).astype(np.float32)
        rgb[:, :32] = [200, 120, 40]
        opaque = np.zeros((32, 64), dtype=bool)
        opaque[:, :32] = True
        raw = blp.write_blp_dxt1(rgb, opaque)
        self.assertEqual(raw[:4], b"BLP2")
        self.assertEqual(len(raw), 1172 + 64 * 32 // 2)
        back = np.asarray(Image.open(io.BytesIO(raw)).convert("RGBA")).astype(np.float32)
        self.assertTrue(((back[..., 3] > 0) == opaque).all())
        self.assertLess(np.abs(back[:, :32, :3] - rgb[:, :32]).max(), 9)

    def test_mpq_round_trips_through_mpyq(self):
        files = {"textures\\Minimap\\a.blp": bytes(range(256)) * 300, "x\\small.txt": b"hello"}
        with tempfile.TemporaryDirectory() as folder:
            path = os.path.join(folder, "patch-T1.MPQ")
            mpq.write_mpq(path, files)
            archive = mpyq.MPQArchive(path, listfile=True)
            try:
                self.assertEqual(sorted(f.decode() for f in archive.files), sorted(files))
                self.assertIn(b"x\\small.txt", archive.read_file("(listfile)"))
                for name, content in files.items():
                    self.assertEqual(archive.read_file(name), content)
            finally:
                archive.file.close()

    def test_client_data_prefers_later_patches_and_skips_generated_archives(self):
        with tempfile.TemporaryDirectory() as folder:
            mpq.write_mpq(os.path.join(folder, "patch-A.MPQ"), {"x\\f.txt": b"base"})
            mpq.write_mpq(os.path.join(folder, "patch-B.MPQ"), {"x\\f.txt": b"newer"})
            mpq.write_mpq(os.path.join(folder, "patch-ZM.MPQ"), {"x\\f.txt": b"ours", wmo.MARKER_PATH: b"x"})
            data = wmo.ClientData(folder)
            try:
                self.assertEqual(data.read_with_source("x\\f.txt"), ("patch-B.MPQ", b"newer"))
            finally:
                data.close()

    def test_archive_order_puts_patch_mpq_below_lettered_patches(self):
        order = wmo.archive_order(["patch-ZM.MPQ", "common.MPQ", "patch.MPQ", "patch-2.MPQ", "patch-O.MPQ"])
        self.assertEqual(order, ["patch-ZM.MPQ", "patch-O.MPQ", "patch-2.MPQ", "patch.MPQ", "common.MPQ"])


class TranslationTests(unittest.TestCase):
    def test_covered_groups_reads_wmo_entries_case_insensitively(self):
        text = "dir: WMO\\A\nWMO\\A\\Inn_003_00_01.blp\tabc.blp\nAzeroth\\map1_2.blp\tdef.blp\n"
        self.assertEqual(build.covered_groups(text), {("wmo\\a\\inn", 3)})

    def test_merged_trs_keeps_existing_lines_and_adds_upper_case_variants(self):
        base = "dir: Azeroth\nAzeroth\\map1_2.blp\tdef.blp\n\n"
        merged = build.merged_trs(base, {"WMO\\a": [("WMO\\a\\inn_001_00_00.blp", "f00.blp")]}).decode()
        lines = merged.splitlines()
        self.assertEqual(lines[:2], ["dir: Azeroth", "Azeroth\\map1_2.blp\tdef.blp"])
        self.assertIn("dir: WMO\\a", lines)
        self.assertIn("WMO\\a\\inn_001_00_00.blp\tf00.blp", lines)
        self.assertIn("WMO\\A\\INN_001_00_00.blp\tf00.blp", lines)

    def test_tile_name_follows_the_wmo_path(self):
        self.assertEqual(build.tile_name("world\\wmo\\a\\b\\inn.wmo", 4, 1, 2), "WMO\\a\\b\\inn_004_01_02.blp")


if __name__ == "__main__":
    unittest.main()
