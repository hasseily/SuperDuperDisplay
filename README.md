# SuperDuperDisplay
Display engine for the Apple 2 network bus card

# TODO
- For SDHR_CMD_UPDATE_WINDOW_SET_WINDOW_POSITION, update the window struct and do one of 2 things:
	- either pass the window data to the vertex shader and have it cull triangles that aren't in the window, and then have
	  the fragment shader quit early if outside the window bounds
	- or somehow make a stencil buffer for each mesh when rendering it
- Disable depth testing because we'll always draw back-to-front starting with window id 0 forward

- We _could_ render each mesh as an instanced rendering of tiles. Or draw _all the meshes_, _all the tiles_, as instanced
  in one single rendering call (with the z dimension as the window id for depth testing). But very little data is common
  to them all, so it may not be useful

- We could also replace all the tiles with font glyphs and use the font rendering system of ImGui instead.