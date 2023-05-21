# SuperDuperDisplay
Display engine for the Apple 2 network bus card

# TODO
- SDHR_CMD_UPDATE_WINDOW_SET_IMMEDIATE & SDHR_CMD_UPDATE_WINDOW_SET_UPLOAD need the generation of vertices.
- Modify mesh.h to:
	- be the rectangular mesh that will be displayed in the window
	- add a window_id to the mesh (or just have the mesh as part of the window struct)
	- have its vertices repositioned in the world so they align to the window by passing to the vertex shader a world matrix
	- get the world matrix to change based on SDHR_CMD_UPDATE_WINDOW_SHIFT_TILES or SDHR_CMD_UPDATE_WINDOW_ADJUST_WINDOW_VIEW
	- make sure that the world-positioned vertices are within -1 and 1 only inside the SDHR screen. This will auto-cull them
- For SDHR_CMD_UPDATE_WINDOW_SET_WINDOW_POSITION, update the window struct and do one of 2 things:
	- either pass the window data to the vertex shader and have it cull triangles that aren't in the window, and then have
	  the fragment shader quit early if outside the window bounds
	- or somehow make a stencil buffer for each mesh when rendering it
- When rendering, draw() each mesh
- Disable depth testing because we'll always draw back-to-front starting with window id 0 forward
- There will be exactly 16 textures, with their index passed in to the shaders

- We _could_ render each mesh as an instanced rendering of tiles. Or draw _all the meshes_, _all the tiles_, as instanced
  in one single rendering call (with the z dimension as the window id for depth testing). But very little data is common
  to them all, so it may not be useful

- We could also replace all the tiles with font glyphs and use the font rendering system of ImGui instead.