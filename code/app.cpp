#include "app.h"

#include <assert.h>
#include <stdlib.h>
#include <string>
#include <fstream>
#include <functional>
#include <filesystem>

#include "maths.h"
#include "win32-opengl.h"
#include "opengl-util.h"
#include "perlin.h"
#include "shaders.h"

#include "imgui-master/imgui.h"
#include "imgui-master/imgui_impl_opengl3.h"
#include "imgui-master/imgui_stdlib.h"

void init_terrain(app_state *state, u32 chunk_tile_length, u32 world_width);

static const u32 MAX_RESOLUTION = 8192;
static const char *texture_resolutions[6] = { "256", "512", "1024", "2048", "4096", "8192" };

void *my_malloc(app_memory *memory, u64 size)
{
#ifdef _DEBUG
	assert(memory->free_offset + size < memory->permenant_storage_size);
#endif
	void *p = (void *)((char *)(memory->permenant_storage) + memory->free_offset);
	memory->free_offset += size;
	return p;
}

void init_lod_detail_levels(LODSettings *lod_settings, u32 chunk_tile_length)
{
	u32 detail = 1;
	u32 multiplier = pow(2, lod_settings->detail_multiplier);

	for (u32 i = 0; i < lod_settings->max_details_count; i++) {
		lod_settings->details[i] = detail;
		detail *= multiplier;
	}

	// Find the most LODs possible for the chunk size.
	for (u32 i = 0; i < lod_settings->max_details_count; i++) {
		lod_settings->max_available_count = i;

		if (lod_settings->details[i] >= chunk_tile_length) {
			break;
		}
	}

	lod_settings->details_in_use = 1;
}

static real32 noise(V2 p)
{
	return perlin(p);
}

static float pattern(V2 p)
{
	V2 q;
	V2 a = { 0.f, 0.f };
	V2 b = { 5.2f, 1.3f };
	q.x = noise(p + 4.f * a);
	q.y = noise(p + 4.f * b);
	return noise(p + q);
}

static void app_generate_terrain_chunk(
	app_state *state
	, Chunk *chunk
	, bool32 just_lods)
{
	if (!just_lods) {
		for (u32 j = 0; j < state->chunk_vertices_length; j++) {
			for (u32 i = 0; i < state->chunk_vertices_length; i++) {
				u32 index = j * state->chunk_vertices_length + i;

				real32 x = state->cur_preset.params.x_offset + (chunk->x + (real32)i / state->cur_preset.params.chunk_tile_length) / state->cur_preset.params.scale;
				real32 y = state->cur_preset.params.z_offset + (chunk->y + (real32)j / state->cur_preset.params.chunk_tile_length) / state->cur_preset.params.scale;

				real32 total = 0;
				real32 frequency = 1;
				real32 amplitude = 1;
				real32 total_amplitude = 0;

				for (u32 octave = 0; octave < state->cur_preset.params.max_octaves; octave++) {
					total += (0.5f + noise({ (real32)(frequency * x), (real32)(frequency * y) })) * amplitude;
					total_amplitude += amplitude;
					amplitude *= state->cur_preset.params.persistence;
					frequency *= state->cur_preset.params.lacunarity;
				}

				real32 octave_result = total / total_amplitude;

				if (octave_result < 0) {
					octave_result = 0;
				}

				real32 elevation = ((powf(octave_result, state->cur_preset.params.elevation_power)) * state->cur_preset.params.y_scale * state->cur_preset.params.scale);

				chunk->vertices[index].pos.x = i;
				chunk->vertices[index].pos.y = elevation;
				chunk->vertices[index].pos.z = j;
				chunk->vertices[index].nor = {};
			}
		}

		// Calculate normals
		for (u32 j = 0; j < state->cur_preset.params.chunk_tile_length; j++) {
			for (u32 i = 0; i < state->cur_preset.params.chunk_tile_length; i++) {
				u32 index = j * state->cur_preset.params.chunk_tile_length + i;

				u32 v0 = j * state->chunk_vertices_length + i;
				u32 v1 = v0 + 1;
				u32 v2 = v0 + (1 * state->chunk_vertices_length);
				u32 v3 = v2 + 1;

				Vertex *a = &chunk->vertices[v3];
				Vertex *b = &chunk->vertices[v1];
				Vertex *c = &chunk->vertices[v0];
				Vertex *d = &chunk->vertices[v2];

				V3 cp = v3_cross(b->pos - a->pos, c->pos - a->pos);
				
				a->nor += cp;
				b->nor += cp;
				c->nor += cp;

				cp = v3_cross(a->pos - d->pos, c->pos - d->pos);

				d->nor += cp;
				a->nor += cp;
				c->nor += cp;
			}
		}

		// // Average sum of normals for each vertex.
		for (u32 j = 0; j < state->chunk_vertices_length; j++) {
			for (u32 i = 0; i < state->chunk_vertices_length; i++) {
				u32 index = j * (state->chunk_vertices_length) + i;
				chunk->vertices[index].nor = v3_normalise(chunk->vertices[index].nor);
			}
		}
	}

	// Create lods.
	u64 lod_offset = 0;

	for (u32 lod_detail_index = 0; lod_detail_index < state->lod_settings.max_available_count; lod_detail_index++) {
		const u32 detail = state->lod_settings.details[lod_detail_index];

		u32 indices_length = state->chunk_vertices_length - detail;

		chunk->lod_data_infos[lod_detail_index].quads_count = 0;

		u32 v0, v1, v2, v3;

		const u32 max_width = state->cur_preset.params.chunk_tile_length;
		const u32 max_height = state->cur_preset.params.chunk_tile_length;

		// Create mesh for vertices above water
		for (u32 j = 0; j < indices_length; j += detail) {
			for (u32 i = 0; i < indices_length; i += detail) {
				u32 index = j * state->chunk_vertices_length + i;

				v1 = v2 = v3 = 0;
				v0 = index;

				v1 = v0 + detail;
				v2 = v0 + (detail * state->chunk_vertices_length);
				v3 = v2 + detail;

				chunk->lods[lod_offset + chunk->lod_data_infos[lod_detail_index].quads_count].i[0] = v3; // Top-right
				chunk->lods[lod_offset + chunk->lod_data_infos[lod_detail_index].quads_count].i[1] = v1; // Bottom-right
				chunk->lods[lod_offset + chunk->lod_data_infos[lod_detail_index].quads_count].i[2] = v0; // Bottom-left
				chunk->lods[lod_offset + chunk->lod_data_infos[lod_detail_index].quads_count].i[3] = v2; // Top-left
				chunk->lods[lod_offset + chunk->lod_data_infos[lod_detail_index].quads_count].i[4] = v3;
				chunk->lods[lod_offset + chunk->lod_data_infos[lod_detail_index].quads_count].i[5] = v0;

				chunk->lod_data_infos[lod_detail_index].quads_count++;
			}
		}

		chunk->lod_data_infos[lod_detail_index].quads = &chunk->lods[lod_offset];
		chunk->lod_data_infos[lod_detail_index].data_offset = lod_offset;

		lod_offset += chunk->lod_data_infos[lod_detail_index].quads_count;
	}

	chunk->lod_indices_count = lod_offset * 6;
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

	glDeleteShader(vertex_shader);
	glDeleteShader(fragment_shader);

	return program_id;
}

static u32 create_framebuffer_texture(u32 width, u32 height)
{
	u32 tex = 0;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, tex, 0);
	return tex;
}

static void init_water_data(app_state *state)
{
	WaterFrameBuffers wfb;

	glGenFramebuffers(1, &wfb.reflection_fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, wfb.reflection_fbo);
	glDrawBuffer(GL_COLOR_ATTACHMENT0);

	wfb.reflection_texture = create_framebuffer_texture(wfb.REFLECTION_WIDTH, wfb.REFLECTION_HEIGHT);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glGenFramebuffers(1, &wfb.refraction_fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, wfb.refraction_fbo);
	glDrawBuffer(GL_COLOR_ATTACHMENT0);

	wfb.refraction_texture = create_framebuffer_texture(wfb.REFRACTION_WIDTH, wfb.REFRACTION_HEIGHT);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	state->water_frame_buffers = wfb;
}

static void init_depth_map(app_state *state)
{
	glGenFramebuffers(1, &state->depth_map_fbo);

	glGenTextures(1, &state->depth_map);
	glBindTexture(GL_TEXTURE_2D, state->depth_map);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, 4096, 4096, 0, GL_DEPTH_COMPONENT, GL_FLOAT, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

	// Fixes artifacts on shadow edges.
	real32 border_color[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border_color);
	
	glBindFramebuffer(GL_FRAMEBUFFER, state->depth_map_fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, state->depth_map, 0);
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

static void create_terrain_texture_map_texture(TextureMapData *texture_map_data)
{
	if (texture_map_data->texture) {
		glDeleteTextures(1, &texture_map_data->texture);
		glDeleteFramebuffers(1, &texture_map_data->fbo);
	}

	glGenFramebuffers(1, &texture_map_data->fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, texture_map_data->fbo);
	glDrawBuffer(GL_COLOR_ATTACHMENT0);

	texture_map_data->texture = create_framebuffer_texture(texture_map_data->resolution, texture_map_data->resolution);
}

static void init_terrain_texture_maps(app_state *state)
{
	u32 pixels_size = MAX_RESOLUTION * MAX_RESOLUTION * sizeof(RGB);
	state->texture_map_data.pixels = (RGB *)malloc(pixels_size);

	glGenFramebuffers(1, &state->texture_map_data.fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, state->texture_map_data.fbo);
	glDrawBuffer(GL_COLOR_ATTACHMENT0);

	state->texture_map_data.resolution = 512;
	create_terrain_texture_map_texture(&state->texture_map_data);
}

static void app_on_destroy(app_state *state)
{
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	glDeleteTextures(1, &state->water_frame_buffers.reflection_texture);
	glDeleteTextures(1, &state->water_frame_buffers.refraction_texture);
	glDeleteFramebuffers(1, &state->water_frame_buffers.reflection_fbo);
	glDeleteFramebuffers(1, &state->water_frame_buffers.refraction_fbo);

	glDeleteTextures(1, &state->texture_map_data.texture);
	glDeleteFramebuffers(1, &state->texture_map_data.fbo);

	for (u32 i = 0; i < state->chunk_count; i++) {
		glDeleteBuffers(1, &state->chunks[i]->ebo);
		glDeleteBuffers(1, &state->chunks[i]->vbo);
	}

	glDeleteVertexArrays(1, &state->triangle_vao);

	glDeleteBuffers(1, &state->quad_vbo);
	glDeleteBuffers(1, &state->quad_ebo);
	glDeleteVertexArrays(1, &state->triangle_vao);

	glDeleteProgram(state->terrain_shader.program);
	glDeleteProgram(state->simple_shader.program);
	glDeleteProgram(state->water_shader.program);
}

static void terrain_shader_use(app_state *state, real32 *clip)
{
	glUseProgram(state->terrain_shader.program);

	glUniformMatrix4fv(state->terrain_shader.projection, 1, GL_FALSE, state->cur_cam.frustrum);
	glUniformMatrix4fv(state->terrain_shader.view, 1, GL_FALSE, state->cur_cam.view);

	glUniform4fv(state->terrain_shader.plane, 1, clip);

	glUniform1f(state->terrain_shader.ambient_strength, state->cur_preset.params.ambient_strength);
	glUniform1f(state->terrain_shader.diffuse_strength, state->cur_preset.params.diffuse_strength);
	glUniform1f(state->terrain_shader.specular_strength, state->cur_preset.params.specular_strength);
	glUniform1f(state->terrain_shader.gamma_correction, state->cur_preset.params.gamma_correction);

	glUniform3fv(state->terrain_shader.light_pos, 1, (GLfloat *)(&state->light_pos));
	glUniform1f(state->terrain_shader.sand_height, state->cur_preset.params.sand_height);
	glUniform1f(state->terrain_shader.stone_height, state->cur_preset.params.stone_height);
	glUniform1f(state->terrain_shader.snow_height, state->cur_preset.params.snow_height);

	glUniform3fv(state->terrain_shader.light_colour, 1, (GLfloat *)&state->cur_preset.params.light_colour);
	glUniform3fv(state->terrain_shader.slope_colour, 1, (GLfloat *)&state->cur_preset.params.slope_colour);
	glUniform3fv(state->terrain_shader.ground_colour, 1, (GLfloat *)&state->cur_preset.params.ground_colour);
	glUniform3fv(state->terrain_shader.sand_colour, 1, (GLfloat *)&state->cur_preset.params.sand_colour);
	glUniform3fv(state->terrain_shader.stone_colour, 1, (GLfloat *)&state->cur_preset.params.stone_colour);
	glUniform3fv(state->terrain_shader.snow_colour, 1, (GLfloat *)&state->cur_preset.params.snow_colour);

	glUniform3fv(state->terrain_shader.view_position, 1, (GLfloat *)&state->cur_cam.pos);

	glUniform1i(state->terrain_shader.shadow_map, 0);
}

static void app_render_chunk(app_state *state, real32 *clip, Chunk *chunk, u32 model_handle)
{
	glBindVertexArray(state->triangle_vao);

	u32 chunk_index = chunk->y * state->cur_preset.params.world_width + chunk->x;

	glBindBuffer(GL_ARRAY_BUFFER, state->chunks[chunk_index]->vbo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, state->chunks[chunk_index]->ebo);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof Vertex, (void *)0);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof Vertex, (void *)(3 * sizeof(real32)));

	real32 model[16];
	mat4_identity(model);
	mat4_translate(model, chunk->x * state->cur_preset.params.chunk_tile_length , 0, chunk->y * state->cur_preset.params.chunk_tile_length);

	glUniformMatrix4fv(model_handle, 1, GL_FALSE, model);

	// LOD 0 is the highest quality.
	u32 chunk_lod_detail = 0;
	s32 distance = 0;

	// Calculate distance from camera's chunk and use that to index the 
	// the LOD level data.
	if (chunk != state->current_chunk) {
		const u32 dx = chunk->x - state->current_chunk->x;
		const u32 dy = chunk->y - state->current_chunk->y;
		distance = sqrtf(dx * dx + dy * dy);

		// Cap the distance to the highest (lowest detail) LOD.
		if (distance >= state->lod_settings.details_in_use) {
			distance = state->lod_settings.details_in_use - 1;
		}

		chunk_lod_detail = distance;
	}

	const u32 lod_indices_area = chunk->lod_data_infos[chunk_lod_detail].quads_count * 6;
	const u64 chunk_offset_in_bytes = chunk->lod_data_infos[chunk_lod_detail].data_offset * sizeof(QuadIndices);

	// Find the offset of the LOD data we want to use be looping through every LOD level
	// before the one we want and calculating the sum of the total size.
	glDrawElements(GL_TRIANGLES, lod_indices_area, GL_UNSIGNED_INT, (void *)(chunk_offset_in_bytes));
}

static void simple_shader_use(app_state *state)
{
	glUseProgram(state->simple_shader.program);

	glUniform1f(state->simple_shader.ambient_strength, state->cur_preset.params.ambient_strength);
	glUniform1f(state->simple_shader.diffuse_strength, state->cur_preset.params.diffuse_strength);
	glUniform1f(state->simple_shader.gamma_correction, state->cur_preset.params.gamma_correction);

	glUniform3fv(state->simple_shader.light_pos, 1, (GLfloat *)(&state->light_pos));
	glUniform3fv(state->simple_shader.light_colour, 1, (GLfloat *)&state->cur_preset.params.light_colour);

	glUniformMatrix4fv(state->simple_shader.projection, 1, GL_FALSE, state->cur_cam.frustrum);
	glUniformMatrix4fv(state->simple_shader.view, 1, GL_FALSE, state->cur_cam.view);

	glUniform1i(state->simple_shader.shadow_map, 0);
}

static void app_render_trunks(app_state *state, u32 model_handle)
{
	real32 model[16];

	glBindVertexArray(state->triangle_vao);

	glBindBuffer(GL_ARRAY_BUFFER, state->trunk->vbos[0]);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, state->trunk->vbos[1]);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof Vertex, (void *)0);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof Vertex, (void *)(3 * sizeof(real32)));

	for (u32 i = 0; i < state->trees_pos.size(); i++) {
		if (state->trees_pos[i].y < state->cur_preset.params.tree_min_height || state->trees_pos[i].y > state->cur_preset.params.tree_max_height) {
			continue;
		}

		const real32 scale = state->cur_preset.params.tree_size * state->cur_preset.params.scale;

		mat4_identity(model);
		mat4_translate(model, state->trees_pos[i].x, state->trees_pos[i].y, state->trees_pos[i].z);
		mat4_rotate_x(model, state->trees_rotation[i].x);
		mat4_rotate_y(model, state->trees_rotation[i].y);
		mat4_rotate_z(model, state->trees_rotation[i].z);
		mat4_scale(model, scale, scale, scale);
		glUniformMatrix4fv(model_handle, 1, GL_FALSE, model);
		glDrawElements(GL_TRIANGLES, 3 * state->trunk->polygons.size(), GL_UNSIGNED_INT, 0);
	}
}

static void app_render_leaves(app_state *state, u32 model_handle)
{
	real32 model[16];

	glDisable(GL_CULL_FACE);

	glBindBuffer(GL_ARRAY_BUFFER, state->leaves->vbos[0]);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, state->leaves->vbos[1]);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof Vertex, (void *)0);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof Vertex, (void *)(3 * sizeof(real32)));

	for (u32 i = 0; i < state->trees_pos.size(); i++) {
		if (state->trees_pos[i].y < state->cur_preset.params.tree_min_height || state->trees_pos[i].y > state->cur_preset.params.tree_max_height) {
			continue;
		}

		const real32 scale = state->cur_preset.params.tree_size * state->cur_preset.params.scale;

		mat4_identity(model);
		mat4_translate(model, state->trees_pos[i].x, state->trees_pos[i].y, state->trees_pos[i].z);
		mat4_rotate_x(model, state->trees_rotation[i].x);
		mat4_rotate_y(model, state->trees_rotation[i].y);
		mat4_rotate_z(model, state->trees_rotation[i].z);
		mat4_scale(model, scale, scale, scale);
		glUniformMatrix4fv(model_handle, 1, GL_FALSE, model);
		glDrawElements(GL_TRIANGLES, 3 * state->leaves->polygons.size(), GL_UNSIGNED_INT, 0);
	}
}

static void app_render_rocks(app_state *state, u32 model_handle)
{
	real32 model[16];

	glBindVertexArray(state->triangle_vao);
	glBindBuffer(GL_ARRAY_BUFFER, state->rock->vbos[0]);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, state->rock->vbos[1]);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof Vertex, (void *)0);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof Vertex, (void *)(3 * sizeof(real32)));
	
	glEnable(GL_CULL_FACE);

	// Render rocks.
	for (u32 i = 0; i < state->rocks_pos.size(); i++) {
		if (state->rocks_pos[i].y < state->cur_preset.params.rock_min_height || state->rocks_pos[i].y > state->cur_preset.params.rock_max_height) {
			continue;
		}

		const real32 scale = state->cur_preset.params.rock_size * state->cur_preset.params.scale;

		mat4_identity(model);
		mat4_translate(model, state->rocks_pos[i].x, state->rocks_pos[i].y, state->rocks_pos[i].z);
		mat4_rotate_x(model, state->rocks_rotation[i].x);
		mat4_rotate_y(model, state->rocks_rotation[i].y);
		mat4_rotate_z(model, state->rocks_rotation[i].z);
		mat4_scale(model, scale, scale, scale);

		glUniformMatrix4fv(model_handle, 1, GL_FALSE, model);
		glDrawElements(GL_TRIANGLES, 3 * state->rock->polygons.size(), GL_UNSIGNED_INT, 0);
	}
	// End of features
}


static void generate_trees(app_state *state)
{
	// Hardcoded limit
	if (state->cur_preset.params.tree_count > 10000) {
		state->cur_preset.params.tree_count = 10000;
	}

	state->trees_pos.clear();
	state->trees_rotation.clear();

	for (u32 i = 0; i < state->cur_preset.params.tree_count; i++) {
		real32 x, y, z;
		x = y = z = -1;

		std::uniform_real_distribution<> rotation_distr(0, 360);

		u32 attempt = 0;
		while (y < state->cur_preset.params.tree_min_height || y > state->cur_preset.params.tree_max_height) {
			if (++attempt >= 50) {
				break;
			}

			std::uniform_int_distribution<> chunk_index(0, state->world_area - 1);

			Chunk *chunk = state->chunks[chunk_index(state->rng)];

			std::uniform_int_distribution<> vertex_index(0, chunk->vertices_count - 1);

			Vertex *v = &chunk->vertices[vertex_index(state->rng)];
			x = chunk->x * state->cur_preset.params.chunk_tile_length + v->pos.x;
			y = v->pos.y;
			z = chunk->y * state->cur_preset.params.chunk_tile_length + v->pos.z;
		}

		if (attempt < 50) {
			state->trees_pos.push_back({ x, y, z });
			state->trees_rotation.push_back({ 0.f, (real32)rotation_distr(state->rng), 0.f });
		}
	}
}

static void generate_rocks(app_state *state)
{
	// Hardcoded limit.
	if (state->cur_preset.params.rock_count > 10000) {
		state->cur_preset.params.rock_count = 10000;
	}

	state->rocks_pos.clear();
	state->rocks_rotation.clear();

	std::uniform_real_distribution<> rotation_distr(0, 360);

	for (u32 i = 0; i < state->cur_preset.params.rock_count; i++) {
		real32 x, y, z;
		x = y = z = -1;
		Vertex *v = &state->chunks[0]->vertices[0];

		u32 attempt = 0;
		while (y < state->cur_preset.params.rock_min_height || y > state->cur_preset.params.rock_max_height) {
			if (++attempt >= 50) {
				break;
			}

			std::uniform_int_distribution<> chunk_index(0, state->world_area - 1);

			Chunk *chunk = state->chunks[chunk_index(state->rng)];

			std::uniform_int_distribution<> vertex_index(0, chunk->vertices_count - 1);

			v = &chunk->vertices[vertex_index(state->rng)];
			x = chunk->x * state->cur_preset.params.chunk_tile_length + v->pos.x;
			y = v->pos.y;
			z = chunk->y * state->cur_preset.params.chunk_tile_length + v->pos.z;
		}

		if (attempt < 50) {
			state->rocks_pos.push_back({ x, y, z });

			state->rocks_rotation.push_back({
				(acosf(v3_dot(v->nor, { 1, 0, 0 })) * 180.f / (real32)M_PI),
				(real32)rotation_distr(state->rng),
				(acosf(v3_dot(v->nor, { 0, 0, 1 })) * 180.f / (real32)M_PI)
			});
		}
	}
}

static void generate_world(app_state *state, bool32 just_lods = false)
{
	for (u32 j = 0; j < state->cur_preset.params.world_width; j++) {
		for (u32 i = 0; i < state->cur_preset.params.world_width; i++) {
			state->generation_threads.push_back(std::thread(app_generate_terrain_chunk, state, state->chunks[j * state->cur_preset.params.world_width + i], just_lods));
		}
	}

	for (u32 i = 0; i < state->world_area; i++) {
		state->generation_threads[i].join();
	}

	state->generation_threads.clear();

	glBindVertexArray(state->triangle_vao);

	u64 quads_sum = 0;
	for (u32 j = 0; j < state->cur_preset.params.world_width; j++) {
		for (u32 i = 0; i < state->cur_preset.params.world_width; i++) {
			u32 index = j * state->cur_preset.params.world_width + i;

			Chunk *chunk = state->chunks[index];

			glBindBuffer(GL_ARRAY_BUFFER, chunk->vbo);
			glBufferData(GL_ARRAY_BUFFER, chunk->vertices_count * sizeof Vertex, chunk->vertices.data(), GL_STATIC_DRAW);
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof Vertex, (void *)0);
			glEnableVertexAttribArray(0);

			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, chunk->ebo);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, chunk->lod_indices_count * sizeof(u32), chunk->lods.data(), GL_STATIC_DRAW);
			glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof Vertex, (void *)(3 * sizeof(real32)));
			glEnableVertexAttribArray(1);

			quads_sum += chunk->lod_data_infos[0].quads_count;
		}
	}

	generate_trees(state);
	generate_rocks(state);
}

static std::string face_string_with_normals(u32 f0, u32 f1, u32 f2)
{
	std::stringstream ss;
	ss << "f " << f0 << "//" << f0 << " " << f1 << "//" << f1 << " " << f2 << "//" << f2 << std::endl;
	return ss.str();
}

static std::string face_string_with_uv(u32 f0, u32 f1, u32 f2)
{
	std::stringstream ss;
	ss << "f " << f0 << "/" << f0 << " " << f1 << "/" << f1 << " " << f2 << "/" << f2 << std::endl;
	return ss.str();
}

static std::string face_string_with_normals_and_uv(u32 f0, u32 f1, u32 f2)
{
	std::stringstream ss;
	ss << "f " << f0 << "/" << f0 << "/" << f0 << " " << f1 << "/" << f1 << "/" << f1 << " " << f2 << "/" << f2 << "/" << f2 << std::endl;
	return ss.str();
}

static std::string face_string_without_normals(u32 f0, u32 f1, u32 f2)
{
	std::stringstream ss;
	ss << "f " << f0 << " " << f1 << " " << f2 << std::endl;
	return ss.str();
}

static void export_terrain_chunk(app_state *state, std::string path, Chunk *chunk)
{
	std::string filename = "chunk_" + std::to_string(chunk->y) + "_" + std::to_string(chunk->x) + ".obj";
	std::ofstream object_file(path + filename, std::ios::out);

	if (object_file.good()) {
		object_file << "mtllib terrain.mtl" << std::endl;
		object_file << "usemtl textured" << std::endl;
		object_file << "o " << filename << std::endl;

		for (u32 vertex = 0; vertex < chunk->vertices_count; vertex++) {
			Vertex *current_vertex = &chunk->vertices[vertex];
			// Offset chunk vertices by world position.
			real32 x = current_vertex->pos.x + chunk->x * (state->cur_preset.params.chunk_tile_length);
			real32 y = current_vertex->pos.y;
			real32 z = current_vertex->pos.z + chunk->y * (state->cur_preset.params.chunk_tile_length);

			object_file << "v " << x << " " << y << " " << z;
			object_file << std::endl;
		}

		if (state->export_settings.texture_map) {
			for (u32 vertex_row = 0; vertex_row < state->chunk_vertices_length; vertex_row++) {
				for (u32 vertex_col = 0; vertex_col < state->chunk_vertices_length; vertex_col++) {
					real32 u = (real32)(chunk->y * state->chunk_vertices_length + vertex_row) / (state->cur_preset.params.world_width * state->chunk_vertices_length);
					real32 v = (real32)(chunk->x * state->chunk_vertices_length + vertex_col) / (state->cur_preset.params.world_width * state->chunk_vertices_length);

					object_file << "vt " << u << " " << v << std::endl;
				}
			}
		}

		if (state->export_settings.with_normals) {
			for (u32 vertex = 0; vertex < chunk->vertices_count; vertex++) {
				Vertex *current_vertex = &chunk->vertices[vertex];
				real32 nx = current_vertex->nor.x;
				real32 ny = current_vertex->nor.y;
				real32 nz = current_vertex->nor.z;
				object_file << "vn " << nx << " " << ny << " " << nz << std::endl;
			}
		}

		std::function<std::string(u32, u32, u32)> face_string_func;
		face_string_func = face_string_without_normals;

		if (state->export_settings.with_normals) {
			face_string_func = face_string_with_normals;

			if (state->export_settings.texture_map) {
				face_string_func = face_string_with_normals_and_uv;
			}
		}
		else if (state->export_settings.texture_map) {
			face_string_func = face_string_with_uv;
		}

		u32 num_lods_to_export = 1;

		if (state->export_settings.lods) {
			num_lods_to_export = state->lod_settings.details_in_use;
		}

		// Each LOD has a group in that object.
		for (u32 lod_detail_index = 0; lod_detail_index < num_lods_to_export; lod_detail_index++) {
			object_file << "g " << filename << "_lod" << lod_detail_index << std::endl;

			const u32 lod_detail = state->lod_settings.details[lod_detail_index];

			u32 num_quads = chunk->lod_data_infos[lod_detail_index].quads_count;

			for (u32 index = 0; index <= num_quads; index++) {
				QuadIndices *current_quad = &chunk->lod_data_infos[lod_detail_index].quads[index];

				u32 f0 = current_quad->i[0] + 1;
				u32 f1 = current_quad->i[1] + 1;
				u32 f2 = current_quad->i[2] + 1;
				object_file << face_string_func(f0, f1, f2);

				u32 f3 = current_quad->i[3] + 1;
				u32 f4 = current_quad->i[4] + 1;
				u32 f5 = current_quad->i[5] + 1;
				object_file << face_string_func(f3, f4, f5);
			}
		}
	}
}

static void export_terrain_one_obj(app_state *state, std::string path)
{
	std::ofstream object_file(path + "terrain.obj", std::ios::out);

	if (object_file.good()) {
		object_file << "mtllib terrain.mtl" << std::endl;
		object_file << "usemtl textured" << std::endl;
		object_file << "o Terrain" << std::endl;

		for (u32 chunk_z = 0; chunk_z < state->cur_preset.params.world_width; chunk_z++) {
			for (u32 chunk_x = 0; chunk_x < state->cur_preset.params.world_width; chunk_x++) {
				u32 chunk_index = chunk_z * state->cur_preset.params.world_width + chunk_x;

				object_file << "# Chunk" << chunk_index << " vertices" << std::endl;

				for (u32 vertex = 0; vertex < state->chunks[chunk_index]->vertices_count; vertex++) {
					Vertex *current_vertex = &state->chunks[chunk_index]->vertices[vertex];
					// Offset chunk vertices by world position.
					real32 x = current_vertex->pos.x + chunk_x * (state->cur_preset.params.chunk_tile_length);
					real32 y = current_vertex->pos.y;
					real32 z = current_vertex->pos.z + chunk_z * (state->cur_preset.params.chunk_tile_length);

					object_file << "v " << x << " " << y << " " << z;
					object_file << std::endl;
				}
			}
		}

		if (state->export_settings.texture_map) {
			for (u32 chunk_z = 0; chunk_z < state->cur_preset.params.world_width; chunk_z++) {
				for (u32 chunk_x = 0; chunk_x < state->cur_preset.params.world_width; chunk_x++) {
					for (u32 vertex_row = 0; vertex_row < state->chunk_vertices_length; vertex_row++) {
						for (u32 vertex_col = 0; vertex_col < state->chunk_vertices_length; vertex_col++) {
							real32 u = (real32)(chunk_z * state->chunk_vertices_length + vertex_row) / (state->cur_preset.params.world_width * state->chunk_vertices_length);
							real32 v = (real32)(chunk_x * state->chunk_vertices_length + vertex_col) / (state->cur_preset.params.world_width * state->chunk_vertices_length);

							object_file << "vt " << u << " " << v << std::endl;
						}
					}
				}
			}
		}

		if (state->export_settings.with_normals) {
			for (u32 chunk_z = 0; chunk_z < state->cur_preset.params.world_width; chunk_z++) {
				for (u32 chunk_x = 0; chunk_x < state->cur_preset.params.world_width; chunk_x++) {
					u32 chunk_index = chunk_z * state->cur_preset.params.world_width + chunk_x;

					for (u32 vertex = 0; vertex < state->chunks[chunk_index]->vertices_count; vertex++) {
						Vertex *current_vertex = &state->chunks[chunk_index]->vertices[vertex];
						real32 nx = current_vertex->nor.x;
						real32 ny = current_vertex->nor.y;
						real32 nz = current_vertex->nor.z;
						object_file << "vn " << nx << " " << ny << " " << nz << std::endl;
					}
				}
			}
		}

		std::function<std::string(u32, u32, u32)> face_string_func;
		face_string_func = face_string_without_normals;

		if (state->export_settings.with_normals) {
			face_string_func = face_string_with_normals;

			if (state->export_settings.texture_map) {
				face_string_func = face_string_with_normals_and_uv;
			}
		}
		else if (state->export_settings.texture_map) {
			face_string_func = face_string_with_uv;
		}

		for (u32 chunk_z = 0; chunk_z < state->cur_preset.params.world_width; chunk_z++) {
			for (u32 chunk_x = 0; chunk_x < state->cur_preset.params.world_width; chunk_x++) {
				u32 chunk_index = chunk_z * state->cur_preset.params.world_width + chunk_x;

				Chunk *current_chunk = state->chunks[chunk_index];

				u32 num_lods_to_export = 1;

				if (state->export_settings.lods) {
					num_lods_to_export = state->lod_settings.details_in_use;
				}

				// Each LOD has a group in that object.
				for (u32 lod_detail_index = 0; lod_detail_index < num_lods_to_export; lod_detail_index++) {
					object_file << "g Chunk" << chunk_index << "LOD" << lod_detail_index << std::endl;

					const u32 lod_detail = state->lod_settings.details[lod_detail_index];

					u32 num_quads = current_chunk->lod_data_infos[lod_detail_index].quads_count;

					for (u32 index = 0; index <= num_quads; index++) {
						QuadIndices *current_quad = &current_chunk->lod_data_infos[lod_detail_index].quads[index];
						u64 chunk_vertices_number_offset = chunk_index * state->chunks[chunk_index]->vertices_count;

						u32 f0 = current_quad->i[0] + 1 + chunk_vertices_number_offset;
						u32 f1 = current_quad->i[1] + 1 + chunk_vertices_number_offset;
						u32 f2 = current_quad->i[2] + 1 + chunk_vertices_number_offset;
						object_file << face_string_func(f0, f1, f2);

						u32 f3 = current_quad->i[3] + 1 + chunk_vertices_number_offset;
						u32 f4 = current_quad->i[4] + 1 + chunk_vertices_number_offset;
						u32 f5 = current_quad->i[5] + 1 + chunk_vertices_number_offset;
						object_file << face_string_func(f3, f4, f5);
					}
				}
			}
		}
	}

	object_file.close();
}

static void export_terrain(app_state *state)
{
	auto t = std::time(nullptr);
	auto tm = *std::localtime(&t);
	std::ostringstream oss;
	oss << std::put_time(&tm, "%d-%m-%y-%H-%M-%S");

	const std::string path = "./export/" + state->cur_preset.name + "-" + oss.str() + "/";

	std::filesystem::create_directory("./export"); // If it somehow gets deleted.
	std::filesystem::create_directory(path);

	if (state->export_settings.seperate_chunks) {
		for (u32 j = 0; j < state->cur_preset.params.world_width; j++) {
			for (u32 i = 0; i < state->cur_preset.params.world_width; i++) {
				state->generation_threads.push_back(std::thread(export_terrain_chunk, state, path, state->chunks[j * state->cur_preset.params.world_width + i]));
			}
		}

		for (u32 i = 0; i < state->world_area; i++) {
			state->generation_threads[i].join();
		}

		state->generation_threads.clear();
	}
	else {
		export_terrain_one_obj(state, path);
	}

	// Export trees & rocks.
	if (state->export_settings.trees) {
		std::ofstream trunks_file(path + "tree_trunks.obj", std::ios::out);
		
		if (trunks_file.good()) {
			trunks_file << "mtllib tree_trunks.mtl" << std::endl;
			trunks_file << "usemtl colour" << std::endl;
			
			u32 i = 0;
			u32 vertex_offset = 0;
			for (auto &p : state->trees_pos) {
				trunks_file << "o Tree_" << i++ << std::endl;
				for (auto &v : state->trunk->vertices) {
					V4 v4 = { v->pos.x, v->pos.y, v->pos.z, 1.f };
					V4 d = {};
					real32 m[16];
					mat4_identity(m);

					real32 scale = state->cur_preset.params.tree_size * state->cur_preset.params.scale;
					mat4_scale(m, scale, scale, scale);
					mat4_rotate_y(m, state->trees_rotation[i].y);
					
					for (u32 i = 0; i < 4; i++) {
						for (u32 j = 0; j < 4; j++) {
							d.E[i] += (m[i * 4 + j] * v4.E[j]);
						}
					}

					const real32 x = d.x + p.x;
					const real32 y = d.y + p.y;
					const real32 z = d.z + p.z;
					trunks_file << "v " << x << " " << " " << y << " " << z << std::endl;
				}

				for (auto &v : state->trunk->vertices) {
					const real32 x = v->nor.x;
					const real32 y = v->nor.y;
					const real32 z = v->nor.z;
					trunks_file << "vn " << x << " " << " " << y << " " << z << std::endl;
				}

				for (auto f : state->trunk->polygons) {
					const u32 f0 = f->indices[0] + 1 + vertex_offset;
					const u32 f1 = f->indices[1] + 1 + vertex_offset;
					const u32 f2 = f->indices[2] + 1 + vertex_offset;
					trunks_file << face_string_with_normals_and_uv(f0, f1, f2);
				}

				vertex_offset += state->trunk->vertices.size();
			}

			std::ofstream trunk_material_file(path + "tree_trunks.mtl", std::ios::out);

			if (trunk_material_file.good()) {
				std::string colour_string = std::to_string(state->cur_preset.params.trunk_colour.x) + " "
					+ std::to_string(state->cur_preset.params.trunk_colour.y) + " "
					+ std::to_string(state->cur_preset.params.trunk_colour.z);

				trunk_material_file << "newmtl colour" << std::endl;
				trunk_material_file << "Ka " << colour_string << std::endl;
				trunk_material_file << "Kd " << colour_string << std::endl;
				trunk_material_file << "Ks 0.000 0.000 0.000" << std::endl;
				trunk_material_file << "d 1.000" << std::endl;
				trunk_material_file << "illum 2" << std::endl;
			}

			trunk_material_file.close();
		}

		trunks_file.close();

		std::ofstream leaves_file(path + "tree_leaves.obj", std::ios::out);

		if (leaves_file.good()) {
			leaves_file << "mtllib tree_leaves.mtl" << std::endl;
			leaves_file << "usemtl colour" << std::endl;

			u32 i = 0;
			u32 vertex_offset = 0;
			for (auto &p : state->trees_pos) {
				leaves_file << "o Leaves_" << i++ << std::endl;
				for (auto &v : state->leaves->vertices) {
					V4 v4 = { v->pos.x, v->pos.y, v->pos.z, 1.f };
					V4 d = {};
					real32 m[16];
					mat4_identity(m);

					real32 scale = state->cur_preset.params.tree_size * state->cur_preset.params.scale;
					mat4_scale(m, scale, scale, scale);
					mat4_rotate_y(m, state->trees_rotation[i].y);

					for (u32 i = 0; i < 4; i++) {
						for (u32 j = 0; j < 4; j++) {
							d.E[i] += (m[i * 4 + j] * v4.E[j]);
						}
					}

					const real32 x = d.x + p.x;
					const real32 y = d.y + p.y;
					const real32 z = d.z + p.z;
					leaves_file << "v " << x << " " << " " << y << " " << z << std::endl;
				}

				for (auto &v : state->leaves->vertices) {
					const real32 x = v->nor.x;
					const real32 y = v->nor.y;
					const real32 z = v->nor.z;
					leaves_file << "vn " << x << " " << " " << y << " " << z << std::endl;
				}

				for (auto f : state->leaves->polygons) {
					const u32 f0 = f->indices[0] + 1 + vertex_offset;
					const u32 f1 = f->indices[1] + 1 + vertex_offset;
					const u32 f2 = f->indices[2] + 1 + vertex_offset;
					leaves_file << face_string_with_normals_and_uv(f0, f1, f2);
				}

				vertex_offset += state->leaves->vertices.size();
			}

			std::ofstream leaves_material_file(path + "tree_leaves.mtl", std::ios::out);

			if (leaves_material_file.good()) {
				std::string colour_string = std::to_string(state->cur_preset.params.leaves_colour.x) + " "
					+ std::to_string(state->cur_preset.params.leaves_colour.y) + " "
					+ std::to_string(state->cur_preset.params.leaves_colour.z);

				leaves_material_file << "newmtl colour" << std::endl;
				leaves_material_file << "Ka " << colour_string << std::endl;
				leaves_material_file << "Kd " << colour_string << std::endl;
				leaves_material_file << "Ks 0.000 0.000 0.000" << std::endl;
				leaves_material_file << "d 1.000" << std::endl;
				leaves_material_file << "illum 2" << std::endl;
			}

			leaves_material_file.close();
		}

		leaves_file.close();
	}

	if (state->export_settings.rocks) {
		std::ofstream rocks_file(path + "rocks.obj", std::ios::out);

		if (rocks_file.good()) {
			rocks_file << "mtllib rocks.mtl" << std::endl;
			rocks_file << "usemtl colour" << std::endl;

			u32 i = 0;
			u32 vertex_offset = 0;
			for (auto &p : state->rocks_pos) {
				rocks_file << "o Tree_" << i++ << std::endl;
				for (auto &v : state->rock->vertices) {
					V4 v4 = { v->pos.x, v->pos.y, v->pos.z, 1.f };
					V4 d = {};
					real32 m[16];
					mat4_identity(m);

					real32 scale = state->cur_preset.params.rock_size * state->cur_preset.params.scale;
					mat4_scale(m, scale, scale, scale);
					mat4_rotate_z(m, state->rocks_rotation[i].z);
					mat4_rotate_y(m, state->rocks_rotation[i].y);
					mat4_rotate_x(m, state->rocks_rotation[i].x);
				
					for (u32 i = 0; i < 4; i++) {
						for (u32 j = 0; j < 4; j++) {
							d.E[i] += (m[i * 4 + j] * v4.E[j]);
						}
					}

					const real32 x = d.x + p.x;
					const real32 y = d.y + p.y;
					const real32 z = d.z + p.z;
					rocks_file << "v " << x << " " << " " << y << " " << z << std::endl;
				}

				for (auto &v : state->rock->vertices) {
					const real32 x = v->nor.x;
					const real32 y = v->nor.y;
					const real32 z = v->nor.z;
					rocks_file << "vn " << x << " " << " " << y << " " << z << std::endl;
				}

				for (auto f : state->rock->polygons) {
					const u32 f0 = f->indices[0] + 1 + vertex_offset;
					const u32 f1 = f->indices[1] + 1 + vertex_offset;
					const u32 f2 = f->indices[2] + 1 + vertex_offset;
					rocks_file << face_string_with_normals_and_uv(f0, f1, f2);
				}

				vertex_offset += state->rock->vertices.size();
			}

			std::ofstream rock_material_file(path + "rocks.mtl", std::ios::out);

			if (rock_material_file.good()) {
				std::string colour_string = std::to_string(state->cur_preset.params.rock_colour.x) + " "
					+ std::to_string(state->cur_preset.params.rock_colour.y) + " "
					+ std::to_string(state->cur_preset.params.rock_colour.z);

				rock_material_file << "newmtl colour" << std::endl;
				rock_material_file << "Ka " << colour_string << std::endl;
				rock_material_file << "Kd " << colour_string << std::endl;
				rock_material_file << "Ks 0.000 0.000 0.000" << std::endl;
				rock_material_file << "d 1.000" << std::endl;
				rock_material_file << "illum 2" << std::endl;
			}

			rock_material_file.close();
		}

		rocks_file.close();
	}

	// Material file.
	std::ofstream material_file(path + "terrain.mtl", std::ios::out);

	if (material_file.good()) {
		material_file << "newmtl textured" << std::endl;
		material_file << "Ka 1.000 1.000 1.000" << std::endl;
		material_file << "Kd 1.000 1.000 1.000" << std::endl;
		material_file << "Ks 1.000 1.000 1.000" << std::endl;
		material_file << "d 1.000" << std::endl;
		material_file << "illum 2" << std::endl;

		if (state->export_settings.texture_map) {
			material_file << "map_Ka diffuse.tga" << std::endl;
			material_file << "map_Kd diffuse.tga" << std::endl;
		}
	}

	material_file.close();

	// Texture map.
	if (state->export_settings.texture_map) {
		std::ofstream tga_file(path + "diffuse.tga", std::ios::binary);
		if (!tga_file) return;

		char header[18] = { 0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };
		header[12] = state->texture_map_data.resolution & 0xFF;
		header[13] = (state->texture_map_data.resolution >> 8) & 0xFF;
		header[14] = (state->texture_map_data.resolution) & 0xFF;
		header[15] = (state->texture_map_data.resolution >> 8) & 0xFF;
		header[16] = 24;

		tga_file.write((char *)header, 18);

		real32 no_clip[4] = { 0, -1, 0, 100000 };

		real32 light_projection[16], light_view[16];
		mat4_identity(light_projection);
		mat4_identity(light_view);
		mat4_ortho(light_projection, -1.f * state->world_tile_length, state->world_tile_length, -1.f * state->world_tile_length, state->world_tile_length, 1.f, 10000.f);
		mat4_look_at(light_view, state->light_pos, { 0.f, 0.f, 0.f }, { 0.f, 1.f, 0.f });

		real32 light_space_matrix[16];
		mat4_identity(light_space_matrix);
		mat4_multiply(light_space_matrix, light_projection, light_view);

		V3 light_pos = state->light_pos;

		glActiveTexture(GL_TEXTURE0);

		// Put the light directly above the terrain if we dont want shadows.
		if (!state->export_settings.bake_shadows) {
			light_pos = { ((real32)state->cur_preset.params.chunk_tile_length / 2) * state->cur_preset.params.world_width, 5000.f, ((real32)state->cur_preset.params.chunk_tile_length / 2) * state->cur_preset.params.world_width };
			glBindTexture(GL_TEXTURE_2D, 0);
		}
		else {
			// Render to frame buffer
			glViewport(0, 0, 4096, 4096);
			glBindFramebuffer(GL_FRAMEBUFFER, state->depth_map_fbo);
			glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

			// Render the shadow map from the lights POV.
			glUseProgram(state->depth_shader.program);
			glUniformMatrix4fv(state->depth_shader.projection, 1, GL_FALSE, light_projection);
			glUniformMatrix4fv(state->depth_shader.view, 1, GL_FALSE, light_view);

			glCullFace(GL_FRONT);

			for (u32 i = 0; i < state->chunk_count; i++) {
				app_render_chunk(state, no_clip, state->chunks[i], state->depth_shader.model);
			}

			glCullFace(GL_BACK);

			app_render_trunks(state, state->depth_shader.model);
			app_render_leaves(state, state->depth_shader.model);
			app_render_rocks(state, state->depth_shader.model);

			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			glViewport(0, 0, state->window_info.w, state->window_info.h);

			glBindTexture(GL_TEXTURE_2D, state->depth_map);
		}

		Camera copy_cam = state->cur_cam;
		camera_ortho(&copy_cam, state->world_tile_length, state->world_tile_length);
		copy_cam.pos = { 0, 9000, 0 };
		copy_cam.front = { 0, -1, 0 };
		copy_cam.up = { 1, 0, 0 };
		camera_look_at(&copy_cam); 

		glUseProgram(state->terrain_shader.program);

		glUniform4fv(state->terrain_shader.plane, 1, no_clip);

		glUniform1f(state->terrain_shader.ambient_strength, state->cur_preset.params.ambient_strength);
		glUniform1f(state->terrain_shader.diffuse_strength, state->cur_preset.params.diffuse_strength);
		glUniform1f(state->terrain_shader.specular_strength, state->cur_preset.params.specular_strength);
		glUniform1f(state->terrain_shader.diffuse_strength, state->cur_preset.params.gamma_correction);

		glUniform3fv(state->terrain_shader.light_pos, 1, (GLfloat *)(&light_pos));
		glUniform1f(state->terrain_shader.sand_height, state->cur_preset.params.sand_height);
		glUniform1f(state->terrain_shader.snow_height, state->cur_preset.params.snow_height);

		glUniform3fv(state->terrain_shader.light_colour, 1, (GLfloat *)&state->cur_preset.params.light_colour);
		glUniform3fv(state->terrain_shader.slope_colour, 1, (GLfloat *)&state->cur_preset.params.slope_colour);
		glUniform3fv(state->terrain_shader.ground_colour, 1, (GLfloat *)&state->cur_preset.params.ground_colour);
		glUniform3fv(state->terrain_shader.sand_colour, 1, (GLfloat *)&state->cur_preset.params.sand_colour);
		glUniform3fv(state->terrain_shader.snow_colour, 1, (GLfloat *)&state->cur_preset.params.snow_colour);

		glUniform3fv(state->terrain_shader.view_position, 1, (GLfloat *)&state->cur_cam.pos);

		glUniform1i(state->terrain_shader.shadow_map, 0);

		glUniformMatrix4fv(state->terrain_shader.light_space_matrix, 1, GL_FALSE, light_space_matrix);
		glUniformMatrix4fv(state->terrain_shader.view, 1, GL_FALSE, copy_cam.view);
		glUniformMatrix4fv(state->terrain_shader.projection, 1, GL_FALSE, copy_cam.ortho);

		glBindVertexArray(state->triangle_vao);

		glBindFramebuffer(GL_FRAMEBUFFER, state->texture_map_data.fbo);

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glViewport(0, 0, state->texture_map_data.resolution, state->texture_map_data.resolution);

		for (u32 y = 0; y < state->cur_preset.params.world_width; y++) {
			for (u32 x = 0; x < state->cur_preset.params.world_width; x++) {
				u32 index = y * state->cur_preset.params.world_width + x;
				glBindBuffer(GL_ARRAY_BUFFER, state->chunks[index]->vbo);
				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, state->chunks[index]->ebo);
				glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof Vertex, (void *)0);
				glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof Vertex, (void *)(3 * sizeof(real32)));

				real32 model[16];
				mat4_identity(model);
				mat4_translate(model, x * state->cur_preset.params.chunk_tile_length, 0, y * state->cur_preset.params.chunk_tile_length);
				glUniformMatrix4fv(state->terrain_shader.model, 1, GL_FALSE, model);

				glDrawElements(GL_TRIANGLES, state->chunks[index]->lod_data_infos[0].quads_count * 6, GL_UNSIGNED_INT, (void *)(0));
			}
		}

		glViewport(0, 0, state->window_info.w, state->window_info.h);

		glReadPixels(0, 0, state->texture_map_data.resolution, state->texture_map_data.resolution, GL_BGR, GL_UNSIGNED_BYTE, state->texture_map_data.pixels);

		tga_file.write((char *)state->texture_map_data.pixels, state->texture_map_data.resolution * state->texture_map_data.resolution * sizeof(RGB));

		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		tga_file.close();
	}
}

static void load_presets(app_state *state)
{
	for (auto &p : std::filesystem::directory_iterator("./presets/")) {
		if (p.path().extension() == ".world") {
			std::ifstream file(p.path(), std::ios::binary);

			if (file.good()) {
				preset_file p_file = {};
				p_file.name = p.path().stem().string();
				p_file.index = state->presets.size();
				file.read((char *)&p_file.params, sizeof(world_generation_parameters));

				state->presets.push_back(new preset_file(p_file));
			}

			file.close();
		}
	}
}

static void save_custom_preset_to_file(preset_file *p_file)
{
	std::ofstream file("./presets/" + p_file->name + ".world", std::ios::binary);

	if (file.good()) {
		file.write((char *)&p_file->params, sizeof(world_generation_parameters));
	}

	file.close();
}

static void app_render(app_state *state)
{
	real32 reflection_clip[4] = { 0, 1, 0, -state->cur_preset.params.water_pos.y };
	real32 refraction_clip[4] = { 0, -1, 0, state->cur_preset.params.water_pos.y };
	real32 no_clip[4] = { 0, -1, 0, 100000 };

	real32 light_projection[16], light_view[16];
	mat4_identity(light_projection);
	mat4_identity(light_view);
	mat4_ortho(light_projection, -1.f * state->world_tile_length, state->world_tile_length, -1.f * state->world_tile_length, state->world_tile_length, 1.f, 10000.f);
	mat4_look_at(light_view, state->light_pos, { 0.f, 0.f, 0.f }, { 0.f, 1.f, 0.f });

	real32 light_space_matrix[16];
	mat4_identity(light_space_matrix);
	mat4_multiply(light_space_matrix, light_projection, light_view);

	Camera camera_backup = state->cur_cam;
	Camera reflection_cam = state->cur_cam;
	real32 distance = 2.f * (state->cur_cam.pos.y - state->cur_preset.params.water_pos.y);
	reflection_cam.pos.y -= distance;
	reflection_cam.pitch *= -1;
	camera_update(&reflection_cam);
	camera_look_at(&reflection_cam);

	glEnable(GL_CLIP_DISTANCE0);
	
	glClearColor(state->cur_preset.params.skybox_colour.E[0], state->cur_preset.params.skybox_colour.E[1], state->cur_preset.params.skybox_colour.E[2], 1.f);

	// Reflection.
	glBindFramebuffer(GL_FRAMEBUFFER, state->water_frame_buffers.reflection_fbo);
	glViewport(0, 0, state->water_frame_buffers.REFLECTION_WIDTH, state->water_frame_buffers.REFLECTION_HEIGHT);
	
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	
	state->cur_cam = reflection_cam;
	
	terrain_shader_use(state, reflection_clip);
	glUniformMatrix4fv(state->terrain_shader.light_space_matrix, 1, GL_FALSE, light_space_matrix);
	
	for (u32 i = 0; i < state->chunk_count; i++) {
		app_render_chunk(state, reflection_clip, state->chunks[i], state->terrain_shader.model);
	}
	
	simple_shader_use(state);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, state->depth_map);
	glUniformMatrix4fv(state->simple_shader.light_space_matrix, 1, GL_FALSE, light_space_matrix);
	glUniform3fv(state->simple_shader.object_colour, 1, (GLfloat *)&state->cur_preset.params.trunk_colour);
	app_render_trunks(state, state->simple_shader.model);
	glUniform3fv(state->simple_shader.object_colour, 1, (GLfloat *)&state->cur_preset.params.leaves_colour);
	app_render_leaves(state, state->simple_shader.model);
	glUniform3fv(state->simple_shader.object_colour, 1, (GLfloat *)&state->cur_preset.params.rock_colour);
	app_render_rocks(state, state->simple_shader.model);
	
	// Restore camera.
	state->cur_cam = camera_backup;
	camera_update(&state->cur_cam);
	camera_look_at(&state->cur_cam);
	
	// Refraction.
	glBindFramebuffer(GL_FRAMEBUFFER, state->water_frame_buffers.refraction_fbo);
	glViewport(0, 0, state->water_frame_buffers.REFRACTION_WIDTH, state->water_frame_buffers.REFRACTION_HEIGHT);
	
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	
	terrain_shader_use(state, refraction_clip);
	glUniformMatrix4fv(state->terrain_shader.light_space_matrix, 1, GL_FALSE, light_space_matrix);
	
	for (u32 i = 0; i < state->chunk_count; i++) {
		app_render_chunk(state, refraction_clip, state->chunks[i], state->terrain_shader.model);
	}
	
	simple_shader_use(state);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, state->depth_map); 
	glUniformMatrix4fv(state->simple_shader.light_space_matrix, 1, GL_FALSE, light_space_matrix);
	glUniform3fv(state->simple_shader.object_colour, 1, (GLfloat *)&state->cur_preset.params.trunk_colour);
	app_render_trunks(state, state->simple_shader.model);
	glUniform3fv(state->simple_shader.object_colour, 1, (GLfloat *)&state->cur_preset.params.leaves_colour);
	app_render_leaves(state, state->simple_shader.model);
	glUniform3fv(state->simple_shader.object_colour, 1, (GLfloat *)&state->cur_preset.params.rock_colour);
	app_render_rocks(state, state->simple_shader.model);

	// Render to frame buffer
	glViewport(0, 0, 4096, 4096);
	glBindFramebuffer(GL_FRAMEBUFFER, state->depth_map_fbo);
	glClear(GL_DEPTH_BUFFER_BIT);

	// Render the shadow map from the lights POV.
	glUseProgram(state->depth_shader.program);
	glUniformMatrix4fv(state->depth_shader.projection, 1, GL_FALSE, light_projection);
	glUniformMatrix4fv(state->depth_shader.view, 1, GL_FALSE, light_view);
	
	glCullFace(GL_FRONT);

	for (u32 i = 0; i < state->chunk_count; i++) {
		app_render_chunk(state, no_clip, state->chunks[i], state->depth_shader.model);
	}
	glCullFace(GL_BACK);

	app_render_trunks(state, state->depth_shader.model);
	app_render_leaves(state, state->depth_shader.model);
	app_render_rocks(state, state->depth_shader.model);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, state->window_info.w, state->window_info.h);

	// Finally render to screen.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	terrain_shader_use(state, no_clip);
	glUniformMatrix4fv(state->terrain_shader.light_space_matrix, 1, GL_FALSE, light_space_matrix);
	
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, state->depth_map);

	for (u32 i = 0; i < state->chunk_count; i++) {
		app_render_chunk(state, no_clip, state->chunks[i], state->terrain_shader.model);
	}

	simple_shader_use(state);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, state->depth_map);

	glUniformMatrix4fv(state->simple_shader.light_space_matrix, 1, GL_FALSE, light_space_matrix);
	glUniform3fv(state->simple_shader.object_colour, 1, (GLfloat *)&state->cur_preset.params.trunk_colour);
	app_render_trunks(state, state->simple_shader.model);
	glUniform3fv(state->simple_shader.object_colour, 1, (GLfloat *)&state->cur_preset.params.leaves_colour);
	app_render_leaves(state, state->simple_shader.model);
	glUniform3fv(state->simple_shader.object_colour, 1, (GLfloat *)&state->cur_preset.params.rock_colour);
	app_render_rocks(state, state->simple_shader.model);

	glDisable(GL_CLIP_DISTANCE0);

	// Water
	glUseProgram(state->water_shader.program);
	glBindVertexArray(state->triangle_vao);
	
	glBindBuffer(GL_ARRAY_BUFFER, state->quad_vbo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, state->quad_ebo);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof Vertex, (void *)0);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof Vertex, (void *)(3 * sizeof(real32)));
	
	real32 model[16];
	mat4_identity(model);
	mat4_translate(model, state->world_tile_length / 2, state->cur_preset.params.water_pos.y, state->world_tile_length / 2);
	mat4_scale(model, state->world_tile_length, 1.f, state->world_tile_length);
	
	glUniformMatrix4fv(state->water_shader.projection, 1, GL_FALSE, state->cur_cam.frustrum);
	glUniformMatrix4fv(state->water_shader.view, 1, GL_FALSE, state->cur_cam.view);
	glUniformMatrix4fv(state->water_shader.model, 1, GL_FALSE, model);
	
	glUniform3fv(state->water_shader.water_colour, 1, (GLfloat *)&state->cur_preset.params.water_colour);
	
	glUniform1i(state->water_shader.reflection_texture, 0);
	glUniform1i(state->water_shader.refraction_texture, 1);
	
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, state->water_frame_buffers.reflection_texture);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, state->water_frame_buffers.refraction_texture);
	
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
	// End of water
	
	// UI	
	ImGui::NewFrame();

	IM_ASSERT(ImGui::GetCurrentContext() != NULL && "Missing dear imgui context. Refer to examples app!");
	ImGuiWindowFlags window_flags = 0;

	window_flags |= ImGuiWindowFlags_NoCollapse;
	window_flags |= ImGuiWindowFlags_NoResize;

	ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(300, 300), ImGuiCond_FirstUseEver);

	const s32 ui_item_width = -130;

	if (!ImGui::Begin("General Settings", NULL, window_flags)) {
		ImGui::End();
		return;
	}

	bool regenerate_chunks = false;
	bool regenerate_trees = false;
	bool regenerate_rocks = false;
	bool regenerate_lods = false;
	bool reseed = false;
	bool reinit_chunks = false;
	bool update_camera = false;

	ImGui::PushItemWidth(ui_item_width);

	if (ImGui::TreeNode("Debug")) {
		if (ImGui::Button("Toggle wireframe")) {
			state->wireframe = !state->wireframe;
		}

		u32 quads_displayed = 0;
		u32 quads_in_memory = 0;

		for (u32 i = 0; i < state->chunk_count; i++) {
			quads_in_memory += state->chunks[i]->lod_indices_count;

			u32 lod_detail = 0;

			if (state->chunks[i] != state->current_chunk) {
				const u32 dx = state->chunks[i]->x - state->current_chunk->x;
				const u32 dy = state->chunks[i]->y - state->current_chunk->y;
				distance = sqrtf(dx * dx + dy * dy);

				// Cap the distance to the highest (lowest detail) LOD.
				if (distance >= state->lod_settings.details_in_use) {
					distance = state->lod_settings.details_in_use - 1;
				}

				lod_detail = distance;
			}

			quads_displayed += state->chunks[i]->lod_data_infos[lod_detail].quads_count;
		}

		// Indices -> Quads.
		quads_in_memory /= 6;

		ImGui::Text("Quads onscreen: %d", quads_displayed);
		ImGui::Text("Quads in memory: %d", quads_in_memory);

		ImGui::TreePop();
	}

	if (ImGui::TreeNode("Export")) {

		ImGui::Checkbox("Include normals", (bool *)&state->export_settings.with_normals);

		ImGui::Checkbox("Diffuse map", (bool *)&state->export_settings.texture_map);
		if (state->export_settings.texture_map) {
			static int texture_map_resolution_current = 0;
			if (ImGui::Combo("texture resolution", &texture_map_resolution_current, texture_resolutions, 6 /* HARDCODED */)) {
				state->texture_map_data.resolution = atoi(texture_resolutions[texture_map_resolution_current]);
				create_terrain_texture_map_texture(&state->texture_map_data);
			}

			ImGui::Checkbox("Bake shadows", (bool *)&state->export_settings.bake_shadows);
		}

		ImGui::Checkbox("Chunks into seperate files", (bool *)&state->export_settings.seperate_chunks);

		ImGui::Checkbox("LODs", (bool *)&state->export_settings.lods);
		ImGui::Checkbox("Trees", (bool *)&state->export_settings.trees);
		ImGui::Checkbox("Rocks", (bool *)&state->export_settings.rocks);

		if (ImGui::Button("Go!")) {
			export_terrain(state);
		}

		ImGui::TreePop();
	}

	if (ImGui::TreeNode("Camera Settings")) {
		ImGui::Checkbox("Flying", (bool*)&state->cur_cam.flying);

		update_camera |= ImGui::SliderFloat("fov", &state->cur_cam.fov, 1.f, 120.f, "%.0f", ImGuiSliderFlags_None);
		ImGui::SliderFloat("walk speed", &state->cur_cam.walk_speed, 1.f, 10.f, "%.2f", ImGuiSliderFlags_None);
		ImGui::SliderFloat("flying speed", &state->cur_cam.fly_speed, 1.f, 200.f, "%.2f", ImGuiSliderFlags_None);

		ImGui::TreePop();
	}

	if (ImGui::TreeNode("Save/Load Presets")) {
		static bool save_as = false;

		std::string s = "Current preset: " + state->cur_preset.name;
		ImGui::Text(s.c_str());

		if (state->cur_preset.name != "default") {
			if (ImGui::Button("Rename")) {
				state->show_filename_prompt = true;
			}

			ImGui::SameLine();

			if (ImGui::Button("Save")) {
				save_custom_preset_to_file(&state->cur_preset);
				
				state->presets.at(state->cur_preset.index)->params = state->cur_preset.params;
			}

			ImGui::SameLine();
		}

		if (ImGui::Button("Save to new")) {
			state->show_filename_prompt = true;
			save_as = true;
		}

		if (state->show_filename_prompt) {
			ImGui::Separator();

			static const char *empty_error = "Please enter a filename";
			static const char *default_error = "'default' is a reserved name";
			
			static bool show_empty_error = false;
			static bool show_default_error = false;
	
			ImGui::InputText("Name", &state->new_preset_name);
			state->is_typing = ImGui::IsItemFocused();

			if (ImGui::Button("Confirm")) {
				show_empty_error = state->new_preset_name == "";
				show_default_error = state->new_preset_name == "default";

				if (!show_empty_error && !show_default_error) {
					if (save_as) {
						preset_file p_file = {};
						p_file.name = state->new_preset_name;
						p_file.index = state->presets.size();
						p_file.params = state->cur_preset.params;

						state->presets.push_back(new preset_file(p_file));
						state->cur_preset = *state->presets.back();
						state->new_preset_name = "";

						save_custom_preset_to_file(state->presets.back());
					} else {
						// Update the preset's filename.
						std::filesystem::path p = std::filesystem::current_path();
						std::filesystem::rename(p/(state->cur_preset.name + ".world"), p/(state->new_preset_name + ".world"));

						// Update the preset's name within the application.
						state->cur_preset.name = state->new_preset_name;
						state->presets.at(state->cur_preset.index)->name = state->cur_preset.name;
					}
					

					state->show_filename_prompt = false;
					save_as = false;
				}
			}
			
			ImGui::SameLine();
			
			if (ImGui::Button("Cancel")) {
				state->show_filename_prompt = false;
				state->new_preset_name = "";

				save_as = false;
			}

			if (show_empty_error) {
				ImGui::Text(empty_error);
			}
			else if (show_default_error) {
				ImGui::Text(default_error);
			}
		}

		ImGui::Separator();
		
		ImGui::Text("Presets:");

		for (auto &p : state->presets) {
			std::string s(p->name);
			if (ImGui::Button(s.c_str())) {
				reinit_chunks = 
						state->cur_preset.params.world_width != p->params.world_width 
					||	state->cur_preset.params.chunk_tile_length != p->params.chunk_tile_length;
				regenerate_chunks = true;
				state->cur_preset = *p;
			}
		}

		ImGui::TreePop();
	}

	ImGui::PopItemWidth();

	ImGui::End();

	ImGui::SetNextWindowPos(ImVec2(0, 300), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(300, 420), ImGuiCond_FirstUseEver);

	if (state->terrain_settings_open) {
		if (!ImGui::Begin("Terrain Settings", &state->terrain_settings_open, window_flags)) {
			ImGui::End();
			return;
		}

		ImGui::PushItemWidth(ui_item_width);

		reseed |= ImGui::InputInt("Seed", (int *)&state->cur_preset.params.seed); ImGui::SameLine();
		regenerate_chunks |= ImGui::Button("Regenerate");

		reinit_chunks |= ImGui::InputInt("World Width", (int *)&state->cur_preset.params.world_width);

		ImGui::Text("Chunk size:"); ImGui::SameLine();

		if (ImGui::Button("64")) {
			state->cur_preset.params.chunk_tile_length = 64;
			reinit_chunks |= true;
		} ImGui::SameLine();
		if (ImGui::Button("128")) {
			state->cur_preset.params.chunk_tile_length = 128;
			reinit_chunks |= true;
		} ImGui::SameLine();
		if (ImGui::Button("256")) {
			state->cur_preset.params.chunk_tile_length = 256;
			reinit_chunks |= true;
		} ImGui::SameLine();
		if (ImGui::Button("512")) {
			state->cur_preset.params.chunk_tile_length = 512;
			reinit_chunks |= true;
		}
		
		ImGui::Separator();

		if (ImGui::TreeNode("Level of detail")) {
			ImGui::SliderInt("number of LODs", (int *)&state->lod_settings.details_in_use, 1, state->lod_settings.max_available_count, "%d", ImGuiSliderFlags_None);

			regenerate_lods |= ImGui::SliderInt("LOD multiplier", (int *)&state->lod_settings.detail_multiplier, 1, state->lod_settings.max_detail_multiplier, "%d", ImGuiSliderFlags_None);

			ImGui::TreePop();
		}

		if (ImGui::TreeNode("Parameters")) {
			regenerate_chunks |= ImGui::SliderFloat("x offset", &state->cur_preset.params.x_offset, 0, 20.f, "%.2f", ImGuiSliderFlags_None);
			regenerate_chunks |= ImGui::SliderFloat("z offset", &state->cur_preset.params.z_offset, 0, 20.f, "%.2f", ImGuiSliderFlags_None);
			regenerate_chunks |= ImGui::SliderFloat("scale", &state->cur_preset.params.scale, 0.1, 10.f, "%.2f", ImGuiSliderFlags_None);
			regenerate_chunks |= ImGui::SliderFloat("height", &state->cur_preset.params.y_scale, 0.f, 200.f, "%.2f", ImGuiSliderFlags_None);
			regenerate_chunks |= ImGui::SliderFloat("ruggedness", &state->cur_preset.params.lacunarity, 0.f, 3.f, "%.2f", ImGuiSliderFlags_None);
			regenerate_chunks |= ImGui::SliderFloat("detail", &state->cur_preset.params.persistence, 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
			regenerate_chunks |= ImGui::SliderFloat("elevation factor", &state->cur_preset.params.elevation_power, 0.f, 5.f, "%.2f", ImGuiSliderFlags_None);
			regenerate_chunks |= ImGui::SliderInt("octaves", &state->cur_preset.params.max_octaves, 1, 20, "%d", ImGuiSliderFlags_None);
			ImGui::TreePop();
		}

		if (ImGui::TreeNode("Lighting")) {
			if (ImGui::TreeNode("Position")) {
				ImGui::SliderFloat("light x", &state->light_pos.E[0], -5000.f, 5000.f, "%.2f", ImGuiSliderFlags_None);
				ImGui::SliderFloat("light y", &state->light_pos.E[1], -5000.f, 5000.f, "%.2f", ImGuiSliderFlags_None);
				ImGui::SliderFloat("light z", &state->light_pos.E[2], -5000.f, 5000.f, "%.2f", ImGuiSliderFlags_None);
				ImGui::TreePop();
			}

			if (ImGui::TreeNode("Strength")) {
				ImGui::SliderFloat("ambient", &state->cur_preset.params.ambient_strength, 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
				ImGui::SliderFloat("diffuse", &state->cur_preset.params.diffuse_strength, 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
				ImGui::SliderFloat("specular", &state->cur_preset.params.specular_strength, 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
				ImGui::TreePop();
			}

			ImGui::TreePop();
		}

		if (ImGui::TreeNode("Colours")) {
			if (ImGui::TreeNode("Lighting")) {
				ImGui::SliderFloat("colour red", &state->cur_preset.params.light_colour.E[0], 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
				ImGui::SliderFloat("colour green", &state->cur_preset.params.light_colour.E[1], 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
				ImGui::SliderFloat("colour blue", &state->cur_preset.params.light_colour.E[2], 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
				ImGui::TreePop();
			}

			if (ImGui::TreeNode("Ground")) {
				ImGui::SliderFloat("ground colour red", &state->cur_preset.params.ground_colour.E[0], 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
				ImGui::SliderFloat("ground colour green", &state->cur_preset.params.ground_colour.E[1], 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
				ImGui::SliderFloat("ground colour blue", &state->cur_preset.params.ground_colour.E[2], 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
				ImGui::TreePop();
			}

			if (ImGui::TreeNode("Slope")) {
				ImGui::SliderFloat("red", &state->cur_preset.params.slope_colour.E[0], 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
				ImGui::SliderFloat("green", &state->cur_preset.params.slope_colour.E[1], 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
				ImGui::SliderFloat("blue", &state->cur_preset.params.slope_colour.E[2], 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
				ImGui::TreePop();
			}

			if (ImGui::TreeNode("Water")) {
				ImGui::SliderFloat("red", &state->cur_preset.params.water_colour.E[0], 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
				ImGui::SliderFloat("green", &state->cur_preset.params.water_colour.E[1], 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
				ImGui::SliderFloat("blue", &state->cur_preset.params.water_colour.E[2], 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
				ImGui::TreePop();
			}

			if (ImGui::TreeNode("Sand")) {
				ImGui::SliderFloat("red", &state->cur_preset.params.sand_colour.E[0], 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
				ImGui::SliderFloat("green", &state->cur_preset.params.sand_colour.E[1], 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
				ImGui::SliderFloat("blue", &state->cur_preset.params.sand_colour.E[2], 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
				ImGui::TreePop();
			}

			if (ImGui::TreeNode("Stone")) {
				ImGui::SliderFloat("stone colour red", &state->cur_preset.params.stone_colour.E[0], 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
				ImGui::SliderFloat("stone colour green", &state->cur_preset.params.stone_colour.E[1], 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
				ImGui::SliderFloat("stone colour blue", &state->cur_preset.params.stone_colour.E[2], 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
				ImGui::TreePop();
			}

			if (ImGui::TreeNode("Snow")) {
				ImGui::SliderFloat("red", &state->cur_preset.params.snow_colour.E[0], 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
				ImGui::SliderFloat("green", &state->cur_preset.params.snow_colour.E[1], 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
				ImGui::SliderFloat("blue", &state->cur_preset.params.snow_colour.E[2], 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
				ImGui::TreePop();
			}

			if (ImGui::TreeNode("Sky")) {
				ImGui::SliderFloat("red", &state->cur_preset.params.skybox_colour.E[0], 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
				ImGui::SliderFloat("green", &state->cur_preset.params.skybox_colour.E[1], 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
				ImGui::SliderFloat("blue", &state->cur_preset.params.skybox_colour.E[2], 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
				ImGui::TreePop();
			}

			if (ImGui::TreeNode("Tree Trunks")) {
				ImGui::SliderFloat("red", &state->cur_preset.params.trunk_colour.E[0], 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
				ImGui::SliderFloat("green", &state->cur_preset.params.trunk_colour.E[1], 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
				ImGui::SliderFloat("blue", &state->cur_preset.params.trunk_colour.E[2], 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
				ImGui::TreePop();
			}

			if (ImGui::TreeNode("Tree Leaves")) {
				ImGui::SliderFloat("red", &state->cur_preset.params.leaves_colour.E[0], 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
				ImGui::SliderFloat("green", &state->cur_preset.params.leaves_colour.E[1], 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
				ImGui::SliderFloat("blue", &state->cur_preset.params.leaves_colour.E[2], 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
				ImGui::TreePop();
			}

			if (ImGui::TreeNode("Rocks")) {
				ImGui::SliderFloat("red", &state->cur_preset.params.rock_colour.E[0], 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
				ImGui::SliderFloat("green", &state->cur_preset.params.rock_colour.E[1], 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
				ImGui::SliderFloat("blue", &state->cur_preset.params.rock_colour.E[2], 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
				ImGui::TreePop();
			}

			ImGui::TreePop();
		}

		if (ImGui::TreeNode("Heightmap")) {
			ImGui::SliderFloat("water height", &state->cur_preset.params.water_pos.y, 0.f, 50.f, "%.2f", ImGuiSliderFlags_None);
			ImGui::SliderFloat("sand start height", &state->cur_preset.params.sand_height, 0.f, 100.f, "%.2f", ImGuiSliderFlags_None);
			ImGui::SliderFloat("stone start height", &state->cur_preset.params.stone_height, 0.f, 250.f, "%.2f", ImGuiSliderFlags_None);
			ImGui::SliderFloat("snow start height", &state->cur_preset.params.snow_height, 0.f, 500.f, "%.2f", ImGuiSliderFlags_None);
			ImGui::TreePop();
		}

		if (ImGui::TreeNode("Features")) {
			if (ImGui::TreeNode("Trees")) {
				regenerate_trees |= ImGui::SliderInt("tree count", (int *)&state->cur_preset.params.tree_count, 0, state->trees_pos.size() + 100, "%d", ImGuiSliderFlags_None);
				ImGui::SliderFloat("tree size", &state->cur_preset.params.tree_size, 0.1f, 5.f, "%.2f", ImGuiSliderFlags_None);
				ImGui::SliderInt("tree min height", (int *)&state->cur_preset.params.tree_min_height, 0, 200, "%d", ImGuiSliderFlags_None);
				ImGui::SliderInt("tree max height", (int *)&state->cur_preset.params.tree_max_height, 0, 200, "%d", ImGuiSliderFlags_None);

				regenerate_trees |= ImGui::Button("Regenerate");
				ImGui::TreePop();
			}

			if (ImGui::TreeNode("Rocks")) {
				regenerate_rocks |= ImGui::SliderInt("rock count", (int *)&state->cur_preset.params.rock_count, 0, state->rocks_pos.size() + 100, "%d", ImGuiSliderFlags_None);
				ImGui::SliderFloat("rock size", &state->cur_preset.params.rock_size, 0.1f, 5.f, "%.2f", ImGuiSliderFlags_None);
				ImGui::SliderInt("rock min height", (int *)&state->cur_preset.params.rock_min_height, 0, 200, "%d", ImGuiSliderFlags_None);
				ImGui::SliderInt("rock max height", (int *)&state->cur_preset.params.rock_max_height, 0, 200, "%d", ImGuiSliderFlags_None);

				regenerate_rocks |= ImGui::Button("Regenerate");
				ImGui::TreePop();
			}
			ImGui::TreePop();
		}

		ImGui::PopItemWidth();

		ImGui::End();
	}

	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

	if (reseed) {
		state->rng.seed(state->cur_preset.params.seed);
		seed_perlin(state->rng);
	}

	if (regenerate_chunks || reinit_chunks) {
		if (reinit_chunks) {
			if (state->cur_preset.params.world_width < 1) {
				state->cur_preset.params.world_width = 1;
			}

			init_terrain(state, state->cur_preset.params.chunk_tile_length, state->cur_preset.params.world_width);
		}

		generate_world(state);
	}

	if (regenerate_lods) {
		init_lod_detail_levels(&state->lod_settings, state->cur_preset.params.chunk_tile_length);
		generate_world(state, true);
	}

	if (regenerate_trees) {
		generate_trees(state);
	}

	if (regenerate_rocks) {
		generate_rocks(state);
	}

	if (update_camera) {
		camera_frustrum(&state->cur_cam, state->window_info.w, state->window_info.h);
	}
	// End of UI
}

static void init_terrain(app_state *state, u32 chunk_tile_length, u32 world_width)
{
	// ---Terrain data.
	state->cur_preset.params.chunk_tile_length = chunk_tile_length;
	state->chunk_vertices_length = state->cur_preset.params.chunk_tile_length + 1;
	state->cur_preset.params.world_width = world_width;
	state->world_area = state->cur_preset.params.world_width * state->cur_preset.params.world_width;
	state->world_tile_length = state->cur_preset.params.chunk_tile_length * state->cur_preset.params.world_width;
	// ---End of terrain data.

	// ---LOD settings
	// detail_multiplier = n where 2^n = the rate of detail loss for each LOD level.
	// e.g detail multiplier = 2
	// LOD 0: quality: 1 / (2 ^ 2) * 0
	// LOD 1: quality: 1 / (2 ^ 2) * 1
	// LOD 2: quality: 1 / (2 ^ 2) * 2
	// ...
	state->lod_settings.detail_multiplier = 1;
	state->lod_settings.max_detail_multiplier = 1;

	// Calculate the maximum possible detail multiplier for the chunk size.
	while (pow(2, 5 + state->lod_settings.max_detail_multiplier) <= state->cur_preset.params.chunk_tile_length) {
		state->lod_settings.max_detail_multiplier++;
	}

	state->lod_settings.max_details_count = 10;
	state->lod_settings.details = (u32 *)malloc(state->lod_settings.max_details_count * sizeof(u32));

	init_lod_detail_levels(&state->lod_settings, state->cur_preset.params.chunk_tile_length);
	// ---end of LOD settings.

	if (state->world_area > state->chunks.size()) {
		u32 new_chunks = state->world_area - state->chunks.size();
		while (new_chunks-- > 0) {
			state->chunks.push_back(new Chunk);
			glGenBuffers(1, &state->chunks.back()->vbo);
			glGenBuffers(1, &state->chunks.back()->ebo);
		}
	}

	state->chunk_count = 0;
	for (u32 j = 0; j < state->cur_preset.params.world_width; j++) {
		for (u32 i = 0; i < state->cur_preset.params.world_width; i++) {
			u32 index = j * state->cur_preset.params.world_width + i;

			u64 chunk_vertices_size = (u64)state->chunk_vertices_length * state->chunk_vertices_length * sizeof(Vertex);
			u64 chunk_lods_size = (u64)state->lod_settings.max_available_count * state->cur_preset.params.chunk_tile_length * state->cur_preset.params.chunk_tile_length * sizeof(QuadIndices);
			u32 vertices_count = state->chunk_vertices_length * state->chunk_vertices_length;

			state->chunks[index]->vertices_count = vertices_count;
			state->chunks[index]->lod_indices_count = 0;
			state->chunks[index]->vertices.resize(chunk_vertices_size);
			state->chunks[index]->lods.resize(chunk_lods_size);
			state->chunks[index]->lod_data_infos.resize(state->lod_settings.max_available_count);
			state->chunks[index]->x = i;
			state->chunks[index]->y = j;

			state->chunk_count++;
		}
	}
}

app_state *app_init(u32 w, u32 h)
{
	app_state *state = new app_state;

	if (!state) {
		return nullptr;
	}

	state->window_info.w = w;
	state->window_info.h = h;
	state->window_info.resize = false;
	state->window_info.running = true;

	glGenVertexArrays(1, &state->triangle_vao);
	glBindVertexArray(state->triangle_vao);

	// --- Quad mesh
	real32 quad_verts[24] = {
		 0.5f,  0, 0.5f, 0, 0, 0,
		 0.5f, 0, -0.5f, 0, 0, 0,
		-0.5f, 0, -0.5f, 0, 0, 0,
		-0.5f,  0, 0.5f, 0, 0, 0,
	};

	u32 quad_indices[6] = {
		0, 1, 3,
		1, 2, 3
	};

	glGenBuffers(1, &state->quad_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, state->quad_vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof quad_verts, quad_verts, GL_STATIC_DRAW);

	glGenBuffers(1, &state->quad_ebo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, state->quad_ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof quad_indices, quad_indices, GL_STATIC_DRAW);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof Vertex, (void *)0);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof Vertex, (void *)(3 * sizeof(real32)));

	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	// ---End of quad mesh

	// ---Shaders
	state->terrain_shader.program = create_shader(Shaders::DEFAULT_VERTEX_SHADER_SOURCE, Shaders::DEFAULT_FRAGMENT_SHADER_SOURCE);
	state->terrain_shader.projection = glGetUniformLocation(state->terrain_shader.program, "projection");
	state->terrain_shader.view = glGetUniformLocation(state->terrain_shader.program, "view");
	state->terrain_shader.model = glGetUniformLocation(state->terrain_shader.program, "model");
	state->terrain_shader.light_space_matrix = glGetUniformLocation(state->terrain_shader.program, "light_space_matrix");
	state->terrain_shader.shadow_map = glGetUniformLocation(state->terrain_shader.program, "shadow_map");
	state->terrain_shader.light_pos = glGetUniformLocation(state->terrain_shader.program, "light_pos");
	state->terrain_shader.plane = glGetUniformLocation(state->terrain_shader.program, "plane");
	state->terrain_shader.sand_height = glGetUniformLocation(state->terrain_shader.program, "sand_height");
	state->terrain_shader.stone_height = glGetUniformLocation(state->terrain_shader.program, "stone_height");
	state->terrain_shader.snow_height = glGetUniformLocation(state->terrain_shader.program, "snow_height");
	state->terrain_shader.light_colour = glGetUniformLocation(state->terrain_shader.program, "light_colour");
	state->terrain_shader.ground_colour = glGetUniformLocation(state->terrain_shader.program, "ground_colour");
	state->terrain_shader.slope_colour = glGetUniformLocation(state->terrain_shader.program, "slope_colour");
	state->terrain_shader.sand_colour = glGetUniformLocation(state->terrain_shader.program, "sand_colour");
	state->terrain_shader.stone_colour = glGetUniformLocation(state->terrain_shader.program, "stone_colour");
	state->terrain_shader.snow_colour = glGetUniformLocation(state->terrain_shader.program, "snow_colour");
	state->terrain_shader.ambient_strength = glGetUniformLocation(state->terrain_shader.program, "ambient_strength");
	state->terrain_shader.diffuse_strength = glGetUniformLocation(state->terrain_shader.program, "diffuse_strength");
	state->terrain_shader.specular_strength = glGetUniformLocation(state->terrain_shader.program, "specular_strength");
	state->terrain_shader.gamma_correction = glGetUniformLocation(state->terrain_shader.program, "gamma_correction");
	state->terrain_shader.view_position = glGetUniformLocation(state->terrain_shader.program, "view_position");

	state->simple_shader.program = create_shader(Shaders::SIMPLE_VERTEX_SHADER_SOURCE, Shaders::SIMPLE_FRAGMENT_SHADER_SOURCE);
	state->simple_shader.projection = glGetUniformLocation(state->simple_shader.program, "projection");
	state->simple_shader.view = glGetUniformLocation(state->simple_shader.program, "view");
	state->simple_shader.model = glGetUniformLocation(state->simple_shader.program, "model");
	state->simple_shader.light_space_matrix = glGetUniformLocation(state->simple_shader.program, "light_space_matrix");
	state->simple_shader.shadow_map = glGetUniformLocation(state->simple_shader.program, "shadow_map");
	state->simple_shader.ambient_strength = glGetUniformLocation(state->simple_shader.program, "ambient_strength");
	state->simple_shader.diffuse_strength = glGetUniformLocation(state->simple_shader.program, "diffuse_strength");
	state->simple_shader.gamma_correction = glGetUniformLocation(state->simple_shader.program, "gamma_correction");
	state->simple_shader.light_pos = glGetUniformLocation(state->simple_shader.program, "light_pos");
	state->simple_shader.light_colour = glGetUniformLocation(state->simple_shader.program, "light_colour");
	state->simple_shader.object_colour = glGetUniformLocation(state->simple_shader.program, "object_colour");
	
	state->water_shader.program = create_shader(Shaders::WATER_VERTEX_SHADER_SOURCE, Shaders::WATER_FRAGMENT_SHADER_SOURCE);
	state->water_shader.projection = glGetUniformLocation(state->water_shader.program, "projection");
	state->water_shader.view = glGetUniformLocation(state->water_shader.program, "view");
	state->water_shader.model = glGetUniformLocation(state->water_shader.program, "model");
	state->water_shader.reflection_texture = glGetUniformLocation(state->water_shader.program, "reflection_texture");
	state->water_shader.refraction_texture = glGetUniformLocation(state->water_shader.program, "refraction_texture");
	state->water_shader.water_colour = glGetUniformLocation(state->water_shader.program, "water_colour");

	state->depth_shader.program = create_shader(Shaders::DEPTH_VERTEX_SHADER_SOURCE, Shaders::DEPTH_FRAGMENT_SHADER_SOURCE);
	state->depth_shader.projection = glGetUniformLocation(state->depth_shader.program, "projection");
	state->depth_shader.view = glGetUniformLocation(state->depth_shader.program, "view");
	state->depth_shader.model = glGetUniformLocation(state->depth_shader.program, "model");
	// ---End of shaders

	// --- Default generation parameters if no file is present.
	state->presets.push_back(new preset_file);

	state->presets[0]->name = "default";
	state->presets[0]->index = 0;
	state->presets[0]->params.seed = 10;
	state->presets[0]->params.chunk_tile_length = 64;
	state->presets[0]->params.world_width = 3;
	state->presets[0]->params.x_offset = 10.f;
	state->presets[0]->params.z_offset = 10.f;
	state->presets[0]->params.scale = 3.f;
	state->presets[0]->params.lacunarity = 1.65f;
	state->presets[0]->params.persistence = 0.5f;
	state->presets[0]->params.elevation_power = 3.f;
	state->presets[0]->params.y_scale = 35.f;
	state->presets[0]->params.max_octaves = 10;
	state->presets[0]->params.water_pos.x = 0;
	state->presets[0]->params.water_pos.y = 1.2f * state->presets[0]->params.scale;
	state->presets[0]->params.water_pos.z = 0;
	state->presets[0]->params.sand_height = 1.5f * state->presets[0]->params.scale;
	state->presets[0]->params.stone_height = 20.f * state->presets[0]->params.scale;
	state->presets[0]->params.snow_height = 50.f * state->presets[0]->params.scale;
	state->presets[0]->params.ambient_strength = 0.5f;
	state->presets[0]->params.diffuse_strength = 0.5f;
	state->presets[0]->params.specular_strength = 0.05f;
	state->presets[0]->params.gamma_correction = 2.2f;
	state->presets[0]->params.light_colour = { 1.f, 0.95f, 0.95f };
	state->presets[0]->params.ground_colour = { 0.07f, 0.2f, 0.07f };
	state->presets[0]->params.sand_colour = { 0.8f, 0.81f, 0.55f };
	state->presets[0]->params.stone_colour = { 0.2f, 0.2f, 0.2f };
	state->presets[0]->params.snow_colour = { 0.8f, 0.8f, 0.8f };
	state->presets[0]->params.slope_colour = { 0.45f, 0.5f, 0.35f };
	state->presets[0]->params.water_colour = { .31f, .31f, 0.35f };
	state->presets[0]->params.skybox_colour = { 0.65f, 0.65f, 1.f };
	state->presets[0]->params.trunk_colour = { 0.65f, 0.65f, 0.3f };
	state->presets[0]->params.leaves_colour = { 0.2f, 0.4f, 0.2f };
	state->presets[0]->params.rock_colour = { 0.3f, 0.3f, 0.3f };
	state->presets[0]->params.tree_count = 0;
	state->presets[0]->params.tree_size = 1.f;
	state->presets[0]->params.tree_min_height = state->presets[0]->params.sand_height;
	state->presets[0]->params.tree_max_height = state->presets[0]->params.snow_height;
	state->presets[0]->params.rock_count = 0;
	state->presets[0]->params.rock_size = 1.f;
	state->presets[0]->params.rock_min_height = state->presets[0]->params.sand_height;
	state->presets[0]->params.rock_max_height = state->presets[0]->params.snow_height;

	// Attempt to load custom parameters from file.
	load_presets(state);

	state->cur_preset = *state->presets[0];
	// ---End of generation parameters

	init_terrain(state, state->cur_preset.params.chunk_tile_length, state->cur_preset.params.world_width);
	init_water_data(state);
	init_terrain_texture_maps(state);
	init_depth_map(state);

	state->rng = std::mt19937(state->cur_preset.params.seed);

	seed_perlin(state->rng);
	generate_world(state);

	camera_init(&state->cur_cam);
	state->cur_cam.pos = { 0, 100.f, 0 };
	state->cur_cam.front = { 0.601920426f, -0.556864262f, 0.572358370f };
	state->cur_cam.yaw = 43.5579033f;
	state->cur_cam.pitch = -33.8392143;

	glViewport(0, 0, state->window_info.w, state->window_info.h);
	camera_frustrum(&state->cur_cam, state->window_info.w, state->window_info.h);
	camera_ortho(&state->cur_cam, state->window_info.w, state->window_info.h);

	state->light_pos = { -2000.f, 3000.f, 3000.f };

	state->export_settings = {};

	state->wireframe = false;

	state->rock = load_object("./data/rock.obj");
	create_vbos(state->rock);

	state->trunk = load_object("./data/trunk.obj");
	create_vbos(state->trunk);

	state->leaves = load_object("./data/leaves.obj");
	create_vbos(state->leaves);

	state->terrain_settings_open = true;
	state->general_settings_open = true;
	state->show_filename_prompt = false;
	state->new_preset_name = "";
	state->is_typing = false;

	return state;
}

void app_handle_input(real32 dt, app_state *state, app_keyboard_input *keyboard)
{
	if (!state->is_typing) {
		if (keyboard->forward.ended_down) {
			camera_move_forward(&state->cur_cam, dt);
		}
		else if (keyboard->backward.ended_down) {
			camera_move_backward(&state->cur_cam, dt);
		}

		if (keyboard->left.ended_down) {
			camera_move_left(&state->cur_cam, dt);
		}
		else if (keyboard->right.ended_down) {
			camera_move_right(&state->cur_cam, dt);
		}

		if (keyboard->cam_up.ended_down) {
			state->cur_cam.pitch += state->cur_cam.look_speed * dt;
		}
		else if (keyboard->cam_down.ended_down) {
			state->cur_cam.pitch -= state->cur_cam.look_speed * dt;
		}

		if (keyboard->cam_left.ended_down) {
			state->cur_cam.yaw -= state->cur_cam.look_speed * dt;
		}
		else if (keyboard->cam_right.ended_down) {
			state->cur_cam.yaw += state->cur_cam.look_speed * dt;
		}

		if (keyboard->wireframe.toggled) {
			state->wireframe = !state->wireframe;
		}

		if (keyboard->fly.toggled) {
			state->cur_cam.flying = !state->cur_cam.flying;
		}
	}
}

void app_update(app_state *state)
{
	camera_update(&state->cur_cam);
	camera_look_at(&state->cur_cam);

	// Keep track of current chunk for LODs and collision.
	real32 contrained_x = min(state->world_tile_length - 1, max(0, state->cur_cam.pos.x));
	real32 contrained_z = min(state->world_tile_length - 1, max(0, state->cur_cam.pos.z));

	const u32 current_chunk_x = (u32)(contrained_x / state->cur_preset.params.chunk_tile_length);
	const u32 current_chunk_z = (u32)(contrained_z / state->cur_preset.params.chunk_tile_length);

	state->current_chunk = state->chunks[current_chunk_z * state->cur_preset.params.world_width + current_chunk_x];

	if (!state->cur_cam.flying) {
		real32 cam_pos_x_relative = contrained_x - current_chunk_x * state->cur_preset.params.chunk_tile_length;
		real32 cam_pos_z_relative = contrained_z - current_chunk_z * state->cur_preset.params.chunk_tile_length;

		const u32 camera_vertex = (u32)cam_pos_z_relative * state->chunk_vertices_length + (u32)cam_pos_x_relative;
		QuadIndices *camera_quad = &state->current_chunk->lod_data_infos[0].quads[0];

		// Find the quad who's first vertex is the vertex bottom-left of the camera.
		for (u32 quad_index = 0; quad_index < state->current_chunk->lod_data_infos[0].quads_count; quad_index++) {
			if (state->current_chunk->lod_data_infos[0].quads[quad_index].i[2] == camera_vertex) {
				camera_quad = &state->current_chunk->lod_data_infos[0].quads[quad_index];
				break;
			}
		}

		// We now need to find which triangle the camera is in.
		// To do this we can compare the distance from the left edge with the bottom edge.
		// p1?===== p2
		// |       /|
		// |    /   | <---- p3?
		// | /      |
		// p0 ===== p1?
		V3 p3 = { cam_pos_x_relative, state->cur_cam.pos.y, cam_pos_z_relative };

		u32 i0, i1, i2;
		i0 = camera_vertex;
		
		// p2 connects both triangles so we can set the vertex early.
		i2 = camera_quad->i[0]; // Top right index.

		// Find which triangle the camera is on.
		if (contrained_x - state->current_chunk->vertices[i0].pos.x > contrained_z - state->current_chunk->vertices[i0].pos.z) {
			// bottom right triangle
			i1 = camera_quad->i[1]; // bottom right index.
		} else {
			// top left triangle
			i1 = camera_quad->i[3]; // top left index.
		}

		V3 p0 = state->current_chunk->vertices[i0].pos;
		V3 p1 = state->current_chunk->vertices[i1].pos;
		V3 p2 = state->current_chunk->vertices[i2].pos;

		// Plane equation found here from DanielKO's answer.
		// https://stackoverflow.com/questions/18755251/linear-interpolation-of-three-3d-points-in-3d-space
		const V3 N = v3_cross(p1 - p0, p2 - p0);
		const real32 y = p0.y - ((p3.x - p0.x) * N.x + (p3.z - p0.z) * N.z) / N.y;
		state->cur_cam.pos.y = y + 0.8f;
	}
}

void app_update_and_render(real32 dt, app_state *state, app_input *input, app_window_info *window_info)
{
	state->window_info = *window_info;

	if (!window_info->running) {
		app_on_destroy(state);
		return;
	}

	if (window_info->resize) {
		glViewport(0, 0, window_info->w, window_info->h);
		camera_frustrum(&state->cur_cam, window_info->w, window_info->h);
	}

	app_handle_input(dt, state, &input->keyboard);
	app_update(state);
	app_render(state);
}