"""Makes the app's home screen art (sce_sys) from the player's own pictures:
  icon0.png  the tile, 512x512
  pic0.dds   the background while the tile is selected, 3840x2160 BC7 (DXGI 98) in a DX10 DDS
  pic1.dds   the same picture while the game starts
The DDS profile is the one reUndr's PS5_Vulkan template ships and the console takes (its tools/prepare-assets.sh):
one level, depth one, straight alpha, opaque. BC7 here is mode 6 alone (one subset, 7-bit endpoints and a shared
p-bit, 4-bit indices): principal-axis endpoints, a least-squares refit, alpha 255 throughout.
usage: python3 mkart.py <icon image> <background image> <out dir> [--crop x0,y0,x1,y1] [--icon-crop x0,y0,x1,y1]"""
import struct, sys
import numpy as np
from PIL import Image

W, H = 3840, 2160
WEIGHTS = np.array([0, 4, 9, 13, 17, 21, 26, 30, 34, 38, 43, 47, 51, 55, 60, 64], np.float32)


def header():
    flags = 0x1 | 0x2 | 0x4 | 0x1000 | 0x80000 | 0x20000  # caps, height, width, pixel format, linear size, mip count
    h = struct.pack('<4s7I', b'DDS ', 124, flags, H, W, (W // 4) * (H // 4) * 16, 1, 1)
    h += b'\0' * 44                                          # reserved
    h += struct.pack('<2I4s5I', 32, 0x4, b'DX10', 0, 0, 0, 0, 0)
    h += struct.pack('<5I', 0x1000, 0, 0, 0, 0)              # texture
    h += struct.pack('<5I', 98, 3, 0, 1, 1)                 # BC7_UNORM, 2D, one surface, straight alpha
    assert len(h) == 148
    return h


def quantize(e):  # 8-bit endpoint values -> 7-bit with p-bit 1 (odd values: alpha 255 needs p = 1)
    return np.clip(np.rint((e - 1.0) / 2.0), 0, 127)


def palette(q0, q1):  # (n,3) 7-bit -> (n,16,3) colours
    e0, e1 = q0 * 2 + 1, q1 * 2 + 1
    return np.floor(((64 - WEIGHTS)[None, :, None] * e0[:, None, :] + WEIGHTS[None, :, None] * e1[:, None, :] + 32) / 64)


def indices(px, pal):  # (n,16,3), (n,16,3) -> (n,16)
    d = ((px[:, :, None, :] - pal[:, None, :, :]) ** 2).sum(-1)
    return d.argmin(-1)


def encode(px):  # (n,16,3) float32 -> (n,2) uint64
    n = px.shape[0]
    mean = px.mean(1)
    x = px - mean[:, None, :]
    cov = np.einsum('nki,nkj->nij', x, x)
    axis = np.ones((n, 3), np.float32) / np.sqrt(3)
    for _ in range(8):
        axis = np.einsum('nij,nj->ni', cov, axis)
        axis /= np.maximum(np.linalg.norm(axis, axis=1, keepdims=True), 1e-6)
    t = np.einsum('nki,ni->nk', x, axis)
    e0 = np.clip(mean + axis * t.min(1)[:, None], 0, 255)
    e1 = np.clip(mean + axis * t.max(1)[:, None], 0, 255)
    q0, q1 = quantize(e0), quantize(e1)
    idx = indices(px, palette(q0, q1))
    # least squares: the endpoints that best fit the pixels given their weights, then the indices again
    w = WEIGHTS[idx] / 64.0
    a, b, c = ((1 - w) ** 2).sum(1), ((1 - w) * w).sum(1), (w ** 2).sum(1)
    det = a * c - b * b
    r0 = ((1 - w)[:, :, None] * px).sum(1)
    r1 = (w[:, :, None] * px).sum(1)
    ok = det > 1e-3
    f0 = np.where(ok[:, None], (c[:, None] * r0 - b[:, None] * r1) / np.where(ok, det, 1)[:, None], e0)
    f1 = np.where(ok[:, None], (a[:, None] * r1 - b[:, None] * r0) / np.where(ok, det, 1)[:, None], e1)
    p0, p1 = quantize(np.clip(f0, 0, 255)), quantize(np.clip(f1, 0, 255))
    pal = palette(p0, p1)
    idx2 = indices(px, pal)
    err_old = ((palette(q0, q1)[np.arange(n)[:, None], idx] - px) ** 2).sum((1, 2))
    err_new = ((pal[np.arange(n)[:, None], idx2] - px) ** 2).sum((1, 2))
    better = err_new < err_old
    q0 = np.where(better[:, None], p0, q0)
    q1 = np.where(better[:, None], p1, q1)
    idx = np.where(better[:, None], idx2, idx)
    # the first pixel's index has an implied top bit of 0: swap the endpoints where it would be 1
    swap = idx[:, 0] >= 8
    q0, q1 = np.where(swap[:, None], q1, q0), np.where(swap[:, None], q0, q1)
    idx = np.where(swap[:, None], 15 - idx, idx)
    q0 = q0.astype(np.uint64)
    q1 = q1.astype(np.uint64)
    idx = idx.astype(np.uint64)
    alpha = np.uint64(127)
    lo = (np.uint64(1 << 6) | q0[:, 0] << np.uint64(7) | q1[:, 0] << np.uint64(14) | q0[:, 1] << np.uint64(21)
          | q1[:, 1] << np.uint64(28) | q0[:, 2] << np.uint64(35) | q1[:, 2] << np.uint64(42) | alpha << np.uint64(49)
          | alpha << np.uint64(56) | np.uint64(1) << np.uint64(63))
    hi = np.uint64(1) | idx[:, 0] << np.uint64(1)
    for k in range(1, 16):
        hi |= idx[:, k] << np.uint64(4 + 4 * (k - 1))
    return np.stack([lo, hi], 1)


def dds(img, path):
    a = np.asarray(img.convert('RGB'), np.float32)
    blocks = a.reshape(H // 4, 4, W // 4, 4, 3).transpose(0, 2, 1, 3, 4).reshape(-1, 16, 3)
    out = np.concatenate([encode(blocks[i:i + 20000]) for i in range(0, len(blocks), 20000)])
    with open(path, 'wb') as f:
        f.write(header())
        f.write(out.astype('<u8').tobytes())


def box(arg):
    return tuple(int(v) for v in arg.split(','))


def main():
    args = sys.argv[1:]
    crop = icon_crop = None
    if '--crop' in args:
        i = args.index('--crop'); crop = box(args[i + 1]); del args[i:i + 2]
    if '--icon-crop' in args:
        i = args.index('--icon-crop'); icon_crop = box(args[i + 1]); del args[i:i + 2]
    icon, bg, out = args
    im = Image.open(icon).convert('RGB')
    if icon_crop:
        im = im.crop(icon_crop)
    im.resize((512, 512), Image.LANCZOS).save(f'{out}/icon0.png', optimize=True)
    im = Image.open(bg).convert('RGB')
    if crop:
        im = im.crop(crop)
    im = im.resize((W, H), Image.LANCZOS)
    dds(im, f'{out}/pic0.dds')
    with open(f'{out}/pic0.dds', 'rb') as f, open(f'{out}/pic1.dds', 'wb') as g:
        g.write(f.read())


if __name__ == '__main__':
    main()
