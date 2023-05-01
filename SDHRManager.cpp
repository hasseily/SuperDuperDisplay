#include "SDHRManager.h"
#include <cstring>
#include <zlib.h>
#include <iostream>
#include <fstream>
#include <sstream>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <algorithm>
#ifdef _DEBUGTIMINGS
#include <chrono>
#endif

// below because "The declaration of a static data member in its class definition is not a definition"
SDHRManager* SDHRManager::s_instance;

//////////////////////////////////////////////////////////////////////////
// Commands structs
//////////////////////////////////////////////////////////////////////////

#pragma pack(push)
#pragma pack(1)

struct UploadDataCmd {
	uint16_t dest_block;
	uint16_t source_addr;
};

struct UploadDataFilenameCmd {
	uint8_t dest_addr_med;
	uint8_t dest_addr_high;
	uint8_t filename_length;
	uint8_t filename[];
};

struct DefineImageAssetCmd {
	uint8_t asset_index;
	uint16_t block_count;
};

struct DefineImageAssetFilenameCmd {
	uint8_t asset_index;
	uint8_t filename_length;
	uint8_t filename[];  // don't include the trailing null either in the data or counted in the filename_length
};

struct DefineTilesetCmd {
	uint8_t tileset_index;
	uint8_t asset_index;
	uint8_t num_entries;
	uint16_t xdim;
	uint16_t ydim;
	uint16_t block_count;
};

struct DefineTilesetImmediateCmd {
	uint8_t tileset_index;
	uint8_t num_entries;
	uint8_t xdim;
	uint8_t ydim;
	uint8_t asset_index;
	uint8_t data[];  // data is 4-byte records, 16-bit x and y offsets (scaled by x/ydim), from the given asset
};

struct DefineWindowCmd {
	uint8_t window_index;
	uint16_t screen_xcount;		// width in pixels of visible screen area of window
	uint16_t screen_ycount;
	uint16_t tile_xdim;			// xy dimension, in pixels, of tiles in the window.
	uint16_t tile_ydim;
	uint16_t tile_xcount;		// xy dimension, in tiles, of the tile array
	uint16_t tile_ycount;
};

struct UpdateWindowSetImmediateCmd {
	uint8_t window_index;
	uint16_t data_length;
};

struct UpdateWindowSetUploadCmd {
	uint8_t window_index;
	uint16_t block_count;
};

struct UpdateWindowShiftTilesCmd {
	uint8_t window_index;
	int8_t x_dir; // +1 shifts tiles right by 1, negative shifts tiles left by 1, zero no change
	int8_t y_dir; // +1 shifts tiles down by 1, negative shifts tiles up by 1, zero no change
};

struct UpdateWindowSetWindowPositionCmd {
	uint8_t window_index;
	int32_t screen_xbegin;
	int32_t screen_ybegin;
};

struct UpdateWindowAdjustWindowViewCommand {
	uint8_t window_index;
	int32_t tile_xbegin;
	int32_t tile_ybegin;
};

struct UpdateWindowEnableCmd {
	uint8_t window_index;
	uint8_t enabled;
};

#pragma pack(pop)

//////////////////////////////////////////////////////////////////////////
// Image Asset Methods
//////////////////////////////////////////////////////////////////////////

void SDHRManager::ImageAsset::AssignByFilename(const char* filename) {
	int width;
	int height;
	int channels;
	data = stbi_load(filename, &width, &height, &channels, 4);
	if (data == NULL) {
		// image failed to load
		SDHRManager::GetInstance()->error_flag = true;
		return;
	}
	image_xcount = width;
	image_ycount = height;
}

void SDHRManager::ImageAsset::AssignByMemory(SDHRManager* owner, const uint8_t* buffer, uint64_t size) {
	int width;
	int height;
	int channels;
	data = stbi_load_from_memory(buffer, size, &width, &height, &channels, 4);
	if (data == NULL) {
		owner->CommandError(stbi_failure_reason());
		owner->error_flag = true;
		return;
	}
	image_xcount = width;
	image_ycount = height;
}

void SDHRManager::ImageAsset::ExtractTile(SDHRManager* owner, uint32_t* tile_p, uint16_t tile_xdim, uint16_t tile_ydim, uint64_t xsource, uint64_t ysource) {
	uint32_t* dest_p = tile_p;
	if (xsource + tile_xdim > image_xcount ||
		ysource + tile_ydim > image_ycount) {
		owner->CommandError("ExtractTile out of bounds");
		owner->error_flag = true;
		return;
	}

	// Extracting from RGBA to RGBA. No rewriring of channels necessary
	uint32_t* data32 = reinterpret_cast<uint32_t*>(data);
	for (uint64_t y = 0; y < tile_ydim; ++y) {
		uint64_t source_yoffset = (ysource + y) * image_xcount;
		for (uint64_t x = 0; x < tile_xdim; ++x) {
			uint64_t pixel_offset = source_yoffset + (xsource + x);
			uint32_t dest_pixel = data32[pixel_offset];
			*dest_p = dest_pixel;
			++dest_p;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// Static Methods
//////////////////////////////////////////////////////////////////////////

int upload_inflate(const char* source, uint64_t size, std::ostream& dest) {
	static const int CHUNK = 16384;
	int ret;
	unsigned have;
	z_stream strm;
	unsigned char in[CHUNK];
	unsigned char out[CHUNK];

	/* allocate inflate state */
	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	strm.opaque = Z_NULL;
	strm.avail_in = 0;
	strm.next_in = Z_NULL;
	ret = inflateInit2(&strm, (15 + 32));
	if (ret != Z_OK)
		return ret;

	/* decompress until deflate stream ends or end of file */
	uint64_t bytes_read = 0;
	while (bytes_read < size) {
		uint64_t bytes_to_read = std::min((uint64_t)CHUNK, size - bytes_read);
		memcpy(in, source + bytes_read, bytes_to_read);
		bytes_read += bytes_to_read;
		strm.avail_in = bytes_to_read;
		if (strm.avail_in == 0)
			break;
		strm.next_in = in;

		/* run inflate() on input until output buffer not full */
		do {
			strm.avail_out = CHUNK;
			strm.next_out = out;
			ret = inflate(&strm, Z_NO_FLUSH);
			assert(ret != Z_STREAM_ERROR);  /* state not clobbered */
			switch (ret) {
			case Z_NEED_DICT:
				ret = Z_DATA_ERROR;     /* and fall through */
			case Z_DATA_ERROR:
			case Z_MEM_ERROR:
				(void)inflateEnd(&strm);
				return ret;
			}
			have = CHUNK - strm.avail_out;
			dest.write((char*)out, have);
		} while (strm.avail_out == 0);

		/* done when inflate() says it's done */
	} while (ret != Z_STREAM_END);

	/* clean up and return */
	(void)inflateEnd(&strm);
	return ret == Z_STREAM_END ? Z_OK : Z_DATA_ERROR;
}

//////////////////////////////////////////////////////////////////////////
// Methods
//////////////////////////////////////////////////////////////////////////

void SDHRManager::Initialize()
{
	m_bEnabled = false;
	error_flag = false;
	memset(error_str, 0, sizeof(error_str));
	memset(uploaded_data_region, 0, sizeof(uploaded_data_region));
	*image_assets = {};
	*tileset_records = {};
	*windows = {};

	cpubuffer = (uint32_t*)malloc(640 * 360 * 4);

	command_buffer.clear();
	command_buffer.reserve(64 * 1024);

	// Initialize the Apple 2 memory duplicate
	// Whenever memory is written from the Apple2
	// in the main bank between $200 and $BFFF it will
	// be sent through the socket and this buffer will be updated
	if (a2mem == NULL)
		a2mem = new uint8_t[0xbfff];	// anything below $200 is unused
	memset(a2mem, 0, 0xbfff);

}

SDHRManager::~SDHRManager()
{
	for (uint16_t i = 0; i < 256; ++i) {
		if (image_assets[i].data) {
			stbi_image_free(image_assets[i].data);
		}
		if (tileset_records[i].tile_data) {
			free(tileset_records[i].tile_data);
		}
		if (windows[i].tilesets) {
			free(windows[i].tilesets);
		}
		if (windows[i].tile_indexes) {
			free(windows[i].tile_indexes);
		}
	}
	free(cpubuffer);
	delete[] a2mem;
}

void SDHRManager::AddPacketDataToBuffer(uint8_t data)
{
	command_buffer.push_back(data);
}

void SDHRManager::ClearBuffer()
{
	command_buffer.clear();
}

void SDHRManager::CommandError(const char* err) {
	strcpy(error_str, err);
	error_flag = true;
	std::cerr << "Command Error: " << error_str << std::endl;
}

bool SDHRManager::CheckCommandLength(uint8_t* p, uint8_t* e, size_t sz) {
	size_t command_length = e - p;
	if (command_length < sz) {
		CommandError("Insufficient buffer space");
		return false;
	}
	return true;
}

uint32_t SDHRManager::ARGB555_to_ARGB888(uint16_t argb555) {
	uint8_t r = (argb555 >> 10) & 0x1F;
	uint8_t g = (argb555 >> 5) & 0x1F;
	uint8_t b = argb555 & 0x1F;
	uint8_t a = (argb555 & 0x8000);	// alpha in RGB555 is all or nothing

	uint32_t r888 = (r << 3) | (r >> 2);
	uint32_t g888 = (g << 3) | (g >> 2);
	uint32_t b888 = (b << 3) | (b >> 2);
	uint32_t a888 = a * 0xFF;

	uint32_t argb888 = (a888 << 24) | (r888 << 16) | (g888 << 8) | b888;
	return argb888;
}

uint8_t* SDHRManager::GetApple2MemPtr()
{
	return a2mem;
}

void SDHRManager::DefineTileset(uint8_t tileset_index, uint16_t num_entries, uint8_t xdim, uint8_t ydim,
	ImageAsset* asset, uint8_t* offsets) {
	uint64_t store_data_size = (uint64_t)xdim * ydim * sizeof(uint32_t) * num_entries;
	TilesetRecord* r = tileset_records + tileset_index;
	if (r->tile_data) {
		free(r->tile_data);
	}
	*r = {};
	r->xdim = xdim;
	r->ydim = ydim;
	r->num_entries = num_entries;
	r->tile_data = (uint32_t*)malloc(store_data_size);
#ifdef DEBUG
	std::cout << "Allocating tile data size: " << store_data_size << " for index: " << (uint32_t)tileset_index << std::endl;
#endif

	uint8_t* offset_p = offsets;
	uint32_t* dest_p = r->tile_data;
	for (uint64_t i = 0; i < num_entries; ++i) {
		uint64_t xoffset = *((uint16_t*)offset_p);
		offset_p += 2;
		uint64_t yoffset = *((uint16_t*)offset_p);
		offset_p += 2;
		uint64_t asset_xoffset = xoffset * xdim;
		uint64_t asset_yoffset = yoffset * xdim;
		asset->ExtractTile(this, dest_p, xdim, ydim, asset_xoffset, asset_yoffset);
		dest_p += (uint64_t)xdim * ydim;
	}
}

/**
 * Commands in the buffer look like:
 * First 2 bytes are the command length (excluding these bytes)
 * 3rd byte is the command id
 * Anything after that is the command's packed struct,
 * for example UpdateWindowEnableCmd.
 * So the buffer of UpdateWindowEnable will look like:
 * {03, 00, 13, 0, 1} to enable window 0
*/

bool SDHRManager::ProcessCommands(void)
{
	if (error_flag) {
		return false;
	}
	if (command_buffer.empty()) {
		//nothing to do
		return true;
	}
	uint8_t* begin = &command_buffer[0];
	uint8_t* end = begin + command_buffer.size();
	uint8_t* p = begin;

#ifdef DEBUG
	// std::cerr << "Command buffer size: " << command_buffer.size() << std::endl;
#endif

	while (p < end) {
		// Header (2 bytes) giving the size in bytes of the command
		if (!CheckCommandLength(p, end, 2)) {
			std::cerr << "CheckCommandLength failed!" << std::endl;
			return false;
		}
		uint16_t message_length = *((uint16_t*)p);
		if (!CheckCommandLength(p, end, message_length)) return false;
		p += 2;
		// Command ID (1 byte)
		uint8_t cmd = *p++;
		// Command data (variable)
		switch (cmd) {
		case SDHR_CMD_UPLOAD_DATA: {
			if (!CheckCommandLength(p, end, sizeof(UploadDataCmd))) return false;
			UploadDataCmd* cmd = (UploadDataCmd*)p;
			uint64_t dest_offset = (uint64_t)cmd->dest_block * 512;
			uint64_t data_size = (uint64_t)512;
			if (!DataSizeCheck(dest_offset, data_size)) {
				std::cerr << "DataSizeCheck failed!" << std::endl;
				return false;
			}
			/*
			std::cout << std::hex << "Uploaded from: " << (uint64_t)(cmd->source_addr) 
				<< " To: " << (uint64_t)(uploaded_data_region + dest_offset)
				<< " Amount: " << std::dec << (uint64_t)data_size
				<< " Destination Block: " << (uint64_t)cmd->dest_block
				<< std::endl;
			*/
			memcpy(uploaded_data_region + dest_offset, a2mem + ((uint16_t)cmd->source_addr), data_size);
#ifdef DEBUG
			// std::cout << "SDHR_CMD_UPLOAD_DATA: Success: " << std::hex << data_size << std::endl;
#endif
		} break;
		case SDHR_CMD_DEFINE_IMAGE_ASSET: {
			if (!CheckCommandLength(p, end, sizeof(DefineImageAssetCmd))) return false;
			DefineImageAssetCmd* cmd = (DefineImageAssetCmd*)p;
			uint64_t upload_start_addr = 0;
			uint64_t upload_data_size = (uint64_t)cmd->block_count * 512;

			ImageAsset* r = image_assets + cmd->asset_index;

			if (r->data != NULL) {
				stbi_image_free(r->data);
			}
			r->AssignByMemory(this, uploaded_data_region + upload_start_addr, upload_data_size);
			if (error_flag) {
				std::cerr << "AssignByMemory failed!" << std::endl;
				return false;
			}
#ifdef DEBUG
			std::cout << "SDHR_CMD_DEFINE_IMAGE_ASSET: Success:" << r->image_xcount << " x " << r->image_ycount << std::endl;
#endif
		} break;
		case SDHR_CMD_DEFINE_IMAGE_ASSET_FILENAME: {
			std::cerr << "SDHR_CMD_DEFINE_IMAGE_ASSET_FILENAME: Not Implemented." << std::endl;
			// NOT IMPLEMENTED
		} break;
		case SDHR_CMD_UPLOAD_DATA_FILENAME: {
			std::cerr << "SDHR_CMD_UPLOAD_DATA_FILENAME: Not Implemented." << std::endl;
			// NOT IMPLEMENTED
		} break;
		case SDHR_CMD_DEFINE_TILESET: {
			if (!CheckCommandLength(p, end, sizeof(DefineTilesetCmd))) return false;
			DefineTilesetCmd* cmd = (DefineTilesetCmd*)p;
			uint16_t num_entries = cmd->num_entries;
			if (num_entries == 0) {
				num_entries = 256;
			}
			uint64_t required_data_size = num_entries * 4;
			if (cmd->block_count * 512 < required_data_size) {
				CommandError("Insufficient data space for tileset");
			}
			ImageAsset* asset = image_assets + cmd->asset_index;
			DefineTileset(cmd->tileset_index, num_entries, cmd->xdim, cmd->ydim, asset, uploaded_data_region);
#ifdef DEBUG
			std::cout << "SDHR_CMD_DEFINE_TILESET: Success! " << (uint32_t)cmd->tileset_index << ';'<< (uint32_t)num_entries << std::endl;
#endif
		} break;
		case SDHR_CMD_DEFINE_TILESET_IMMEDIATE: {
			if (!CheckCommandLength(p, end, sizeof(DefineTilesetImmediateCmd))) return false;
			DefineTilesetImmediateCmd* cmd = (DefineTilesetImmediateCmd*)p;
			uint16_t num_entries = cmd->num_entries;
			if (num_entries == 0) {
				num_entries = 256;
			}
			uint64_t load_data_size;
			load_data_size = (uint64_t)num_entries * 4;
			if (message_length != sizeof(DefineTilesetImmediateCmd) + load_data_size) {
				CommandError("DefineTilesetImmediate data size mismatch");
				return false;
			}
			ImageAsset* asset = image_assets + cmd->asset_index;
			DefineTileset(cmd->tileset_index, num_entries, cmd->xdim, cmd->ydim, asset, cmd->data);
#ifdef DEBUG
			std::cout << "SDHR_CMD_DEFINE_TILESET_IMMEDIATE: Success! " << (uint32_t)cmd->tileset_index << ';' << (uint32_t)num_entries << std::endl;
#endif
		} break;
		case SDHR_CMD_DEFINE_WINDOW: {
			if (!CheckCommandLength(p, end, sizeof(DefineWindowCmd))) return false;
			DefineWindowCmd* cmd = (DefineWindowCmd*)p;
			Window* r = windows + cmd->window_index;
			if (r->screen_xcount > screen_xcount) {
				CommandError("Window exceeds max x resolution");
				return false;
			}
			if (r->screen_ycount > screen_ycount) {
				CommandError("Window exceeds max y resolution");
				return false;
			}
			r->enabled = false;
			r->screen_xcount = cmd->screen_xcount;
			r->screen_ycount = cmd->screen_ycount;
			r->screen_xbegin = 0;
			r->screen_ybegin = 0;
			r->tile_xbegin = 0;
			r->tile_ybegin = 0;
			r->tile_xdim = cmd->tile_xdim;
			r->tile_ydim = cmd->tile_ydim;
			r->tile_xcount = cmd->tile_xcount;
			r->tile_ycount = cmd->tile_ycount;
			if (r->tilesets) {
				free(r->tilesets);
			}
			r->tilesets = (uint8_t*)malloc(r->tile_xcount * r->tile_ycount);
			if (r->tile_indexes) {
				free(r->tile_indexes);
			}
			r->tile_indexes = (uint8_t*)malloc(r->tile_xcount * r->tile_ycount);
#ifdef DEBUG
			std::cout << "SDHR_CMD_DEFINE_WINDOW: Success! " 
				<< cmd->window_index << ';' << (uint32_t)r->tile_xcount << ';' << (uint32_t)r->tile_ycount << std::endl;
#endif
		} break;
		case SDHR_CMD_UPDATE_WINDOW_SET_IMMEDIATE: {
			size_t cmd_sz = sizeof(UpdateWindowSetImmediateCmd);
			if (!CheckCommandLength(p, end, cmd_sz)) return false;
			UpdateWindowSetImmediateCmd* cmd = (UpdateWindowSetImmediateCmd*)p;
			Window* r = windows + cmd->window_index;

			// full tile specification: tileset and index
			uint64_t required_data_size = (uint64_t)r->tile_xcount * r->tile_ycount * 2;
			if (required_data_size != cmd->data_length) {
				CommandError("UpdateWindowSetImmediate data size mismatch");
				return false;
			}
			if (!CheckCommandLength(p, end, cmd_sz + cmd->data_length)) return false;
			uint8_t* sp = p + cmd_sz;
			for (uint64_t i = 0; i < cmd->data_length / 2; ++i) {
				uint8_t tileset_index = sp[i * 2];
				uint8_t tile_index = sp[i * 2 + 1];
				if (tileset_records[tileset_index].xdim != r->tile_xdim ||
					tileset_records[tileset_index].ydim != r->tile_ydim ||
					tileset_records[tileset_index].num_entries <= tile_index) {
					CommandError("invalid tile specification");
					return false;
				}
				r->tilesets[i] = tileset_index;
				r->tile_indexes[i] = tile_index;
			}
			p += cmd->data_length;
#ifdef DEBUG
			std::cout << "SDHR_CMD_UPDATE_WINDOW_SET_IMMEDIATE: Success!" << std::endl;
#endif
		} break;
		case SDHR_CMD_UPDATE_WINDOW_SET_UPLOAD: {
			if (!CheckCommandLength(p, end, sizeof(UpdateWindowSetUploadCmd))) return false;
			UpdateWindowSetUploadCmd* cmd = (UpdateWindowSetUploadCmd*)p;
			Window* r = windows + cmd->window_index;
			// full tile specification: tileset and index
			uint64_t data_size = (uint64_t)cmd->block_count * 512;
			std::stringstream ss;
			upload_inflate((const char*)uploaded_data_region, data_size, ss);
			std::string s = ss.str();
			if (s.length() != r->tile_xcount * r->tile_ycount * 2) {
				CommandError("UploadWindowSetUpload data insufficient to define window tiles");
			}
			uint8_t* sp = (uint8_t*)s.c_str();
			for (uint64_t tile_y = 0; tile_y < r->tile_ycount; ++tile_y) {
				uint64_t line_offset = (uint64_t)tile_y * r->tile_xcount;
				for (uint64_t tile_x = 0; tile_x < r->tile_xcount; ++tile_x) {
					uint8_t tileset_index = *sp++;
					uint8_t tile_index = *sp++;
					if (tileset_records[tileset_index].xdim != r->tile_xdim ||
						tileset_records[tileset_index].ydim != r->tile_ydim ||
						tileset_records[tileset_index].num_entries <= tile_index) {
						CommandError("invalid tile specification");
						return false;
					}
					r->tilesets[line_offset + tile_x] = tileset_index;
					r->tile_indexes[line_offset + tile_x] = tile_index;
				}
			}
#ifdef DEBUG
			std::cout << "SDHR_CMD_UPDATE_WINDOW_SET_UPLOAD: Success!" << std::endl;
#endif
		} break;
/*
		case SDHR_CMD_UPDATE_WINDOW_SINGLE_TILESET: {
			if (!CheckCommandLength(p, end, sizeof(UpdateWindowSingleTilesetCmd))) return false;
			UpdateWindowSingleTilesetCmd* cmd = (UpdateWindowSingleTilesetCmd*)p;
			Window* r = windows + cmd->window_index;
			if ((uint64_t)cmd->tile_xbegin + cmd->tile_xcount > r->tile_xcount ||
				(uint64_t)cmd->tile_ybegin + cmd->tile_ycount > r->tile_ycount) {
				CommandError("tile update region exceeds tile dimensions");
				return false;
			}
			// partial tile specification: index and palette, single tileset
			uint64_t data_size = (uint64_t)cmd->tile_xcount * cmd->tile_ycount;
			if (data_size + sizeof(UpdateWindowSingleTilesetCmd) != message_length) {
				CommandError("UpdateWindowSingleTileset data size mismatch");
				return false;
			}
			uint8_t* dp = cmd->data;
			for (uint64_t tile_y = 0; tile_y < cmd->tile_ycount; ++tile_y) {
				uint64_t line_offset = (cmd->tile_ybegin + tile_y) * r->tile_xcount + cmd->tile_xbegin;
				for (uint64_t tile_x = 0; tile_x < cmd->tile_xcount; ++tile_x) {
					uint8_t tile_index = *dp++;
					if (tileset_records[cmd->tileset_index].xdim != r->tile_xdim ||
						tileset_records[cmd->tileset_index].ydim != r->tile_ydim ||
						tileset_records[cmd->tileset_index].num_entries <= tile_index) {
						CommandError("invalid tile specification");
						return false;
					}
					r->tilesets[line_offset + tile_x] = cmd->tileset_index;
					r->tile_indexes[line_offset + tile_x] = tile_index;
				}
			}
			std::cout << "SDHR_CMD_UPDATE_WINDOW_SINGLE_TILESET: Success!" << std::endl;
		} break;
*/
		case SDHR_CMD_UPDATE_WINDOW_SHIFT_TILES: {
			if (!CheckCommandLength(p, end, sizeof(UpdateWindowShiftTilesCmd))) return false;
			UpdateWindowShiftTilesCmd* cmd = (UpdateWindowShiftTilesCmd*)p;
			Window* r = windows + cmd->window_index;
			if (cmd->x_dir < -1 || cmd->x_dir > 1 || cmd->y_dir < -1 || cmd->y_dir > 1) {
				CommandError("invalid tile shift");
				return false;
			}
			if (r->tile_xcount == 0 || r->tile_ycount == 0) {
				CommandError("invalid window for tile shift");
				return false;
			}
			if (cmd->x_dir == -1) {
				for (uint64_t y_index = 0; y_index < r->tile_ycount; ++y_index) {
					uint64_t line_offset = y_index * r->tile_xcount;
					for (uint64_t x_index = 1; x_index < r->tile_xcount; ++x_index) {
						r->tilesets[line_offset + x_index - 1] = r->tilesets[line_offset + x_index];
						r->tile_indexes[line_offset + x_index - 1] = r->tile_indexes[line_offset + x_index];
					}
				}
			}
			else if (cmd->x_dir == 1) {
				for (uint64_t y_index = 0; y_index < r->tile_ycount; ++y_index) {
					uint64_t line_offset = y_index * r->tile_xcount;
					for (uint64_t x_index = r->tile_xcount - 1; x_index > 0; --x_index) {
						r->tilesets[line_offset + x_index] = r->tilesets[line_offset + x_index - 1];
						r->tile_indexes[line_offset + x_index] = r->tile_indexes[line_offset + x_index - 1];
					}
				}
			}
			if (cmd->y_dir == -1) {
				for (uint64_t y_index = 1; y_index < r->tile_ycount; ++y_index) {
					uint64_t line_offset = y_index * r->tile_xcount;
					uint64_t prev_line_offset = line_offset - r->tile_xcount;
					for (uint64_t x_index = 0; x_index < r->tile_xcount; ++x_index) {
						r->tilesets[prev_line_offset + x_index] = r->tilesets[line_offset + x_index];
						r->tile_indexes[prev_line_offset + x_index] = r->tile_indexes[line_offset + x_index];
					}
				}
			}
			else if (cmd->y_dir == 1) {
				for (uint64_t y_index = r->tile_ycount - 1; y_index > 0; --y_index) {
					uint64_t line_offset = y_index * r->tile_xcount;
					uint64_t prev_line_offset = line_offset - r->tile_xcount;
					for (uint64_t x_index = 0; x_index < r->tile_xcount; ++x_index) {
						r->tilesets[line_offset + x_index] = r->tilesets[prev_line_offset + x_index];
						r->tile_indexes[line_offset + x_index] = r->tile_indexes[prev_line_offset + x_index];
					}
				}
			}
#ifdef DEBUG
			std::cout << "SDHR_CMD_UPDATE_WINDOW_SHIFT_TILES: Success! " 
				<< (uint32_t)cmd->window_index << ';' << (uint32_t)cmd->x_dir << ';' << (uint32_t)cmd->y_dir << std::endl;
#endif
		} break;
		case SDHR_CMD_UPDATE_WINDOW_SET_WINDOW_POSITION: {
			if (!CheckCommandLength(p, end, sizeof(UpdateWindowSetWindowPositionCmd))) return false;
			UpdateWindowSetWindowPositionCmd* cmd = (UpdateWindowSetWindowPositionCmd*)p;
			Window* r = windows + cmd->window_index;
			r->screen_xbegin = cmd->screen_xbegin;
			r->screen_ybegin = cmd->screen_ybegin;
#ifdef DEBUG
			std::cout << "SDHR_CMD_UPDATE_WINDOW_SET_WINDOW_POSITION: Success! "
				<< (uint32_t)cmd->window_index << ';' << (uint32_t)cmd->screen_xbegin << ';' << (uint32_t)cmd->screen_ybegin << std::endl;
#endif
		} break;
		case SDHR_CMD_UPDATE_WINDOW_ADJUST_WINDOW_VIEW: {
			if (!CheckCommandLength(p, end, sizeof(UpdateWindowAdjustWindowViewCommand))) return false;
			UpdateWindowAdjustWindowViewCommand* cmd = (UpdateWindowAdjustWindowViewCommand*)p;
			Window* r = windows + cmd->window_index;
			r->tile_xbegin = cmd->tile_xbegin;
			r->tile_ybegin = cmd->tile_ybegin;
#ifdef DEBUG
			std::cout << "SDHR_CMD_UPDATE_WINDOW_ADJUST_WINDOW_VIEW: Success! "
				<< (uint32_t)cmd->window_index << ';' << (uint32_t)cmd->tile_xbegin << ';' << (uint32_t)cmd->tile_ybegin << std::endl;
#endif
		} break;
		case SDHR_CMD_UPDATE_WINDOW_ENABLE: {
			if (!CheckCommandLength(p, end, sizeof(UpdateWindowEnableCmd))) return false;
			UpdateWindowEnableCmd* cmd = (UpdateWindowEnableCmd*)p;
			Window* r = windows + cmd->window_index;
			if (!r->tile_xcount || !r->tile_ycount) {
				CommandError("cannote enable empty window");
				return false;
			}
			r->enabled = cmd->enabled;
#ifdef DEBUG
			std::cout << "SDHR_CMD_UPDATE_WINDOW_ENABLE: Success! "
				<< (uint32_t)cmd->window_index << std::endl;
#endif
		} break;
		default:
			CommandError("unrecognized command");
			return false;
		}
		p += message_length - 3;
	}
	return true;
}

void SDHRManager::DrawWindowsIntoScreenImage()
{
#ifdef _DEBUGTIMINGS
	using std::chrono::high_resolution_clock;
	using std::chrono::duration_cast;
	using std::chrono::duration;
	using std::chrono::milliseconds;

	duration<double, std::milli> ms_double2;
	auto t1 = high_resolution_clock::now();
#endif

#ifdef DEBUG
	// std::cout << "Entered DrawWindowsIntoScreenImage" << std::endl;
#endif

	glBindTexture(GL_TEXTURE_2D, screen_image.texture_id);
	// Setup filtering parameters for display
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); // This is required on WebGL for non power-of-two textures
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); // Same

	// Draw the windows into the bound texture
	uint32_t pixel_color_rgba = 0;
	for (uint16_t window_index = 0; window_index < 256; ++window_index) {
		Window* w = windows + window_index;
		if (!w->enabled) {
			continue;
		}

#if defined(GL_UNPACK_ROW_LENGTH) && !defined(__EMSCRIPTEN__)
		glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#endif

		for (int64_t tile_y = w->tile_ybegin; tile_y < w->tile_ybegin + (int64_t)w->screen_ycount; ++tile_y) {
			int64_t adj_tile_y = tile_y;
			int64_t tile_yspan = (int64_t)w->tile_ycount * w->tile_ydim;
			while (adj_tile_y < 0) adj_tile_y += tile_yspan;
			while (adj_tile_y >= tile_yspan) adj_tile_y -= tile_yspan;
			uint64_t tile_yindex = adj_tile_y / w->tile_ydim;
			uint64_t tile_yoffset = adj_tile_y % w->tile_ydim;
			for (int64_t tile_x = w->tile_xbegin; tile_x < w->tile_xbegin + (int64_t)w->screen_xcount; ++tile_x) {
				// check if destination pixel is offscreen
				int64_t screen_y = tile_y + w->screen_ybegin - w->tile_ybegin;
				int64_t screen_x = tile_x + w->screen_xbegin - w->tile_xbegin;
				if (screen_x < 0 || screen_y < 0 || screen_x > screen_xcount || screen_y > screen_ycount) {
					// destination pixel is offscreen, do not draw
					continue;
				}
				int64_t adj_tile_x = tile_x;
				int64_t tile_xspan = (int64_t)w->tile_xcount * w->tile_xdim;
				while (adj_tile_x < 0) adj_tile_x += tile_xspan;
				while (adj_tile_x >= tile_xspan) adj_tile_x -= tile_xspan;
				uint64_t tile_xindex = adj_tile_x / w->tile_xdim;
				uint64_t tile_xoffset = adj_tile_x % w->tile_xdim;
				uint64_t entry_index = tile_yindex * w->tile_xcount + tile_xindex;
				TilesetRecord* t = tileset_records + w->tilesets[entry_index];
				uint64_t tile_index = w->tile_indexes[entry_index];
				pixel_color_rgba = t->tile_data[tile_index * t->xdim * t->ydim + tile_yoffset * t->xdim + tile_xoffset];
				/*
				if ((pixel_color_rgba & 0x0000FF) == 0) {
					continue; // zero alpha, don'd draw
				}
				*/

#ifdef DEBUG
				// std::cout << std::dec << screen_x << "," << screen_y << " >> " << std::hex << pixel_color_rgba << std::endl;
#endif
				// Where's the pixel?
				int64_t screen_offset = ((640 * screen_y) + (screen_x));
				cpubuffer[screen_offset] = pixel_color_rgba;
				// glTexSubImage2D(GL_TEXTURE_2D, 0, screen_x, screen_y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &pixel_color_rgba);
			}
		}

#ifdef DEBUG
		std::cout << "Drew into buffer window " << window_index << std::endl;
#endif
	}

#ifdef _DEBUGTIMINGS
	auto t2 = high_resolution_clock::now();
	duration<double, std::milli> ms_double = t2 - t1;
	std::cout << "DrawWindowsIntoBuffer() duration: " << ms_double.count() << "ms\n";
	std::cout << "Framebuffer write: " << ms_double2.count() << "ms\n";
#endif
}
