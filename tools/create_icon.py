from pathlib import Path
from PIL import Image, ImageDraw

root = Path(__file__).resolve().parents[1]
assets = root / "assets"
assets.mkdir(exist_ok=True)
image = Image.new("RGBA", (256, 256))
draw = ImageDraw.Draw(image)
draw.rounded_rectangle((8, 8, 248, 248), radius=67, fill="#26735b")
draw.rounded_rectangle((82, 49, 174, 207), radius=44, outline="white", width=10)
draw.line((128, 75, 128, 105), fill="white", width=10)
image.save(assets / "side-mapping.ico", sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])
