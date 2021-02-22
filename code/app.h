#ifndef APP_H
#define APP_H

#include <thread>

#include "types.h"
#include "maths.h"
#include "camera.h"

#define Kilobytes(value) ((value) * 1024ULL)
#define Megabytes(value) (Kilobytes(value) * 1024ULL)
#define Gigabytes(value) (Megabytes(value) * 1024ULL)

struct app_button_state {
    bool32 started_down;
    bool32 ended_down;
    bool32 toggled;
};

struct app_keyboard_input {
    union {
        app_button_state buttons[8];
        struct {
            app_button_state forward;
            app_button_state backward;
            app_button_state left;
            app_button_state right;
            app_button_state cam_up;
            app_button_state cam_down;
            app_button_state cam_left;
            app_button_state cam_right;
            app_button_state wireframe;
            app_button_state reset;
            app_button_state fly;
        };
    };
};

struct app_input {
    app_keyboard_input keyboard;
};

struct app_memory {
    bool32 is_initialized;
    u64 free_offset;
    u64 permenant_storage_size;
    void *permenant_storage;
};

struct app_window_info {
    u32 w, h;
    bool32 resize, running;
};

struct world_generation_parameters {
    real32 scale;
    real32 lacunarity;
    real32 persistence;
    real32 elevation_power;
    real32 y_scale;
    real32 sand_height;
    real32 snow_height;
    real32 ambient_strength;
    real32 diffuse_strength;
    real32 specular_strength; 
    V3 water_pos;
    V3 grass_colour;
    V3 sand_colour;
    V3 snow_colour;
    V3 slope_colour;
    V3 water_colour;
    V3 light_colour;
    V3 skybox_colour;
    s32 max_octaves;
    u32 tree_count;
    u32 tree_min_height;
    u32 tree_max_height;
    u32 max_trees;
};

struct TerrainShader {
    u32 program;
    u32 projection;
    u32 view;
    u32 model;
    u32 plane;
    u32 ambient_strength;
    u32 diffuse_strength;
    u32 specular_strength;
    u32 light_pos;
    u32 light_colour;
    u32 grass_colour;
    u32 slope_colour;
    u32 sand_colour;
    u32 snow_colour;
    u32 sand_height;
    u32 snow_height;
    u32 view_position;
};

struct SimpleShader {
    u32 program;
    u32 projection;
    u32 view;
    u32 model;
    u32 object_colour;
    u32 plane;
};

struct WaterShader {
    u32 program;
    u32 projection;
    u32 view;
    u32 model;
    u32 reflection_texture;
    u32 refraction_texture;
    u32 water_colour;
};

struct WaterFrameBuffers {
    static const u32 REFLECTION_WIDTH = 640;
	static const u32 REFLECTION_HEIGHT = 360;
	static const u32 REFRACTION_WIDTH = 1280;
	static const u32 REFRACTION_HEIGHT = 720;

    u32 reflection_fbo;
    u32 reflection_texture;
    u32 refraction_fbo;
    u32 refraction_texture;
};

struct RGB {
    u8 r, g, b;
};

struct TextureMapData {
    static const u32 MAX_RESOLUTION = 16384;
    u32 resolution;
    u32 fbo, texture;
    RGB *pixels;
};

struct Vertex {
    V3 pos;
    V3 nor;
};

struct QuadIndices {
	u32 i[6];
};

struct LODDataInfo {
    QuadIndices *quads;
    u32 quads_count;
    u64 data_offset;
};

struct Chunk {
    Vertex* vertices;
    Vertex* optimized_vertices;
    bool32* searched; // Used to check if a vertex has been checned when optimizing.
    QuadIndices* lods;
    LODDataInfo *lod_data_infos;
    u64 lod_indices_count;
    V3 world_pos;
    u64 vertices_count;
    u32 x, y;
    u32 vbo, ebo;
};

struct ExportSettings {
    bool32 with_normals;
    bool32 texture_map;
    bool32 lods;
};

struct LODSettings {
    u32 *details;
    u32 max_details_count; // Size of the array
    u32 max_available_count; // Number of possible LODs for the chunk size.
    u32 details_in_use; // Current amount of LODs being used.
    u32 detail_multiplier;
};

struct app_state {
    app_window_info window_info;

    TerrainShader terrain_shader;
    SimpleShader simple_shader;
    WaterShader water_shader;

    world_generation_parameters custom_parameters;
    world_generation_parameters green_plains_parameters;
    world_generation_parameters rugged_desert_parameters;
    world_generation_parameters harsh_mountains_parameters;
    world_generation_parameters *params;

    ExportSettings export_settings;
    TextureMapData texture_map_data;

    Camera cur_cam;

    WaterFrameBuffers water_frame_buffers;

    std::thread *generation_threads;

    LODSettings lod_settings;
    Chunk *chunks;
    Chunk* current_chunk;
    V3* trees;
    u32 chunk_count;
    u32 chunk_tile_length;
    u32 chunk_vertices_length;
    u32 world_width;
    u32 world_area;
    u32 world_tile_length;
    V3 light_pos;
    
    u32 triangle_vao, quad_vbo, quad_ebo;

    bool32 wireframe;
    bool32 flying;
};

extern void app_update_and_render(real32 dt, app_input *input, app_memory *memory, app_window_info *window_info);

#endif