#ifndef APP_H
#define APP_H

#include <thread>
#include <random>
#include <vector>
#include <array>

#include "types.h"
#include "maths.h"
#include "camera.h"
#include "object.h"

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
    real32 x_offset;
    real32 z_offset;
    real32 scale;
    real32 lacunarity;
    real32 persistence;
    real32 elevation_power;
    real32 y_scale;
    real32 sand_height;
    real32 stone_height;
    real32 snow_height;
    real32 ambient_strength;
    real32 diffuse_strength;
    real32 specular_strength;
    real32 gamma_correction;
    real32 rock_size;
    real32 tree_size;
    V3 water_pos;
    V3 ground_colour;
    V3 sand_colour;
    V3 stone_colour;
    V3 snow_colour;
    V3 slope_colour;
    V3 water_colour;
    V3 light_colour;
    V3 skybox_colour;
    V3 rock_colour;
    V3 trunk_colour;
    V3 leaves_colour;
    s32 max_octaves;
    u32 chunk_tile_length;
    u32 world_width;
    u32 seed;
    u32 tree_count;
    u32 tree_min_height;
    u32 tree_max_height;
    u32 rock_count;
    u32 rock_min_height;
    u32 rock_max_height;
};

struct preset_file {
    std::string name;
    u32 index;
    world_generation_parameters params;
};

struct TerrainShader {
    u32 program;
    u32 projection;
    u32 view;
    u32 model;
    u32 light_space_matrix;
    u32 shadow_map;
    u32 plane;
    u32 ambient_strength;
    u32 diffuse_strength;
    u32 specular_strength;
    u32 gamma_correction;
    u32 light_pos;
    u32 light_colour;
    u32 ground_colour;
    u32 slope_colour;
    u32 sand_colour;
    u32 stone_colour;
    u32 snow_colour;
    u32 sand_height;
    u32 stone_height;
    u32 snow_height;
    u32 view_position;
};

struct SimpleShader {
    u32 program;
    u32 projection;
    u32 view;
    u32 model;
    u32 light_space_matrix;
    u32 shadow_map;
    u32 ambient_strength;
    u32 diffuse_strength;
    u32 gamma_correction;
    u32 light_pos;
    u32 light_colour;
    u32 object_colour;
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

struct DepthShader {
    u32 program;
    u32 projection;
    u32 view;
    u32 model;
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
    std::vector<Vertex> vertices;
    std::vector<QuadIndices> lods;
    std::vector<LODDataInfo> lod_data_infos;
    u64 lod_indices_count;
    u64 vertices_count;
    u32 x, y;
    u32 vbo, ebo;
};

struct ExportSettings {
    bool32 with_normals;
    bool32 texture_map;
    bool32 bake_shadows;
    bool32 lods;
    bool32 seperate_chunks;
    bool32 trees;
    bool32 rocks;
};

struct LODSettings {
    u32 *details;
    u32 max_details_count; // Size of the array
    u32 max_available_count; // Number of possible LODs for the chunk size.
    u32 max_detail_multiplier; // The maximum multiplier to generate details.
    u32 details_in_use; // Current amount of LODs being used.
    u32 detail_multiplier;
};

struct app_state {
    app_window_info window_info;

    TerrainShader terrain_shader;
    SimpleShader simple_shader;
    WaterShader water_shader;
    DepthShader depth_shader;

    std::vector<preset_file*> presets;
    preset_file cur_preset;

    ExportSettings export_settings;
    TextureMapData texture_map_data;
    TextureMapData specular_map_data;

    Camera cur_cam;

    Object *rock;
    Object *trunk;
    Object *leaves;

    WaterFrameBuffers water_frame_buffers;

    std::vector<std::thread> generation_threads;
    std::mt19937 rng;

    LODSettings lod_settings;
    std::vector<Chunk*> chunks;
    Chunk* current_chunk;
    std::vector<V3> trees_pos, trees_rotation;
    std::vector<V3> rocks_pos, rocks_rotation;
    u32 chunk_count;
    u32 chunk_vertices_length;
    u32 world_area;
    u32 world_tile_length;
    V3 light_pos;
    
    u32 triangle_vao, quad_vbo, quad_ebo;
    u32 depth_map_fbo, depth_map;

    bool32 wireframe;
    
    bool general_settings_open;
    bool terrain_settings_open;
    bool show_filename_prompt;
    bool is_typing;

    std::string new_preset_name;
};

extern void app_update_and_render(real32 dt, app_state *state, app_input *input, app_window_info *window_info);
extern app_state *app_init(u32 w, u32 h);

#endif