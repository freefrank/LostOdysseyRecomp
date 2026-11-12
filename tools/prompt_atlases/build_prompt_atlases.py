"""Builds LostOdysseyRecomp/hid/prompt_atlases_packed.inl: the Switch, Steam Deck
and keyboard button icons, drawn in the game's controller atlas grid.

    python -I build_prompt_atlases.py --xelu <Xelu prompts folder> [--font <bold TTF>]
                                       [--output <inl>] [--preview <png>]

The artwork is Nicolae "Xelu" Berbece's free controller and keyboard prompts
(Creative Commons 0, https://thoseawesomeguys.com/prompts/): its Switch/ and
Steam Deck/ PNGs. Key caps are drawn here in the same style with --font
(default: matplotlib's DejaVu Sans Bold). Nothing comes from the game.

The game draws button prompts from two 256x128 atlases (Icon_Page_0 and the
font's icon page, hid/controller_atlas_glyphs.h) with the same 7x3 grid of
36-pixel cells:

  row 0: LB  RB  LT  RT  left stick  right stick  L3
  row 1: R3  guide  D-pad  Back  Start  D-pad up  D-pad down
  row 2: A   B   X   Y   D-pad left  D-pad right

The atlases here are 4x (1024x512, 144-pixel cells). The Switch atlas is by
position: the bottom face button (Xbox A) shows B; hid/glyph_atlases.cpp swaps
the face cells when the pad is read by label. The key sheet holds one cap per
KEY_NAMES entry (KEY_CAP square, KEY_COLUMNS per row); the runtime composes the
keyboard atlas from it and the bound keys.

Pixels are stored as zlib streams of 8-bit grey + alpha pairs: every icon is
grey after LIGHTEN, like the game's own white button tiles.
"""
import argparse
import os
import zlib

from PIL import Image, ImageDraw, ImageFont

SCALE = 4
CELL = 36 * SCALE
ATLAS_W, ATLAS_H = 256 * SCALE, 128 * SCALE
ICON = 124  # icon size inside a 144-pixel cell
KEY_CAP = 128
KEY_COLUMNS = 16

# Cells (column, row).
LB, RB, LT, RT, LSTICK, RSTICK, L3 = (0, 0), (1, 0), (2, 0), (3, 0), (4, 0), (5, 0), (6, 0)
R3, GUIDE, DPAD, BACK, START, DUP, DDOWN = (0, 1), (1, 1), (2, 1), (3, 1), (4, 1), (5, 1), (6, 1)
A, B, X, Y, DLEFT, DRIGHT = (0, 2), (1, 2), (2, 2), (3, 2), (4, 2), (5, 2)

SWITCH = {
    A: "Switch_B", B: "Switch_A", X: "Switch_Y", Y: "Switch_X",  # by position
    LB: "Switch_LB", RB: "Switch_RB", LT: "Switch_LT", RT: "Switch_RT",
    LSTICK: "Switch_Left_Stick", RSTICK: "Switch_Right_Stick",
    L3: "Switch_Left_Stick_Click", R3: "Switch_Right_Stick_Click",
    GUIDE: "Switch_Home", DPAD: "Switch_Dpad", BACK: "Switch_Minus", START: "Switch_Plus",
    DUP: "Switch_Dpad_Up", DDOWN: "Switch_Dpad_Down", DLEFT: "Switch_Dpad_Left", DRIGHT: "Switch_Dpad_Right",
}
STEAM_DECK = {
    A: "SteamDeck_A", B: "SteamDeck_B", X: "SteamDeck_X", Y: "SteamDeck_Y",
    LB: "SteamDeck_L1", RB: "SteamDeck_R1", LT: "SteamDeck_L2", RT: "SteamDeck_R2",
    LSTICK: "SteamDeck_Left_Stick", RSTICK: "SteamDeck_Right_Stick",
    L3: "SteamDeck_Left_Stick_Click", R3: "SteamDeck_Right_Stick_Click",
    GUIDE: "SteamDeck_Steam", DPAD: "SteamDeck_Dpad", BACK: "SteamDeck_Inventory", START: "SteamDeck_Menu",
    DUP: "SteamDeck_Dpad_Up", DDOWN: "SteamDeck_Dpad_Down", DLEFT: "SteamDeck_Dpad_Left",
    DRIGHT: "SteamDeck_Dpad_Right",
}

# Key sheet order and cap labels; the runtime maps SDL scancodes to these names.
KEYS = (
    [(f"F{i}", f"F{i}") for i in range(1, 25)]
    + [(chr(c), chr(c)) for c in range(ord("A"), ord("Z") + 1)]
    + [(str(i), str(i)) for i in range(10)]
    + [("Backtick", "`"), ("Minus", "-"), ("Equals", "="), ("Comma", ","), ("Period", "."),
       ("Semicolon", ";"), ("Slash", "/"), ("Backslash", "\\"), ("LBracket", "["), ("RBracket", "]"),
       ("Quote", "'"),
       ("Escape", "Esc"), ("Return", "Enter"), ("Space", "Space"), ("Tab", "Tab"), ("Backspace", "⌫"),
       ("Delete", "Del"), ("Insert", "Ins"), ("Home", "Home"), ("End", "End"), ("PageUp", "PgUp"),
       ("PageDown", "PgDn"),
       ("Left", "←"), ("Right", "→"), ("Up", "↑"), ("Down", "↓"),
       ("Shift", "Shift"), ("Control", "Ctrl"), ("Alt", "Alt")]
    + [(f"Numpad{i}", f"Num{i}") for i in range(10)]
    + [("NumpadEnter", "Enter"), ("NumpadPlus", "Num+"), ("NumpadMinus", "Num-"), ("NumpadStar", "Num*"),
       ("NumpadSlash", "Num/"), ("PrintScreen", "PrtSc"), ("Pause", "Pause"), ("CapsLock", "Caps"),
       ("NumLock", "NumLk"), ("ScrollLock", "ScrLk")]
    + [("Empty", "")]
)


def default_font():
    import matplotlib
    return os.path.join(os.path.dirname(matplotlib.__file__), "mpl-data", "fonts", "ttf", "DejaVuSans-Bold.ttf")


def lighten(img):
    """Xelu's dark grey buttons in the game's tone: light body, dark label,
    the black outline untouched, so they read like the game's own tiles."""
    img = img.copy()
    px = img.load()
    for y in range(img.height):
        for x in range(img.width):
            r, g, b, a = px[x, y]
            if not a:
                continue
            lum = (r * 299 + g * 587 + b * 114) // 1000
            if lum < 48:
                v = lum  # outline
            else:
                t = max(0.0, min(1.0, (lum - 90) / 50.0))
                v = int(228 * (1.0 - t) + 36 * t)
            px[x, y] = (v, v, v, a)
    return img


def controller_atlas(folder, cells):
    atlas = Image.new("RGBA", (ATLAS_W, ATLAS_H), (0, 0, 0, 0))
    for (column, row), name in cells.items():
        icon = Image.open(os.path.join(folder, name + ".png")).convert("RGBA").resize((ICON, ICON), Image.LANCZOS)
        icon = lighten(icon)
        atlas.alpha_composite(icon, (column * CELL + (CELL - ICON) // 2, row * CELL + (CELL - ICON) // 2))
    return atlas


def key_cap(font_path, label):
    """A light key cap with a dark rim and a shadow below, like Xelu's keys."""
    img = Image.new("RGBA", (KEY_CAP, KEY_CAP), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.rounded_rectangle((8, 14, KEY_CAP - 8, KEY_CAP - 6), radius=18, fill=(40, 40, 40, 255))
    d.rounded_rectangle((8, 8, KEY_CAP - 8, KEY_CAP - 14), radius=18, fill=(244, 244, 244, 255),
                        outline=(34, 34, 34, 255), width=6)
    if label:
        size = 64
        while size > 14:
            font = ImageFont.truetype(font_path, size)
            box = d.textbbox((0, 0), label, font=font)
            if box[2] - box[0] <= KEY_CAP - 34 and box[3] - box[1] <= KEY_CAP - 50:
                break
            size -= 2
        box = d.textbbox((0, 0), label, font=font)
        x = (KEY_CAP - (box[2] - box[0])) / 2 - box[0]
        y = (KEY_CAP - 6 - (box[3] - box[1])) / 2 - box[1]
        d.text((x, y), label, font=font, fill=(30, 30, 30, 255))
    return img


def key_sheet(font_path):
    rows = (len(KEYS) + KEY_COLUMNS - 1) // KEY_COLUMNS
    sheet = Image.new("RGBA", (KEY_COLUMNS * KEY_CAP, rows * KEY_CAP), (0, 0, 0, 0))
    for i, (_, label) in enumerate(KEYS):
        sheet.alpha_composite(key_cap(font_path, label), ((i % KEY_COLUMNS) * KEY_CAP, (i // KEY_COLUMNS) * KEY_CAP))
    return sheet


def packed(img):
    grey_alpha = bytearray()
    for r, g, b, a in img.getdata():
        grey_alpha += bytes(((r * 299 + g * 587 + b * 114) // 1000 if a else 0, a))
    return zlib.compress(bytes(grey_alpha), 9)


def array(name, data):
    lines = [f"inline constexpr uint8_t {name}[] = {{"]
    for i in range(0, len(data), 24):
        lines.append("    " + ", ".join(f"0x{b:02x}" for b in data[i:i + 24]) + ",")
    lines.append("};")
    return "\n".join(lines)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--xelu", required=True, help="Xelu's prompts folder (with Switch/ and Steam Deck/)")
    ap.add_argument("--font", default=None, help="bold TTF for key cap labels (default: DejaVu Sans Bold)")
    ap.add_argument("--output", default=os.path.join(os.path.dirname(__file__), "..", "..", "LostOdysseyRecomp",
                                                     "hid", "prompt_atlases_packed.inl"))
    ap.add_argument("--preview", help="PNG with every image on a dark and a light background")
    a = ap.parse_args()
    font_path = a.font or default_font()

    switch = controller_atlas(os.path.join(a.xelu, "Switch"), SWITCH)
    deck = controller_atlas(os.path.join(a.xelu, "Steam Deck"), STEAM_DECK)
    sheet = key_sheet(font_path)

    out = [
        "// Generated by tools/prompt_atlases/build_prompt_atlases.py. Do not edit.",
        "// Switch and Steam Deck icons: Xelu's free prompts, Creative Commons 0",
        "// (https://thoseawesomeguys.com/prompts/); key caps drawn by the tool.",
        "// zlib streams of 8-bit grey + alpha pairs, rows without padding.",
        "",
        "namespace hid::glyphs::packed",
        "{",
        f"inline constexpr uint32_t AtlasWidth = {ATLAS_W}, AtlasHeight = {ATLAS_H}, Cell = {CELL};",
        f"inline constexpr uint32_t KeyCap = {KEY_CAP}, KeyColumns = {KEY_COLUMNS}, "
        f"KeySheetWidth = {sheet.width}, KeySheetHeight = {sheet.height};",
        "// The key sheet's caps in order; the last one is blank.",
        "inline constexpr const char *KeyNames[] = {",
    ]
    names = [f'"{name}"' for name, _ in KEYS]
    for i in range(0, len(names), 12):
        out.append("    " + ", ".join(names[i:i + 12]) + ",")
    out += ["};", "// Switch by position: the bottom face button's cell shows B.",
            array("Switch", packed(switch)), array("SteamDeck", packed(deck)), array("KeySheet", packed(sheet)), "}"]
    with open(a.output, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(out) + "\n")
    print(a.output, os.path.getsize(a.output), "bytes")

    if a.preview:
        images = [switch, deck, sheet]
        width = max(i.width for i in images)
        view = Image.new("RGB", (width * 2, sum(i.height for i in images)), (0, 0, 0))
        y = 0
        for img in images:
            for k, background in enumerate(((28, 30, 44, 255), (150, 140, 120, 255))):
                tile = Image.new("RGBA", img.size, background)
                tile.alpha_composite(img)
                view.paste(tile.convert("RGB"), (k * width, y))
            y += img.height
        view.save(a.preview)


if __name__ == "__main__":
    main()
