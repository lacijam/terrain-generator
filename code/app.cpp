#include "app.h"

#include <assert.h>
#include <stdlib.h>

#include "maths.h"
#include "win32-opengl.h"
#include "opengl-util.h"
#include "perlin.h"
#include "shaders.h"

#include "imgui-master/imgui.h"
#include "imgui-master/imgui_impl_opengl3.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

static app_perlin_params perlin_params;

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
	real32 near_clip = 1.f;
	real32 far_clip = 1000.f;
	real32 fov_y = (50.f * (real32)M_PI / 180.f);
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

static real32 noise(V2 p) {
	return 0.5f + perlin(p);
}

static float pattern(V2 p)
{
	V2 q;
	V2 a = { 0.f, 0.f };
	V2 b = { 5.2f, 1.3f };
	q.x = noise(p + 4.f * a);
	q.y = noise(p + 4.f * b);
	return noise(p + 4.f * q);
}

static void app_generate_terrain_chunk(
	app_state *state
	, app_memory *memory
	, u32 chunk_x
	, u32 chunk_y)
{
	// Calculate vertices.
	struct Vertex {
		V3 pos;
		V3 nor;
	};

	assert(state->chunk_height * state->chunk_width * sizeof(Vertex) < memory->permenant_storage_size - memory->free_offset);

	Vertex *vertices = (Vertex*)((char*)(memory->permenant_storage) + memory->free_offset);
	u64 vertices_size = state->chunk_height * state->chunk_width * sizeof(Vertex);

	for (u32 j = 0; j < state->chunk_height; j++) {
		for (u32 i = 0; i < state->chunk_width; i++) {
			u32 index = j * state->chunk_width + i;

			real32 x = chunk_x + (real32)i / perlin_params.scale;
			real32 y = chunk_y + (real32)j / perlin_params.scale;

			real32 total = 0.f;
			real32 frequency = 1.f;
			real32 amplitude = 1.f;
			real32 total_amplitude = 0;

			for (u32 octave = 0; octave < perlin_params.max_octaves; octave++) {
				if (perlin_params.tectonic) {
					total += pattern({ frequency * x, frequency * y}) * amplitude;
				} else {
					total += noise({ frequency * x, frequency * y}) * amplitude;
				}
				total_amplitude += amplitude;
				amplitude *= perlin_params.persistence;
				frequency *= perlin_params.lacunarity;
			}

			real32 octave_result = total / total_amplitude;

			if (state->terrace) {
				octave_result = (floor(octave_result * perlin_params.terrace_levels) / (real32)perlin_params.terrace_levels);
			}

			const real32 elevation = (powf(octave_result, perlin_params.elevation_power)) * perlin_params.y_scale * perlin_params.scale;

			vertices[index].pos.x = i;
			vertices[index].pos.y = elevation;
			vertices[index].pos.z = j;
			vertices[index].nor = {};
		}
	}

	real32 max = vertices[0].pos.y;
	real32 min = vertices[0].pos.y;
    for (u32 i = 1; i < state->chunk_height * state->chunk_width; i++) {
        if (vertices[i].pos.y > max) 
			max = vertices[i].pos.y;
		if (vertices[i].pos.y < min) 
			min = vertices[i].pos.y;
	}

	printf("min %f max %f\n", min, max);

	struct QuadIndices {
		u32 i[6];
	};

	const u32 row_quads = state->chunk_height - 1;
	const u32 col_quads = state->chunk_width - 1;

	assert(row_quads * col_quads * sizeof(QuadIndices) < memory->permenant_storage_size - memory->free_offset - vertices_size);

	QuadIndices *indices = (QuadIndices*)((char*)(memory->permenant_storage) + memory->free_offset + vertices_size);
	u64 indices_size = row_quads * col_quads * sizeof(QuadIndices);

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

	glGenBuffers(1, &state->ebo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, state->ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, row_quads * col_quads * sizeof QuadIndices, indices, GL_STATIC_DRAW);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof Vertex, (void*)0);
	glEnableVertexAttribArray(0);

	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof Vertex, (void*)(3 * sizeof(real32)));
	glEnableVertexAttribArray(1);

	glGenVertexArrays(1, &state->simple_vao);
	glBindVertexArray(state->simple_vao);

	real32 quad_verts[12] = {
        -0.5f, 0.5f, 0.f,    
        -0.5f, -0.5f, 0.f, 
        0.5f, -0.5f, 0.f, 
        0.5f, 0.5f, 0.f
	};

	u32 quad_indices[6] = {
        0, 1, 2,
        2, 3, 0
	};

	glGenBuffers(1, &state->quad_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, state->quad_vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof quad_verts, quad_verts, GL_STATIC_DRAW);

	glGenBuffers(1, &state->quad_ebo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, state->quad_ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof quad_indices, quad_indices, GL_STATIC_DRAW);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(real32), (void*)0);
	glEnableVertexAttribArray(0);
}

static u32 create_shader(const char *vertex_shader_source, const char *fragment_shader_source)
{
	u32 program_id = glCreateProgram();
	u32 vertex_shader = gl_compile_shader_from_source(vertex_shader_source, program_id, GL_VERTEX_SHADER);
	u32 fragment_shader = gl_compile_shader_from_source(fragment_shader_source, program_id, GL_FRAGMENT_SHADER);
	glAttachShader(program_id, vertex_shader);
	glAttachShader(program_id, fragment_shader);
	glLinkProgram(program_id);
    glUseProgram(program_id);
    if (!gl_check_program_link_log(program_id)) {

	}

	glDeleteShader(vertex_shader);
	glDeleteShader(fragment_shader);

	return program_id;
}

static void app_init_shaders(app_state *state)
{
	state->terrain_shader.program = create_shader(Shaders::DEFAULT_VERTEX_SHADER_SOURCE, Shaders::DEFAULT_FRAGMENT_SHADER_SOURCE);

	state->terrain_shader.projection = glGetUniformLocation(state->terrain_shader.program, "projection");
	state->terrain_shader.view = glGetUniformLocation(state->terrain_shader.program, "view");
	state->terrain_shader.model = glGetUniformLocation(state->terrain_shader.program, "model");
	state->terrain_shader.model = glGetUniformLocation(state->terrain_shader.program, "model");
	state->terrain_shader.light_pos = glGetUniformLocation(state->terrain_shader.program, "light_pos");
	state->terrain_shader.plane = glGetUniformLocation(state->terrain_shader.program, "plane");
	state->terrain_shader.sand_height = glGetUniformLocation(state->terrain_shader.program, "sand_height");
	state->terrain_shader.snow_height = glGetUniformLocation(state->terrain_shader.program, "snow_height");

	state->terrain_shader.light_colour = glGetUniformLocation(state->terrain_shader.program, "light_colour");
	state->terrain_shader.grass_colour = glGetUniformLocation(state->terrain_shader.program, "grass_colour");
	state->terrain_shader.slope_colour = glGetUniformLocation(state->terrain_shader.program, "slope_colour");
	state->terrain_shader.sand_colour = glGetUniformLocation(state->terrain_shader.program, "sand_colour");
	state->terrain_shader.snow_colour = glGetUniformLocation(state->terrain_shader.program, "snow_colour");

	state->terrain_shader.ambient_strength = glGetUniformLocation(state->terrain_shader.program, "ambient_strength");
	state->terrain_shader.diffuse_strength = glGetUniformLocation(state->terrain_shader.program, "diffuse_strength");

	state->simple_shader.program = create_shader(Shaders::SIMPLE_VERTEX_SHADER_SOURCE, Shaders::SIMPLE_FRAGMENT_SHADER_SOURCE);

	state->simple_shader.projection = glGetUniformLocation(state->simple_shader.program, "projection");
	state->simple_shader.view = glGetUniformLocation(state->simple_shader.program, "view");
	state->simple_shader.model = glGetUniformLocation(state->simple_shader.program, "model");
	state->simple_shader.plane = glGetUniformLocation(state->simple_shader.program, "plane");

	state->water_shader.program = create_shader(Shaders::WATER_VERTEX_SHADER_SOURCE, Shaders::WATER_FRAGMENT_SHADER_SOURCE);

	state->water_shader.projection = glGetUniformLocation(state->water_shader.program, "projection");
	state->water_shader.view = glGetUniformLocation(state->water_shader.program, "view");
	state->water_shader.model = glGetUniformLocation(state->water_shader.program, "model");
	state->water_shader.reflection_texture = glGetUniformLocation(state->water_shader.program, "reflection_texture");
	state->water_shader.refraction_texture = glGetUniformLocation(state->water_shader.program, "refraction_texture");
	state->water_shader.water_colour = glGetUniformLocation(state->water_shader.program, "water_colour");
}

static u32 create_framebuffer_texture(u32 width, u32 height)
{
	u32 tex;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, tex, 0);
	return tex;
}

// static u32 create_framebuffer_texture_from_depth(u32 width, u32 height)
// {
// 	u32 tex;
// 	glGenTextures(1, &tex);
// 	glBindTexture(GL_TEXTURE_2D, tex);
// 	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, 0);
// 	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
// 	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
// 	glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, tex, 0);
// 	return tex;
// }

// static u32 create_renderbuffer_from_depth(u32 width, u32 height)
// {
// 	u32 depth_buffer;
// 	glGenRenderbuffers(1, &depth_buffer);
// 	glBindRenderbuffer(GL_RENDERBUFFER, depth_buffer);
// 	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, width, height);
// 	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth_buffer);
// 	return depth_buffer;
// }

static void init_water_data(app_state *state)
{
	WaterFrameBuffers wfb;

	glGenFramebuffers(1, &wfb.reflection_fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, wfb.reflection_fbo);
	glDrawBuffer(GL_COLOR_ATTACHMENT0);

	wfb.reflection_texture = create_framebuffer_texture(wfb.REFLECTION_WIDTH, wfb.REFLECTION_HEIGHT);
	//wfb.reflection_depth = create_renderbuffer_from_depth(REFLECTION_WIDTH, REFLECTION_HEIGHT);
	
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glGenFramebuffers(1, &wfb.refraction_fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, wfb.refraction_fbo);
	glDrawBuffer(GL_COLOR_ATTACHMENT0);

	wfb.refraction_texture = create_framebuffer_texture(wfb.REFRACTION_WIDTH, wfb.REFRACTION_HEIGHT);
	//wfb.refraction_depth_texture = create_framebuffer_texture_from_depth(REFRACTION_WIDTH, REFRACTION_HEIGHT);
	
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	state->water_frame_buffers = wfb;
}

static void app_on_destroy(app_state *state)
{
	glBindVertexArray(0);

	glDeleteBuffers(1, &state->ebo);
	glDeleteBuffers(1, &state->vbo);
	glDeleteVertexArrays(1, &state->vao);

	glDeleteBuffers(1, &state->quad_vbo);
	glDeleteBuffers(1, &state->quad_ebo);
	glDeleteVertexArrays(1, &state->simple_vao);

    glDeleteProgram(state->terrain_shader.program);
    glDeleteProgram(state->simple_shader.program);
    glDeleteProgram(state->water_shader.program);
}

static void app_render_terrain(app_state *state, real32 *clip)
{
	glUseProgram(state->terrain_shader.program);
	glBindVertexArray(state->vao);

	glUniform4fv(state->terrain_shader.plane, 1, clip);

    glUniform1f(state->terrain_shader.ambient_strength, perlin_params.ambient_strength);
    glUniform1f(state->terrain_shader.diffuse_strength, perlin_params.diffuse_strength);

    glUniform3fv(state->terrain_shader.light_pos, 1, (GLfloat*)(&state->light_pos));
    glUniform1f(state->terrain_shader.sand_height, perlin_params.sand_height);
    glUniform1f(state->terrain_shader.snow_height, perlin_params.snow_height);

    glUniform3fv(state->terrain_shader.light_colour, 1, (GLfloat*)&perlin_params.light_colour);
    glUniform3fv(state->terrain_shader.slope_colour, 1, (GLfloat*)&perlin_params.slope_colour);
    glUniform3fv(state->terrain_shader.grass_colour, 1, (GLfloat*)&perlin_params.grass_colour);
    glUniform3fv(state->terrain_shader.sand_colour, 1, (GLfloat*)&perlin_params.sand_colour);
    glUniform3fv(state->terrain_shader.snow_colour, 1, (GLfloat*)&perlin_params.snow_colour);

	real32 model[16];
	mat4_identity(model);

	glUniformMatrix4fv(state->terrain_shader.projection, 1, GL_FALSE, state->cur_cam.frustrum);
	glUniformMatrix4fv(state->terrain_shader.view, 1, GL_FALSE, state->cur_cam.view);
	glUniformMatrix4fv(state->terrain_shader.model, 1, GL_FALSE, model);

	glDrawElements(GL_TRIANGLES, state->chunk_width * state->chunk_height * 6, GL_UNSIGNED_INT, NULL);

	if (state->wireframe) {
		glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
    	real32 terrain_colour[3] = { 1.0f, 1.0f, 1.0f };
    	glUniform3fv(state->terrain_shader.grass_colour, 1, (GLfloat*)&perlin_params.grass_colour);
		glDrawElements(GL_TRIANGLES, state->chunk_width * state->chunk_height * 6, GL_UNSIGNED_INT, NULL);
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	}
}

static void app_render_lights(app_state *state)
{	
	glUseProgram(state->simple_shader.program);

	glBindVertexArray(state->simple_vao);

	real32 model[16];
	mat4_identity(model);
	mat4_translate(model, state->light_pos.x, state->light_pos.y, state->light_pos.z);

	glUniformMatrix4fv(state->simple_shader.projection, 1, GL_FALSE, state->cur_cam.frustrum);
	glUniformMatrix4fv(state->simple_shader.view, 1, GL_FALSE, state->cur_cam.view);
	glUniformMatrix4fv(state->simple_shader.model, 1, GL_FALSE, model);
	glDrawArrays(GL_TRIANGLES, 0, 3);
}

static void app_render_water(app_state *state)
{
	glUseProgram(state->water_shader.program);
	glBindVertexArray(state->simple_vao);

	real32 model[16];

	mat4_identity(model);
	mat4_translate(model, state->chunk_width / 2, perlin_params.water_height, state->chunk_width / 2);
	mat4_scale(model, state->chunk_width, 1.f, state->chunk_width);
	mat4_rotate_x(model, 90.f);

	glUniformMatrix4fv(state->water_shader.projection, 1, GL_FALSE, state->cur_cam.frustrum);
	glUniformMatrix4fv(state->water_shader.view, 1, GL_FALSE, state->cur_cam.view);
	glUniformMatrix4fv(state->water_shader.model, 1, GL_FALSE, model);
	
    glUniform3fv(state->water_shader.water_colour, 1, (GLfloat*)&perlin_params.water_colour);

	glUniform1i(state->water_shader.reflection_texture, 0);
	glUniform1i(state->water_shader.refraction_texture, 1);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, state->water_frame_buffers.reflection_texture);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, state->water_frame_buffers.refraction_texture);

	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}

void app_update_and_render(real32 dt, app_input *input, app_memory *memory, app_window_info *window_info)
{
	app_state *state = (app_state*)memory->permenant_storage;
	if (!memory->is_initialized) {
		memory->free_offset = sizeof(app_state);

		init_rng();

		perlin_params.scale = state->chunk_width;
		perlin_params.lacunarity = 2.f;
		perlin_params.persistence = 0.5f;
		perlin_params.elevation_power = 3.f;
		perlin_params.terrace_levels = 50;
		perlin_params.y_scale = 0.1f;
		perlin_params.max_octaves = 8;
		perlin_params.water_height = 0.f;
		perlin_params.sand_height = 10.f;
		perlin_params.snow_height = 100.f;
		perlin_params.ambient_strength = 0.4f;
		perlin_params.diffuse_strength = 1.0f;
		perlin_params.light_colour = { 1.0f, 0.8f, 0.7f };
		perlin_params.grass_colour = { 0.18f, 0.26f, 0.13f };
		perlin_params.sand_colour = { 0.8f, 0.81f, 0.55f };
		perlin_params.snow_colour = { 0.8f, 0.8f, 0.8f };
		perlin_params.slope_colour = { 0.7f, 0.67f, 0.56f };
		perlin_params.water_colour = { 0.f, 0.f, 0.05f };

		app_init_shaders(state);
		app_generate_terrain_chunk(state, memory, 0, 0);
		state->light_pos = {state->chunk_width / 2, 300.f, state->chunk_height / 2};
		
		init_water_data(state);

		camera_init(&state->cur_cam);
		state->cur_cam.pos = { -64.7642822f, 200.784561f, -48.5622902f};
		state->cur_cam.front = { 0.601920426f, -0.556864262f, 0.572358370f};
		state->cur_cam.yaw = 43.5579033f;
		state->cur_cam.pitch = -33.8392143;

		glViewport(0, 0, window_info->w, window_info->h);
		camera_frustrum(&state->cur_cam, window_info->w, window_info->h);

		state->wireframe = false;
		state->terrace = false;

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

	if (keyboard->wireframe.toggled) {
		state->wireframe = !state->wireframe;
	}

	if (keyboard->reset.toggled) {
		init_rng();
		app_generate_terrain_chunk(state, memory, 0, 0);
	}

	if (keyboard->gen_terrace.toggled) {
		state->terrace = !state->terrace;
		app_generate_terrain_chunk(state, memory, 0, 0);
	}

	camera_update(&state->cur_cam);
    camera_look_at(&state->cur_cam);

	glEnable(GL_CLIP_DISTANCE0);

	glEnable(GL_CULL_FACE);
	glCullFace(GL_FRONT);

    glBindTexture(GL_TEXTURE_2D, 0);
	glBindFramebuffer(GL_FRAMEBUFFER, state->water_frame_buffers.reflection_fbo);
	glViewport(0, 0, state->water_frame_buffers.REFLECTION_WIDTH, state->water_frame_buffers.REFLECTION_HEIGHT);
	real32 reflection_clip[4] = { 0, 1, 0, -perlin_params.water_height };
	real32 distance = 2.f * (state->cur_cam.pos.y - perlin_params.water_height);
	state->cur_cam.pos.y -= distance;
	state->cur_cam.pitch *= -1;

	camera_update(&state->cur_cam);
    camera_look_at(&state->cur_cam);
	
	glClearColor(0.6f, 0.6f, 0.8f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	app_render_terrain(state, reflection_clip);
	app_render_lights(state);

	state->cur_cam.pos.y += distance;
	state->cur_cam.pitch *= -1;

	camera_update(&state->cur_cam);
    camera_look_at(&state->cur_cam);

	glBindTexture(GL_TEXTURE_2D, 0);
	glBindFramebuffer(GL_FRAMEBUFFER, state->water_frame_buffers.refraction_fbo);
	glViewport(0, 0, state->water_frame_buffers.REFRACTION_WIDTH, state->water_frame_buffers.REFRACTION_HEIGHT);
	real32 refraction_clip[4] = { 0, -1, 0, perlin_params.water_height };

	glClearColor(0.6f, 0.6f, 0.8f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	app_render_terrain(state, refraction_clip);
	app_render_lights(state);
	
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glViewport(0, 0, window_info->w, window_info->h);

	glDisable(GL_CLIP_DISTANCE0);
	real32 no_clip[4] = { 0, -1, 0, 100000 };

	glClearColor(0.6f, 0.6f, 0.8f, 1.f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    app_render_terrain(state, no_clip);
	app_render_lights(state);
	app_render_water(state);

	ImGui::NewFrame();
	
    IM_ASSERT(ImGui::GetCurrentContext() != NULL && "Missing dear imgui context. Refer to examples app!");
	ImGuiWindowFlags window_flags = 0;
    window_flags |= ImGuiWindowFlags_NoTitleBar;
	window_flags |= ImGuiWindowFlags_NoScrollbar;
	window_flags |= ImGuiWindowFlags_MenuBar;
	ImGui::SetNextWindowPos(ImVec2(650, 20), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(550, 680), ImGuiCond_FirstUseEver);

	bool p_open = true;
	 if (!ImGui::Begin("Dear ImGui Demo", &p_open, window_flags))
    {
        // Early out if the window is collapsed, as an optimization.
        ImGui::End();
        return;
    }

	//ImGui::Image((void*)(intptr_t)state->water_frame_buffers.refraction_texture, ImVec2(800, 800));

    ImGui::Text("perlin noise parameters");

	bool changed = false;
	changed |= ImGui::SliderFloat("scale", &perlin_params.scale, 1.0f, 1000.f, "scale");
    changed |= ImGui::SliderFloat("y scale", &perlin_params.y_scale, 0.0f, 1.0f, "y scale");
    changed |= ImGui::SliderFloat("lacunarity", &perlin_params.lacunarity, 0.0f, 5.0f, "lacunarity");
    changed |= ImGui::SliderFloat("persistence", &perlin_params.persistence, 0.0f, 2.0f, "persistence");
    changed |= ImGui::SliderFloat("elevation", &perlin_params.elevation_power, 0.0f, 5.0f, "elevation");
    changed |= ImGui::SliderInt("octaves", &perlin_params.max_octaves, 1, 16, "octaves");
    changed |= ImGui::SliderInt("terraces", &perlin_params.terrace_levels, 1, 100, "terraces");
	changed |= ImGui::Checkbox("tectonic", &perlin_params.tectonic);

	ImGui::SliderFloat("ambient strength", &perlin_params.ambient_strength, 0.0f, 1.0f, "ambient strength");
	ImGui::SliderFloat("diffuse strength", &perlin_params.diffuse_strength, 0.0f, 1.0f, "diffuse strength");

	ImGui::SliderFloat("grass colour red", &perlin_params.grass_colour.E[0], 0.0f, 1.0f, "grass colour r");
    ImGui::SliderFloat("grass colour green", &perlin_params.grass_colour.E[1], 0.0f, 1.0f, "grass colour g");
    ImGui::SliderFloat("grass colour blue", &perlin_params.grass_colour.E[2], 0.0f, 1.0f, "grass colour b");

	ImGui::SliderFloat("slope colour red", &perlin_params.slope_colour.E[0], 0.0f, 1.0f, "slope colour r");
    ImGui::SliderFloat("slope colour green", &perlin_params.slope_colour.E[1], 0.0f, 1.0f, "slope colour g");
    ImGui::SliderFloat("slope colour blue", &perlin_params.slope_colour.E[2], 0.0f, 1.0f, "slope colour b");

    ImGui::SliderFloat("water height", &perlin_params.water_height, -50.0f, 50.0f, "water height");
	ImGui::SliderFloat("water colour red", &perlin_params.water_colour.E[0], 0.0f, 1.0f, "water colour r");
    ImGui::SliderFloat("water colour green", &perlin_params.water_colour.E[1], 0.0f, 1.0f, "water colour g");
    ImGui::SliderFloat("water colour blue", &perlin_params.water_colour.E[2], 0.0f, 1.0f, "water colour b");
    ImGui::SliderFloat("sand height", &perlin_params.sand_height, 0.0f, 100.0f, "sand height");
    ImGui::SliderFloat("sand colour red", &perlin_params.sand_colour.E[0], 0.0f, 1.0f, "sand colour r");
    ImGui::SliderFloat("sand colour green", &perlin_params.sand_colour.E[1], 0.0f, 1.0f, "sand colour g");
    ImGui::SliderFloat("sand colour blue", &perlin_params.sand_colour.E[2], 0.0f, 1.0f, "sand colour b");
    ImGui::SliderFloat("snow height", &perlin_params.snow_height, 0.0f, 200.0f, "snow height");
    ImGui::SliderFloat("snow colour red", &perlin_params.snow_colour.E[0], 0.0f, 1.0f, "snow colour r");
    ImGui::SliderFloat("snow colour green", &perlin_params.snow_colour.E[1], 0.0f, 1.0f, "snow colour g");
    ImGui::SliderFloat("snow colour blue", &perlin_params.snow_colour.E[2], 0.0f, 1.0f, "snow colour b");

	ImGui::End();

	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData()); 

	if (changed) {
		app_generate_terrain_chunk(state, memory, 0, 0);
	}
}