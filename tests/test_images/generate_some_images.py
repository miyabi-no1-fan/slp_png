import numpy as np
from PIL import Image

W, H = 3840, 2160 # 4k resolution
rng = np.random.default_rng(42)

def smooth_noise(channels):
    y, x = np.mgrid[0:H, 0:W]
    out = []
    for c in range(channels):
        base = 128 + 80*np.sin(x/(200+60*c)) * np.cos(y/(150+40*c))
        out.append(base + rng.normal(0, 6, (H, W)))
    a = np.stack(out, axis=-1) if channels > 1 else out[0]
    return np.clip(a, 0, 255).astype(np.uint8)

def pure_noise(channels):
    shape = (H, W, channels) if channels > 1 else (H, W)
    return rng.integers(0, 256, shape, dtype=np.uint8)

for name, fn in (("smooth", smooth_noise), ("noise", pure_noise)):
    for ch, mode in ((1, "L"), (3, "RGB"), (4, "RGBA")):
        Image.fromarray(fn(ch), mode).save(f"{name}_{mode}.png", compress_level=6)