import struct

import numpy as np


def _to565(rgb):
    r = (np.clip(rgb[..., 0], 0, 255).astype(np.uint32) * 31 + 127) // 255
    g = (np.clip(rgb[..., 1], 0, 255).astype(np.uint32) * 63 + 127) // 255
    b = (np.clip(rgb[..., 2], 0, 255).astype(np.uint32) * 31 + 127) // 255
    return (r << 11) | (g << 5) | b


def _from565(c):
    r = ((c >> 11) & 31) * 255 // 31
    g = ((c >> 5) & 63) * 255 // 63
    b = (c & 31) * 255 // 31
    return np.stack([r, g, b], axis=-1).astype(np.float32)


def encode_dxt1(rgb, opaque):
    h, w = opaque.shape
    blocks = rgb.reshape(h // 4, 4, w // 4, 4, 3).transpose(0, 2, 1, 3, 4).reshape(-1, 16, 3).astype(np.float32)
    alpha = opaque.reshape(h // 4, 4, w // 4, 4).transpose(0, 2, 1, 3).reshape(-1, 16)
    count = blocks.shape[0]

    weights = alpha[..., None].astype(np.float32)
    present = np.maximum(weights.sum(axis=1), 1.0)
    mean = (blocks * weights).sum(axis=1) / present
    centered = (blocks - mean[:, None, :]) * weights
    cov = np.einsum("nki,nkj->nij", centered, centered)
    axis = np.ones((count, 3), dtype=np.float32)
    for _ in range(4):
        axis = np.einsum("nij,nj->ni", cov, axis)
        axis /= np.maximum(np.linalg.norm(axis, axis=1, keepdims=True), 1e-6)
    proj = np.einsum("nki,ni->nk", blocks - mean[:, None, :], axis)
    proj_masked_lo = np.where(alpha, proj, np.inf).min(axis=1)
    proj_masked_hi = np.where(alpha, proj, -np.inf).max(axis=1)
    proj_masked_lo = np.where(np.isfinite(proj_masked_lo), proj_masked_lo, 0.0)
    proj_masked_hi = np.where(np.isfinite(proj_masked_hi), proj_masked_hi, 0.0)
    lo = mean + axis * proj_masked_lo[:, None]
    hi = mean + axis * proj_masked_hi[:, None]

    c_hi = _to565(hi)
    c_lo = _to565(lo)
    has_transparent = ~alpha.all(axis=1)

    four = ~has_transparent
    swap = four & (c_hi < c_lo)
    c0 = np.where(swap, c_lo, c_hi)
    c1 = np.where(swap, c_hi, c_lo)
    same = four & (c0 == c1)
    c0 = np.where(same & (c0 < 0xFFFF), c0 + 1, c0)
    c1 = np.where(same & (c0 == 0xFFFF) & (c1 > 0), c1 - 1, c1)

    three_swap = has_transparent & (c_hi > c_lo)
    c0 = np.where(has_transparent, np.where(three_swap, c_lo, c_hi), c0)
    c1 = np.where(has_transparent, np.where(three_swap, c_hi, c_lo), c1)

    e0, e1 = _from565(c0), _from565(c1)
    palette4 = np.stack([e0, e1, (2 * e0 + e1) / 3, (e0 + 2 * e1) / 3], axis=1)
    palette3 = np.stack([e0, e1, (e0 + e1) / 2, np.zeros_like(e0)], axis=1)
    palette = np.where(has_transparent[:, None, None], palette3, palette4)
    candidates = 4 - has_transparent.astype(int)

    dist = ((blocks[:, :, None, :] - palette[:, None, :, :]) ** 2).sum(axis=-1)
    usable = np.arange(4)[None, None, :] < candidates[:, None, None]
    dist = np.where(usable, dist, np.inf)
    index = dist.argmin(axis=2).astype(np.uint32)
    index = np.where(alpha, index, 3)
    bits = (index << (2 * np.arange(16, dtype=np.uint32))[None, :]).sum(axis=1).astype(np.uint32)

    out = np.zeros((count, 2), dtype="<u4")
    out[:, 0] = (c0.astype(np.uint32) & 0xFFFF) | ((c1.astype(np.uint32) & 0xFFFF) << 16)
    out[:, 1] = bits
    return out.tobytes()


def write_blp_dxt1(rgb, opaque):
    h, w = opaque.shape
    payload = encode_dxt1(rgb, opaque)
    header = struct.pack("<4sIBBBBII", b"BLP2", 1, 2, 1, 0, 0, w, h)
    offsets = [1172] + [0] * 15
    sizes = [len(payload)] + [0] * 15
    header += struct.pack("<16I", *offsets) + struct.pack("<16I", *sizes) + b"\0" * 1024
    return header + payload
