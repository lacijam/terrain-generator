#pragma once

#define Kilobytes(value) ((value) * 1024)
#define Megabytes(value) (Kilobytes(value) * 1024)
#define Gigabytes(value) (Megabytes(value) * 1024)

struct game_button_state {
    bool32 ended_down;
};

struct game_keyboard_input {
    union {
        game_button_state buttons[8];
        struct {
            game_button_state forward;
            game_button_state backward;
            game_button_state left;
            game_button_state right;
            game_button_state cam_up;
            game_button_state cam_down;
            game_button_state cam_left;
            game_button_state cam_right;
        };
    };
};

struct game_input {
    game_keyboard_input keyboard;
};

struct game_memory {
    bool32 is_initialized;
    u64 permenant_storage_size;
    void *permenant_storage;
};

struct game_window_info {
    u32 w, h;
    bool resize;
};

static void app_update_and_render(float dt, game_input *input, game_window_info *window_info);

struct game_state {
    unsigned vao, vbo, ebo;
    unsigned a_pos, a_nor;
    unsigned program, lighting_program;
    unsigned transform_loc;
    unsigned object_colour_loc;
    unsigned model_loc;
    unsigned light_pos_loc;

    static const u32 chunk_height = 100;
    static const u32 chunk_width = 100;

    Camera cur_cam;
};