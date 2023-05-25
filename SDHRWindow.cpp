#include "SDHRWindow.h"


void SDHRWindow::Reset()
{
	enabled = 0;
	black_or_wrap = false;
	screen_count = uXY({ 0,0 });
	screen_begin = iXY({ 0,0 });
	tile_begin = iXY({ 0,0 });
	tile_dim = uXY({ 0,0 });
	tile_count = uXY({ 0,0 });

	if (mesh) {
		delete mesh;
	}
};

void SDHRWindow::Define(uXY _screen_count, uXY _tile_dim, uXY _tile_count, Shader* _shaderProgram)
{
	this->Reset();
	screen_count = _screen_count;
	tile_dim = _tile_dim;
	tile_count = _tile_count;
	mesh = new MosaicMesh(tile_count.x, tile_count.y, tile_dim.x, tile_dim.y, this->index);
	mesh->shaderProgram = _shaderProgram;
	// Calculate the position of the mesh with respect to the screen top-left 0,0
	mesh->SetWorldCoordinates(screen_begin.x - tile_begin.x, screen_begin.y - tile_begin.y);
}

void SDHRWindow::ShiftTiles(iXY _direction)
{
	tile_begin.x += _direction.x;
	tile_begin.y += _direction.y;

	tile_begin.x %= tile_count.x;
	tile_begin.y %= tile_count.y;

	// Calculate the position of the mesh with respect to the screen top-left 0,0
	mesh->SetWorldCoordinates(screen_begin.x - tile_begin.x, screen_begin.y - tile_begin.y);
}

// Move the window and the mesh on screen
// In other words, move the mesh and its stencil buffer
void SDHRWindow::SetPosition(iXY _screen_pos)
{
	screen_begin.x = _screen_pos.x;
	screen_begin.y = _screen_pos.y;
	mesh->SetWorldCoordinates(screen_begin.x - tile_begin.x, screen_begin.y - tile_begin.y);
}

// Move the underlying mosaic mesh around the window while the window stays put in the world
// In other words, move the mesh while its stencil buffer stays static on screen
void SDHRWindow::AdjustView(iXY _mesh_pos)
{
	tile_begin.x = _mesh_pos.x;
	tile_begin.y = _mesh_pos.y;
	mesh->SetWorldCoordinates(screen_begin.x - tile_begin.x, screen_begin.y - tile_begin.y);
}

void SDHRWindow::Update()
{
	if (enabled)
	{
		if (mesh)
			mesh->updateMesh();
	}
};

// NOTE: This (and any methods with OpenGL calls) must be called from the main thread
// NOTE: It assumes the textures have been already bound to GL_TEXTURE0... GL_TEXTURE16
void SDHRWindow::Render(const glm::mat4& mat_camera, const glm::mat4& mat_proj)
{
	if (enabled) {
		if (mesh) {
			mesh->Draw(mat_camera, mat_proj);
		}
	}
}

