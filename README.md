# SuperDuperDisplay
Display engine for the Apple 2 network bus card

# TODO
- Disable depth testing because we'll always draw back-to-front starting with window id 0 forward

- We _could_ render each mesh as an instanced rendering of tiles. Or draw _all the meshes_, _all the tiles_, as instanced
  in one single rendering call (with the z dimension as the window id for depth testing). But very little data is common
  to them all, so it may not be useful

- We could also replace all the tiles with font glyphs and use the font rendering system of ImGui instead.