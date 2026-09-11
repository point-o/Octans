from PIL import Image

src = Image.open(r"res/OctansLogo.png").convert("RGBA")
w, h = src.size
side = max(w, h)
canvas = Image.new("RGBA", (side, side), (255, 255, 255, 255))
canvas.alpha_composite(src, ((side - w) // 2, (side - h) // 2))

out = [16, 20, 24, 32, 40, 48, 64, 128, 256]
images = [canvas.resize((s, s), Image.LANCZOS) for s in out]
images[-1].save(
    r"res/Octans.ico",
    sizes=[(s, s) for s in out],
    append_images=images[:-1],
    format="ICO",
)
print("Octans.ico written")
