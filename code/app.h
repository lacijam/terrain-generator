#ifndef APP_H
#define APP_H

#include "types.h"
#include "maths.h"

#define Kilobytes(value) ((value) * 1024)
#define Megabytes(value) (Kilobytes(value) * 1024)
#define Gigabytes(value) (Megabytes(value) * 1024)

struct app_button_state {
    bool32 ended_down;
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
        };
    };
};

struct app_input {
    app_keyboard_input keyboard;
};

struct app_memory {
    bool32 is_initialized;
    u64 permenant_storage_size;
    void *permenant_storage;
};

struct app_window_info {
    u32 w, h;
    bool resize;
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

struct app_state {
    u32 vao, vbo, ebo;
    u32 program, lighting_program;
    u32 lvao, lvbo;
    u32 transform_loc;
    u32 object_colour_loc;
    u32 model_loc;
    u32 light_pos_loc;

    static const u32 chunk_height = 300;
    static const u32 chunk_width = 300;

    V3 light_pos;

    Camera cur_cam;
};

#endif