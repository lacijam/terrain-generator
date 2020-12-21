#include "app.h"

#include <assert.h>
#include <stdlib.h>

#include "maths.h"
#include "win32-opengl.h"
#include "opengl-util.h"
#include "perlin.h"
#include "shaders.h"

static void camera_init(Camera *cam)
{
	cam->pos = { 0.f, 0.f, 0.f };
	cam->front = { 0.f, 0.f, -1.f };
	cam->up = { 0.f, 1.f, 0.f };
	mat4_identity(cam->view);
	mat4_identity(cam->frustrum);
	cam->yaw = 0;
	cam->pitch = 0;
	cam->vel = 100.f;
	cam->look_speed = 100.f;
}

static void camera_frustrum(Camera *cam, u32 cx, u32 cy)
{
	assert(cam && cx > 0 && cy > 0);
	real32 near_clip = 0.01f;
	real32 far_clip = 1000.f;
	real32 fov_y = (90.f * (real32)M_PI / 180.f);
	real32 aspect = (real32)cx / cy;
	real32 top = near_clip * tanf(fov_y / 2);
	real32 bottom = -1 * top;
	real32 left = bottom * aspect;
	real32 right = top * aspect;
	mat4_frustrum(cam->frustrum, left, right, bottom, top, near_clip, far_clip);
}

static void camera_update(Camera *cam)
{
	if (cam->pitch > 89.f) {
		cam->pitch = 89.f;
	} else if (cam->pitch <= -89.f) {
		cam->pitch = -89.f;
	}

	V3 direction;
	direction.x = cosf(radians(cam->yaw)) * cosf(radians(cam->pitch));
	direction.y = sinf(radians(cam->pitch));
	direction.z = sinf(radians(cam->yaw)) * cosf(radians(cam->pitch));
	cam->front = v3_normalise(direction);
}

static inline void camera_move_forward(Camera *cam, real32 dt)
{
	cam->pos += cam->vel * dt * cam->front;
}

static inline void camera_move_backward(Camera *cam, real32 dt)
{
	cam->pos -= cam->vel * dt * cam->front;
}

static inline void camera_move_left(Camera *cam, real32 dt)
{
	cam->pos -= v3_normalise(v3_cross(cam->front, cam->up)) * cam->vel * dt;
}

static inline void camera_move_right(Camera *cam, real32 dt)
{
	cam->pos += v3_normalise(v3_cross(cam->front, cam->up)) * cam->vel * dt;
}

static inline void camera_look_at(Camera *cam)
{
	mat4_look_at(cam->view, cam->pos, cam->pos + cam->front, cam->up);
}

struct Mesh {
    real32 *positions;
	real32 *normals;
    real32 *indices;
};

static void app_generate_terrain_mesh(app_state *state)
{
	// Calculate vertices.
	struct Vertex {
		V3 pos;
		V3 nor;
	};

	Vertex *vertices = (Vertex*)malloc(state->chunk_height * state->chunk_width * sizeof(Vertex));
	for (u32 j = 0; j < state->chunk_height; j++) {
		for (u32 i = 0; i < state->chunk_width; i++) {
			u32 index = j * state->chunk_width + i;
			// Map between 0..1 for perlin function.
			real32 x = 0 + (real32)i / state->chunk_width;
			real32 y = 0 + (real32)j / state->chunk_height;

			real32 a, b, c, d;
			a = perlin({0, 0});
			b = perlin({1, 0});
			c = perlin({0, 1});
			d = perlin({1, 1});

			real32 height = 1 * perlin({ 1 * x, 1 * y })
				+ 0.5f * perlin({ 2 * x, 2 * y })
				+ 0.25f * perlin({ 4 * x, 4 * y })
				+ 0.125f * perlin({ 8 * x, 8 * y })
				+ 0.0625f * perlin({ 16 * x, 16 * y })
				+ 0.03125f * perlin({ 32 * x, 32 * y })
				+ 0.0015625f * perlin({ 64 * x, 64 * y });

			vertices[index].pos.x = i;
			vertices[index].pos.y = height * 50;
			vertices[index].pos.z = j;
			vertices[index].nor = {};
		}
	}

	// Calculate indices.
	struct QuadIndices {
		u32 i[6];
	};

	const u32 row_quads = state->chunk_height - 1;
	const u32 col_quads = state->chunk_width - 1;
	QuadIndices *indices = (QuadIndices*)malloc(row_quads * col_quads * sizeof(QuadIndices));
	for (u32 j = 0; j < row_quads; j++) {
		for (u32 i = 0; i < col_quads; i++) {
			u32 pos = j * col_quads + i;
			indices[pos].i[0] = j * state->chunk_width + i;
			indices[pos].i[1] = j * state->chunk_width + i + 1;
			indices[pos].i[2] = (j + 1) * state->chunk_width + i;
			
			indices[pos].i[3] = j * state->chunk_width + i + 1;
			indices[pos].i[4] = (j + 1) * state->chunk_width + i + 1;
			indices[pos].i[5] = (j + 1) * state->chunk_width + i;
		}
	}

	// Calculate normals for each vertex for each sum normals of surrounding.
	for (u32 j = 0; j < row_quads; j++) {
		for (u32 i = 0; i < col_quads; i++) {
			u32 pos = j * col_quads + i;
			for (u32 tri = 0; tri < 2; tri++) {
				Vertex *a = &vertices[indices[pos].i[tri * 3 + 0]];
				Vertex *b = &vertices[indices[pos].i[tri * 3 + 1]];
				Vertex *c = &vertices[indices[pos].i[tri * 3 + 2]];
				V3 cp = v3_cross(b->pos - a->pos, c->pos - a->pos);
				cp = cp * -1.f;
				a->nor += cp;
				b->nor += cp;
				c->nor += cp;
			}
		}
	}

	// Average sum of normals for each vertex.
	for (u32 j = 0; j < state->chunk_height; j++) {
		for (u32 i = 0; i < state->chunk_width; i++) {
			u32 index = j * state->chunk_width + i;
			vertices[index].nor = v3_normalise(vertices[index].nor);
		}
	}

	glGenVertexArrays(1, &state->vao);
	glBindVertexArray(state->vao);

	glGenBuffers(1, &state->vbo);
	glBindBuffer(GL_ARRAY_BUFFER, state->vbo);
	glBufferData(GL_ARRAY_BUFFER, state->chunk_height * state->chunk_width * sizeof Vertex, vertices, GL_STATIC_DRAW);

	free(vertices);

	glGenBuffers(1, &state->ebo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, state->ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, row_quads * col_quads * sizeof QuadIndices, indices, GL_STATIC_DRAW);

	free(indices);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof Vertex, (void*)0);
	glEnableVertexAttribArray(0);

	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof Vertex, (void*)(3 * sizeof(real32)));
	glEnableVertexAttribArray(1);

	// temp light vao.
	glGenVertexArrays(1, &state->lvao);
	glBindVertexArray(state->lvao);

	real32 light_verts[9] = {
		-0.5f, -0.5f, 0.0,
		 0.5f, -0.5f, 0.0,
		 0.0f,  0.5f, 0.0
	};

	glGenBuffers(1, &state->lvbo);
	glBindBuffer(GL_ARRAY_BUFFER, state->lvbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof light_verts, light_verts, GL_STATIC_DRAW);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(real32), (void*)0);
	glEnableVertexAttribArray(0);
}

void app_init_shaders(app_state *state)
{
    u32 default_vertex_shader;
	u32 default_fragment_shader;

	state->program = glCreateProgram();
	default_vertex_shader = gl_compile_shader_from_source(Shaders::DEFAULT_VERTEX_SHADER_SOURCE, state->program, GL_VERTEX_SHADER);
	default_fragment_shader = gl_compile_shader_from_source(Shaders::DEFAULT_FRAGMENT_SHADER_SOURCE, state->program, GL_FRAGMENT_SHADER);

	glAttachShader(state->program, default_vertex_shader);
	glAttachShader(state->program, default_fragment_shader);
	glLinkProgram(state->program);
    glUseProgram(state->program);
    if (!gl_check_program_link_log(state->program)) {
		// Error!
	}

	state->transform_loc = glGetUniformLocation(state->program, "transform");
	state->object_colour_loc = glGetUniformLocation(state->program, "object_colour");
	state->model_loc = glGetUniformLocation(state->program, "model");
	state->light_pos_loc = glGetUniformLocation(state->program, "light_pos");

	state->lighting_program = glCreateProgram();
	u32 light_vertex_shader = gl_compile_shader_from_source(Shaders::LIGHT_VERTEX_SHADER_SOURCE, state->lighting_program, GL_VERTEX_SHADER);
	u32 light_fragment_shader = gl_compile_shader_from_source(Shaders::LIGHT_FRAGMENT_SHADER_SOURCE, state->lighting_program, GL_FRAGMENT_SHADER);
	glAttachShader(state->lighting_program, light_vertex_shader);
	glAttachShader(state->lighting_program, light_fragment_shader);
	glLinkProgram(state->lighting_program);
    glUseProgram(state->lighting_program);
    if (!gl_check_program_link_log(state->lighting_program)) {
		// Error
	}

	glDeleteShader(default_vertex_shader);
	glDeleteShader(light_vertex_shader);
	glDeleteShader(light_fragment_shader);
	glDeleteShader(default_fragment_shader);
}

static void app_on_destroy(app_state *state)
{
	glBindVertexArray(0);

	glDeleteBuffers(1, &state->ebo);
	glDeleteBuffers(1, &state->vbo);
	glDeleteVertexArrays(1, &state->vao);

	glDeleteBuffers(1, &state->lvbo);
	glDeleteVertexArrays(1, &state->lvao);

    glDeleteProgram(state->program);
    glDeleteProgram(state->lighting_program);
}

static void app_render_terrain(app_state *state)
{
    glClearColor(0.2f, 0.2f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glUseProgram(state->program);
	glBindVertexArray(state->vao);

    real32 terrain_colour[3] = { 0.1f, 0.3f, 0.1f };

    glUniform3fv(state->object_colour_loc, 1, terrain_colour);
    glUniform3fv(state->light_pos_loc, 1, (GLfloat*)(&state->light_pos));

    for (u32 j = 0; j < 2; j++) {
        for (u32 i = 0; i < 2; i++) {
            real32 model[16];
            mat4_identity(model);
            mat4_translate(model, i * state->chunk_width, 0.f, j * state->chunk_height);

            glUniformMatrix4fv(state->model_loc, 1, GL_FALSE, model);

            real32 mv[16], mvp[16];
            mat4_identity(mv);
            mat4_identity(mvp);
            mat4_multiply(mv, state->cur_cam.view, model);
            mat4_multiply(mvp, state->cur_cam.frustrum, mv);
            glUniformMatrix4fv(state->transform_loc, 1, GL_FALSE, mvp);

            glDrawElements(GL_TRIANGLES, state->chunk_width * state->chunk_height * 6, GL_UNSIGNED_INT, NULL);
        }
    }
}

static void app_render_lights(app_state *state)
{	
	glUseProgram(state->lighting_program);

	glBindVertexArray(state->lvao);

	real32 model[16];
	mat4_identity(model);
	mat4_translate(model, state->light_pos.x, state->light_pos.y, state->light_pos.z);

	real32 mv[16], mvp[16];
	mat4_identity(mv);
	mat4_identity(mvp);
	mat4_multiply(mv, state->cur_cam.view, model);
	mat4_multiply(mvp, state->cur_cam.frustrum, mv);
	glUniformMatrix4fv(glGetUniformLocation(state->lighting_program, "transform"), 1, GL_FALSE, mvp);
	glDrawArrays(GL_TRIANGLES, 0, 3);
}

void app_update_and_render(real32 dt, app_input *input, app_memory *memory, app_window_info *window_info)
{
	app_state *state = (app_state*)memory->permenant_storage;
	if (!memory->is_initialized) {
		app_init_shaders(state);
		app_generate_terrain_mesh(state);
		state->light_pos = {state->chunk_width / 2, 200.f, state->chunk_height / 2};
		
		camera_init(&state->cur_cam);
		state->cur_cam.pos = { -20.f, 50.f, -20.f};
		state->cur_cam.front = { 1.f, 0.f, 1.f};
		
		glViewport(0, 0, window_info->w, window_info->h);
		camera_frustrum(&state->cur_cam, window_info->w, window_info->h);

		memory->is_initialized = true;
	}

	if (window_info->resize) {
		glViewport(0, 0, window_info->w, window_info->h);
		camera_frustrum(&state->cur_cam, window_info->w, window_info->h);
	}

    app_keyboard_input *keyboard = &input->keyboard;
    if (keyboard->forward.ended_down) {
        camera_move_forward(&state->cur_cam, dt);
    } else if (keyboard->backward.ended_down) {
        camera_move_backward(&state->cur_cam, dt);
    }

    if (keyboard->left.ended_down) {
        camera_move_left(&state->cur_cam, dt);
    } else if (keyboard->right.ended_down) {
        camera_move_right(&state->cur_cam, dt);
    }

    if (keyboard->cam_up.ended_down) {
        state->cur_cam.pitch += state->cur_cam.look_speed * dt;
    } else if (keyboard->cam_down.ended_down) {
        state->cur_cam.pitch -= state->cur_cam.look_speed * dt;
    }

    if (keyboard->cam_left.ended_down) {
        state->cur_cam.yaw -= state->cur_cam.look_speed * dt;
    } else if (keyboard->cam_right.ended_down) {
        state->cur_cam.yaw += state->cur_cam.look_speed * dt;
    }

    camera_update(&state->cur_cam);
    camera_look_at(&state->cur_cam);
    
    app_render_terrain(state);
	app_render_lights(state);
}