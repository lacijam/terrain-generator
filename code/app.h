#ifndef APP_H
#define APP_H

#include "types.h"
#include "maths.h"

#define Kilobytes(value) ((value) * 1024)
#define Megabytes(value) (Kilobytes(value) * 1024)
#define Gigabytes(value) (Megabytes(value) * 1024)

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
            app_button_state gen_terrace;
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
    bool resize;
};

struct app_perlin_params {
    real32 scale;
    real32 lacunarity;
    real32 persistence;
    real32 elevation_power;
    real32 y_scale;
    real32 water_height;
    real32 sand_height;
    real32 snow_height;
    V3 grass_colour;
    V3 sand_colour;
    V3 snow_colour;
    V3 slope_colour;
    V3 water_colour;
    V3 light_colour;
    real32 ambient_strength;
    real32 diffuse_strength;
    s32 terrace_levels;
    s32 max_octaves;
    bool tectonic;
};

extern void app_update_and_render(real32 dt, app_input *input, app_memory *memory, app_window_info *window_info);

struct Camera {
	V3 pos;
	V3 front;
	V3 up;
	real32 view[16];
	real32 frustrum[16];
	real32 yaw, pitch;
	real32 vel;
	real32 look_speed;
};

struct TerrainShader {
    u32 program;
    u32 projection;
    u32 view;
    u32 model;
    u32 plane;
    u32 ambient_strength;
    u32 diffuse_strength;
    u32 light_pos;
    u32 light_colour;
    u32 grass_colour;
    u32 slope_colour;
    u32 sand_colour;
    u32 snow_colour;
    u32 sand_height;
    u32 snow_height;
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
    static const u32 REFLECTION_WIDTH = 320;
	static const u32 REFLECTION_HEIGHT = 180;

	static const u32 REFRACTION_WIDTH = 1280;
	static const u32 REFRACTION_HEIGHT = 720;

    u32 reflection_fbo;
    u32 reflection_texture;
    u32 reflection_depth;

    u32 refraction_fbo;
    u32 refraction_texture;
    u32 refraction_depth_texture;
};

struct app_state {
    u32 vao, vbo, ebo;
    u32 simple_vao, quad_vbo, quad_ebo;

    TerrainShader terrain_shader;
    SimpleShader simple_shader;
    WaterShader water_shader;

    WaterFrameBuffers water_frame_buffers;

    bool32 wireframe;
    bool32 terrace;

    static const u32 chunk_height = 300;
    static const u32 chunk_width = 300;

    V3 light_pos;

    Camera cur_cam;
};

#endif