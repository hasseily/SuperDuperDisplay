#include "SDHRWindow.h"


void SDHRWindow::Reset()
{
	enabled = 0;
	black_or_wrap = true;
	screen_count = uXY({ 0,0 });
	screen_begin = iXY({ 0,0 });
	tile_begin = iXY({ 0,0 });
	tile_dim = uXY({ 0,0 });
	tile_count = uXY({ 0,0 });

	if (mesh) {
		delete mesh;
		mesh = nullptr;
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

	tile_begin.x %= (tile_count.x * tile_dim.x);
	tile_begin.y %= (tile_count.y * tile_dim.y);

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

	tile_begin.x %= (tile_count.x * tile_dim.x);
	tile_begin.y %= (tile_count.y * tile_dim.y);
	mesh->SetWorldCoordinates(screen_begin.x - tile_begin.x, screen_begin.y - tile_begin.y);
}

void SDHRWindow::SetSize(uXY _size)
{
	screen_count = _size;
}

void SDHRWindow::Update()
{
	if (mesh)
		mesh->updateMesh();
};

// NOTE: This (and any methods with OpenGL calls) must be called from the main thread
// NOTE: It assumes the textures have been already bound to _SDHR_TEXTURE_UNITS_START forward
void SDHRWindow::Render(const glm::mat4& mat_camera, const glm::mat4& mat_proj)
{
	if (enabled) {
		if (mesh) {
			glm::vec2 window_topleft = glm::vec2(tile_begin.x, tile_begin.y);
			glm::vec2 window_bottomright = window_topleft + glm::vec2(screen_count.x, screen_count.y);

			mesh->SetupDraw();
			mesh->shaderProgram->setVec2("windowTopLeft", window_topleft);
			mesh->shaderProgram->setVec2("windowBottomRight", window_bottomright);
			mesh->shaderProgram->setInt("anim_ms_frame", anim_ms_frame);

			GLenum glerr;
			if ((glerr = glGetError()) != GL_NO_ERROR) {
				std::cerr << "SDHRWindow draw error: " << glerr << std::endl;
			}

			// draw the main mesh
			mesh->Draw(mat_camera, mat_proj);

			if (this->black_or_wrap)
			{
				// if it wraps, draw the meshes around it that matter
				glm::vec2 msize = glm::vec2(mesh->width, mesh->height);
				glm::mat4 mat_trans;
				bool isX = false;
				bool isY = false;
				if (window_bottomright.x > msize.x)
				{
					// need to draw right
					mat_trans = glm::translate(glm::mat4(1.0f), glm::vec3(msize.x, 0.f, 0.0f));
					mesh->Draw(mat_camera * mat_trans, mat_proj);
					isX = true;
				}
				if (window_bottomright.y > msize.y)
				{
					// need to draw right
					mat_trans = glm::translate(glm::mat4(1.0f), glm::vec3(0.f, msize.y, 0.0f));
					mesh->Draw(mat_camera * mat_trans, mat_proj);
					isY = true;
				}
				if (isX && isY)
				{
					// Need to draw bottom right
					mat_trans = glm::translate(glm::mat4(1.0f), glm::vec3(msize.x, msize.y, 0.0f));
					mesh->Draw(mat_camera * mat_trans, mat_proj);
					isY = true;
				}
			}
		}
	}
}

