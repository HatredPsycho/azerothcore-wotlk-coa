# WMO indoor minimap tiles

Ascension's client ships buildings and caves (WMOs) whose indoor groups have no minimap tiles in
`textures\Minimap\md5translate.trs`; inside them the minimap stays black or shows whatever texture was drawn last
(often calendar art). `build.py` renders the missing tiles from the WMO geometry and writes a patch archive that
carries them together with the client's current translation file.

```bash
python -B tools/wmo_minimap/build.py --data D:/PServer/Ascension/Data --source patch-WC1.MPQ --out D:/PServer/Ascension/Data/patch-ZM.MPQ
```

- `--source` names the archives whose WMOs are covered; only indoor groups without an existing entry are rendered.
- The translation file is taken from the highest-priority archive that has one. Archives written by this tool carry
  `textures\Minimap\wmo_minimap.txt` and are never read back, so a rebuild starts from the client's own data.
- The client reads archives only at start-up: close it before replacing the patch and start it again afterwards.
- `--only` limits the run to WMO paths containing the given strings, `--workers` sets the render processes.
- Requires `mpyq`, `numpy` and `Pillow`.

Tile format, matched against Blizzard's and Ascension's own tiles: 2 pixels per yard, tiles of 128 yards counted from
the group's bounding-box minimum, pixel row 0 at the top of each tile, BLP2 with DXT1 and 1-bit alpha, no mipmaps.
An axis that fits into one tile is sized to the next power of two of its extent in pixels (the client stretches the
texture over exactly that area); axes with several tiles use 256 pixels. Floors are drawn from above with their
textures and vertex lighting; ceilings, collision-only triangles and doodads are left out.

After a `/reload` the client can keep stale texture handles for indoor tiles and show other UI art until the next
zone change or restart; that is a client fault, not a missing tile.

Tests: `python -B tools/verify_all.py --stages harness --harness test_wmo_minimap`.
