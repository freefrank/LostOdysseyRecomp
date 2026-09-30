# Installer font provenance

`unifont_packed.inl` is a packed subset of GNU Unifont 13.0.06 from the
repository's pinned SDL dependency, not an original Lost Odyssey game font.
The local provenance review compared all 33,142 glyphs and 1,052,064 bitmap bytes
against [SDL's source hex file](../../thirdparty/SDL/test/unifont-13.0.06.hex):
every glyph, width and byte matched, and codepoints were strictly increasing.

- Source SHA-256: `a009fec40b656e5875ca13aa88c4dd033155a089bfe7b0724767f65e7c96e666`.
- The supplied [SIL Open Font License 1.1](../../thirdparty/SDL/test/unifont-13.0.06-license.txt)
  applies to the font data, separately from the application source license.
- License-file SHA-256: `869692af094c57fb7258c57fe26820c759319603321d0ffeb278de3651763ded`.
- Packing changes storage layout and selects glyphs; it does not establish
  full Unicode coverage, readable Chinese rendering or game-style fidelity.

Release packages retain this notice as `licenses/FONT-PROVENANCE.md` and include
the complete upstream OFL text as `licenses/Unifont-OFL-1.1.txt`. The unused
`installer_font_data.inl` VGA experiment is not part of the active font
implementation.
