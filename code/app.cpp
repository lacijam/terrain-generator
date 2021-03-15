#include "app.h"

#include <assert.h>
#include <stdlib.h>
#include <string>
#include <fstream>
#include <sstream>
#include <functional>

#include "maths.h"
#include "win32-opengl.h"
#include "opengl-util.h"
#include "perlin.h"
#include "shaders.h"

#include "imgui-master/imgui.h"
#include "imgui-master/imgui_impl_opengl3.h"

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

				real32 x = state->params->x_offset + (chunk->x + (real32)i / state->params->chunk_tile_length) / state->params->scale;
				real32 y = state->params->z_offset + (chunk->y + (real32)j / state->params->chunk_tile_length) / state->params->scale;

				real32 total = 0;
				real32 frequency = 1;
				real32 amplitude = 1;
				real32 total_amplitude = 0;

				for (u32 octave = 0; octave < state->params->max_octaves; octave++) {
					total += (0.5f + pattern({ (real32)(frequency * x), (real32)(frequency * y) })) * amplitude;
					total_amplitude += amplitude;
					amplitude *= state->params->persistence;
					frequency *= state->params->lacunarity;
				}

				real32 octave_result = total / total_amplitude;

				if (octave_result < 0) {
					octave_result = 0;
				}

				real32 elevation = ((powf(octave_result, state->params->elevation_power)) * state->params->y_scale * state->params->scale);

				chunk->vertices[index].pos.x = i;
				chunk->vertices[index].pos.y = elevation;
				chunk->vertices[index].pos.z = j;
				chunk->vertices[index].nor = {};
			}
		}

		// Calculate normals
		for (u32 j = 0; j < state->params->chunk_tile_length; j++) {
			for (u32 i = 0; i < state->params->chunk_tile_length; i++) {
				u32 index = j * state->params->chunk_tile_length + i;

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

		const u32 max_width = state->params->chunk_tile_length;
		const u32 max_height = state->params->chunk_tile_length;

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
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, 1024, 1024, 0, GL_DEPTH_COMPONENT, GL_FLOAT, 0);
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

static void app_render_chunk(app_state *state, real32 *clip, Chunk *chunk)
{
	glUniform4fv(state->terrain_shader.plane, 1, clip);

	glUniform1f(state->terrain_shader.ambient_strength, state->params->ambient_strength);
	glUniform1f(state->terrain_shader.diffuse_strength, state->params->diffuse_strength);
	glUniform1f(state->terrain_shader.specular_strength, state->params->specular_strength);
	glUniform1f(state->terrain_shader.gamma_correction, state->params->gamma_correction);

	glUniform3fv(state->terrain_shader.light_pos, 1, (GLfloat *)(&state->light_pos));
	glUniform1f(state->terrain_shader.sand_height, state->params->sand_height);
	glUniform1f(state->terrain_shader.stone_height, state->params->stone_height);
	glUniform1f(state->terrain_shader.snow_height, state->params->snow_height);

	glUniform3fv(state->terrain_shader.light_colour, 1, (GLfloat *)&state->params->light_colour);
	glUniform3fv(state->terrain_shader.slope_colour, 1, (GLfloat *)&state->params->slope_colour);
	glUniform3fv(state->terrain_shader.ground_colour, 1, (GLfloat *)&state->params->ground_colour);
	glUniform3fv(state->terrain_shader.sand_colour, 1, (GLfloat *)&state->params->sand_colour);
	glUniform3fv(state->terrain_shader.stone_colour, 1, (GLfloat *)&state->params->stone_colour);
	glUniform3fv(state->terrain_shader.snow_colour, 1, (GLfloat *)&state->params->snow_colour);

	glUniform3fv(state->terrain_shader.view_position, 1, (GLfloat *)&state->cur_cam.pos);

	glUniform1i(state->terrain_shader.shadow_map, 0);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, state->depth_map);

	glBindVertexArray(state->triangle_vao);

	u32 chunk_index = chunk->y * state->params->world_width + chunk->x;

	glBindBuffer(GL_ARRAY_BUFFER, state->chunks[chunk_index]->vbo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, state->chunks[chunk_index]->ebo);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof Vertex, (void *)0);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof Vertex, (void *)(3 * sizeof(real32)));

	real32 model[16];
	mat4_identity(model);
	mat4_translate(model, chunk->x * state->params->chunk_tile_length , 0, chunk->y * state->params->chunk_tile_length);

	glUniformMatrix4fv(state->terrain_shader.model, 1, GL_FALSE, model);

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

	if (state->wireframe) {
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		V3 grid_colour = { 1.f, 1.f, 1.f };
		glUniform3fv(state->terrain_shader.light_colour, 1, (GLfloat *)&grid_colour);
		glUniform3fv(state->terrain_shader.slope_colour, 1, (GLfloat *)&grid_colour);
		glUniform3fv(state->terrain_shader.ground_colour, 1, (GLfloat *)&grid_colour);
		glUniform3fv(state->terrain_shader.sand_colour, 1, (GLfloat *)&grid_colour);
		glUniform3fv(state->terrain_shader.stone_colour, 1, (GLfloat *)&grid_colour);
		glUniform3fv(state->terrain_shader.snow_colour, 1, (GLfloat *)&grid_colour);
		glDrawElements(GL_TRIANGLES, lod_indices_area, GL_UNSIGNED_INT, (void *)(chunk_offset_in_bytes));
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	}
}

static void app_render_lights_and_features(app_state *state)
{
	glUseProgram(state->simple_shader.program);

	glUniform1f(state->simple_shader.ambient_strength, state->params->ambient_strength);
	glUniform1f(state->simple_shader.diffuse_strength, state->params->diffuse_strength);
	glUniform1f(state->simple_shader.gamma_correction, state->params->gamma_correction);

	glUniform3fv(state->simple_shader.light_pos, 1, (GLfloat *)(&state->light_pos));
	glUniform3fv(state->simple_shader.light_colour, 1, (GLfloat *)&state->params->light_colour);
	glUniform3fv(state->simple_shader.object_colour, 1, (GLfloat *)&state->params->tree_colour);

	glUniformMatrix4fv(state->simple_shader.projection, 1, GL_FALSE, state->cur_cam.frustrum);
	glUniformMatrix4fv(state->simple_shader.view, 1, GL_FALSE, state->cur_cam.view);

	real32 model[16];

	glBindVertexArray(state->triangle_vao);

	// Render trees
	/*glBindBuffer(GL_ARRAY_BUFFER, state->tree->vbos[0]);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, state->tree->vbos[1]);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof Vertex, (void *)0);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof Vertex, (void *)(3 * sizeof(real32)));

	for (u32 i = 0; i < state->params->tree_count; i++) {
		if (state->trees[i].y < state->params->tree_min_height || state->trees[i].y > state->params->tree_max_height) {
			continue;
		}

		const u32 scale = 1.f * state->params->scale;

		mat4_identity(model);
		mat4_translate(model, state->trees[i].x, state->trees[i].y + (scale / 2), state->trees[i].z);
		mat4_scale(model, 1.f, scale, 1.f);
		glUniformMatrix4fv(state->simple_shader.model, 1, GL_FALSE, model);
		glDrawElements(GL_TRIANGLES, 4 * state->tree->polygons.size(), GL_UNSIGNED_INT, 0);
	}*/

	glUniform3fv(state->simple_shader.object_colour, 1, (GLfloat *)&state->params->rock_colour);

	glBindVertexArray(state->triangle_vao);
	glBindBuffer(GL_ARRAY_BUFFER, state->rock->vbos[0]);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, state->rock->vbos[1]);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof Vertex, (void *)0);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof Vertex, (void *)(3 * sizeof(real32)));

	// Render rocks.
	for (u32 i = 0; i < state->params->rock_count; i++) {
		if (state->rocks_pos[i].y < state->params->rock_min_height || state->rocks_pos[i].y > state->params->rock_max_height) {
			continue;
		}

		mat4_identity(model);
		mat4_translate(model, state->rocks_pos[i].x, state->rocks_pos[i].y, state->rocks_pos[i].z);
		mat4_rotate_x(model, state->rocks_rotation[i].x);
		mat4_rotate_y(model, state->rocks_rotation[i].y);
		mat4_rotate_z(model, state->rocks_rotation[i].z);
		mat4_scale(model, state->params->rock_size, state->params->rock_size, state->params->rock_size);

		glUniformMatrix4fv(state->simple_shader.model, 1, GL_FALSE, model);
		glDrawElements(GL_TRIANGLES, 3 * state->rock->polygons.size(), GL_UNSIGNED_INT, 0);
	}
	// End of features
}


static void generate_trees(app_state *state)
{
	for (u32 i = 0; i < state->params->max_trees; i++) {
		real32 x, y, z;
		x = y = z = -1;

		u32 attempt = 0;
		while (y < state->params->tree_min_height || y > state->params->tree_max_height) {
			if (++attempt > 50) {
				break;
			}

			std::uniform_int_distribution<> chunk_index(0, state->world_area - 1);

			Chunk *chunk = state->chunks[chunk_index(state->rng)];

			std::uniform_int_distribution<> vertex_index(0, chunk->vertices_count - 1);

			Vertex *v = &chunk->vertices[vertex_index(state->rng)];
			x = chunk->x * state->params->chunk_tile_length + v->pos.x;
			y = v->pos.y;
			z = chunk->y * state->params->chunk_tile_length + v->pos.z;
			state->trees[i].x = x;
			state->trees[i].y = y;
			state->trees[i].z = z;
		}
	}
}

static void generate_rocks(app_state *state)
{
	for (u32 i = 0; i < state->params->max_rocks; i++) {
		real32 x, y, z;
		x = y = z = -1;

		u32 attempt = 0;
		while (y < state->params->rock_min_height || y > state->params->rock_max_height) {
			if (++attempt > 50) {
				break;
			}

			std::uniform_int_distribution<> chunk_index(0, state->world_area - 1);

			Chunk *chunk = state->chunks[chunk_index(state->rng)];

			std::uniform_int_distribution<> vertex_index(0, chunk->vertices_count - 1);

			Vertex *v = &chunk->vertices[vertex_index(state->rng)];
			x = chunk->x * state->params->chunk_tile_length + v->pos.x;
			y = v->pos.y;
			z = chunk->y * state->params->chunk_tile_length + v->pos.z;
			state->rocks_pos[i].x = x;
			state->rocks_pos[i].y = y;
			state->rocks_pos[i].z = z;
			
			std::uniform_real_distribution<> rotation_distr(0, 360);

			state->rocks_rotation[i].x = (acos(v3_dot(v->nor, { 1, 0, 0 })) * 180.f/ M_PI) ;
			state->rocks_rotation[i].y = rotation_distr(state->rng);
			state->rocks_rotation[i].z = (acos(v3_dot(v->nor, { 0, 0, 1 })) * 180.f / M_PI);
		}
	}
}

static void generate_world(app_state *state, bool32 just_lods = false)
{
	for (u32 j = 0; j < state->params->world_width; j++) {
		for (u32 i = 0; i < state->params->world_width; i++) {
			state->generation_threads.push_back(std::thread(app_generate_terrain_chunk, state, state->chunks[j * state->params->world_width + i], just_lods));
		}
	}

	for (u32 i = 0; i < state->world_area; i++) {
		state->generation_threads[i].join();
	}

	state->generation_threads.clear();

	glBindVertexArray(state->triangle_vao);

	u64 quads_sum = 0;
	for (u32 j = 0; j < state->params->world_width; j++) {
		for (u32 i = 0; i < state->params->world_width; i++) {
			u32 index = j * state->params->world_width + i;

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

static void export_terrain_chunk(app_state *state, Chunk *chunk)
{
	std::string filename = "terrain_chunk_" + std::to_string(chunk->y) + "_" + std::to_string(chunk->x) + ".obj";
	std::ofstream object_file("export/" + filename, std::ios::out);

	if (object_file.good()) {
		object_file << "mtllib terrain.mtl" << std::endl;
		object_file << "usemtl textured" << std::endl;
		object_file << "o " << filename << std::endl;

		for (u32 vertex = 0; vertex < chunk->vertices_count; vertex++) {
			Vertex *current_vertex = &chunk->vertices[vertex];
			// Offset chunk vertices by world position.
			real32 x = current_vertex->pos.x + chunk->x * (state->params->chunk_tile_length);
			real32 y = current_vertex->pos.y;
			real32 z = current_vertex->pos.z + chunk->y * (state->params->chunk_tile_length);

			object_file << "v " << x << " " << y << " " << z;
			object_file << std::endl;
		}

		if (state->export_settings.texture_map) {
			for (u32 vertex_row = 0; vertex_row < state->chunk_vertices_length; vertex_row++) {
				for (u32 vertex_col = 0; vertex_col < state->chunk_vertices_length; vertex_col++) {
					real32 u = (real32)(chunk->y * state->chunk_vertices_length + vertex_row) / (state->params->world_width * state->chunk_vertices_length);
					real32 v = (real32)(chunk->x * state->chunk_vertices_length + vertex_col) / (state->params->world_width * state->chunk_vertices_length);

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

static void export_terrain_one_obj(app_state *state)
{
	std::ofstream object_file("export/terrain.obj", std::ios::out);

	if (object_file.good()) {
		object_file << "mtllib terrain.mtl" << std::endl;
		object_file << "usemtl textured" << std::endl;
		object_file << "o Terrain" << std::endl;

		for (u32 chunk_z = 0; chunk_z < state->params->world_width; chunk_z++) {
			for (u32 chunk_x = 0; chunk_x < state->params->world_width; chunk_x++) {
				u32 chunk_index = chunk_z * state->params->world_width + chunk_x;

				object_file << "# Chunk" << chunk_index << " vertices" << std::endl;

				for (u32 vertex = 0; vertex < state->chunks[chunk_index]->vertices_count; vertex++) {
					Vertex *current_vertex = &state->chunks[chunk_index]->vertices[vertex];
					// Offset chunk vertices by world position.
					real32 x = current_vertex->pos.x + chunk_x * (state->params->chunk_tile_length);
					real32 y = current_vertex->pos.y;
					real32 z = current_vertex->pos.z + chunk_z * (state->params->chunk_tile_length);

					object_file << "v " << x << " " << y << " " << z;
					object_file << std::endl;
				}
			}
		}

		if (state->export_settings.texture_map) {
			for (u32 chunk_z = 0; chunk_z < state->params->world_width; chunk_z++) {
				for (u32 chunk_x = 0; chunk_x < state->params->world_width; chunk_x++) {
					for (u32 vertex_row = 0; vertex_row < state->chunk_vertices_length; vertex_row++) {
						for (u32 vertex_col = 0; vertex_col < state->chunk_vertices_length; vertex_col++) {
							real32 u = (real32)(chunk_z * state->chunk_vertices_length + vertex_row) / (state->params->world_width * state->chunk_vertices_length);
							real32 v = (real32)(chunk_x * state->chunk_vertices_length + vertex_col) / (state->params->world_width * state->chunk_vertices_length);

							object_file << "vt " << u << " " << v << std::endl;
						}
					}
				}
			}
		}

		if (state->export_settings.with_normals) {
			for (u32 chunk_z = 0; chunk_z < state->params->world_width; chunk_z++) {
				for (u32 chunk_x = 0; chunk_x < state->params->world_width; chunk_x++) {
					u32 chunk_index = chunk_z * state->params->world_width + chunk_x;

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

		for (u32 chunk_z = 0; chunk_z < state->params->world_width; chunk_z++) {
			for (u32 chunk_x = 0; chunk_x < state->params->world_width; chunk_x++) {
				u32 chunk_index = chunk_z * state->params->world_width + chunk_x;

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
	if (state->export_settings.seperate_chunks) {
		for (u32 j = 0; j < state->params->world_width; j++) {
			for (u32 i = 0; i < state->params->world_width; i++) {
				state->generation_threads.push_back(std::thread(export_terrain_chunk, state, state->chunks[j * state->params->world_width + i]));
			}
		}

		for (u32 i = 0; i < state->world_area; i++) {
			state->generation_threads[i].join();
		}

		state->generation_threads.clear();
	}
	else {
		export_terrain_one_obj(state);
	}

	std::ofstream material_file("export/terrain.mtl", std::ios::out);

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

	if (state->export_settings.texture_map) {
		std::ofstream tga_file("export/diffuse.tga", std::ios::binary);
		if (!tga_file) return;

		char header[18] = { 0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };
		header[12] = state->texture_map_data.resolution & 0xFF;
		header[13] = (state->texture_map_data.resolution >> 8) & 0xFF;
		header[14] = (state->texture_map_data.resolution) & 0xFF;
		header[15] = (state->texture_map_data.resolution >> 8) & 0xFF;
		header[16] = 24;

		tga_file.write((char *)header, 18);

		glBindVertexArray(state->triangle_vao);
		glUseProgram(state->terrain_shader.program);

		glBindFramebuffer(GL_FRAMEBUFFER, state->texture_map_data.fbo);

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		real32 no_clip[4] = { 0, -1, 0, 100000 };

		Camera copy_cam = state->cur_cam;
		camera_ortho(&copy_cam, state->world_tile_length, state->world_tile_length);
		copy_cam.pos = { 0, 9000, 0 };
		copy_cam.front = { 0, -1, 0 };
		copy_cam.up = { 1, 0, 0 };
		camera_look_at(&copy_cam); 
		
		real32 light_projection[16], light_view[16];
		mat4_identity(light_projection);
		mat4_identity(light_view);
		mat4_ortho(light_projection, -10.f, 10.f, -10.f, 10.f, 1.f, 100.f);
		mat4_look_at(light_view, state->light_pos, { state->world_tile_length / 2.f, 0.f, state->world_tile_length / 2.f }, { 1.f, 0.f, 0.f });

		real32 light_space_matrix[16];
		mat4_identity(light_space_matrix);
		mat4_multiply(light_space_matrix, light_projection, light_view);

		V3 light_pos = state->light_pos;

		// Put the light directly above the terrain if we dont want shadows.
		if (!state->export_settings.bake_shadows) {
			light_pos = { ((real32)state->params->chunk_tile_length / 2) * state->params->world_width, 5000.f, ((real32)state->params->chunk_tile_length / 2) * state->params->world_width };
		}

		glUniform4fv(state->terrain_shader.plane, 1, no_clip);

		glUniform1f(state->terrain_shader.ambient_strength, state->params->ambient_strength);
		glUniform1f(state->terrain_shader.diffuse_strength, state->params->diffuse_strength);
		glUniform1f(state->terrain_shader.specular_strength, state->params->specular_strength);
		glUniform1f(state->terrain_shader.diffuse_strength, state->params->gamma_correction);

		glUniform3fv(state->terrain_shader.light_pos, 1, (GLfloat *)(&light_pos));
		glUniform1f(state->terrain_shader.sand_height, state->params->sand_height);
		glUniform1f(state->terrain_shader.snow_height, state->params->snow_height);

		glUniform3fv(state->terrain_shader.light_colour, 1, (GLfloat *)&state->params->light_colour);
		glUniform3fv(state->terrain_shader.slope_colour, 1, (GLfloat *)&state->params->slope_colour);
		glUniform3fv(state->terrain_shader.ground_colour, 1, (GLfloat *)&state->params->ground_colour);
		glUniform3fv(state->terrain_shader.sand_colour, 1, (GLfloat *)&state->params->sand_colour);
		glUniform3fv(state->terrain_shader.snow_colour, 1, (GLfloat *)&state->params->snow_colour);

		glUniform3fv(state->terrain_shader.view_position, 1, (GLfloat *)&state->cur_cam.pos);

		glUniform1i(state->terrain_shader.shadow_map, 0);

		glUniformMatrix4fv(state->terrain_shader.light_space_matrix, 1, GL_FALSE, light_space_matrix);
		glUniformMatrix4fv(state->terrain_shader.view, 1, GL_FALSE, copy_cam.view);
		glUniformMatrix4fv(state->terrain_shader.projection, 1, GL_FALSE, copy_cam.ortho);

		glBindTexture(GL_TEXTURE_2D, state->depth_map);
		glActiveTexture(GL_TEXTURE0);

		glDisable(GL_DEPTH_TEST);
		glViewport(0, 0, state->texture_map_data.resolution, state->texture_map_data.resolution);

		for (u32 y = 0; y < state->params->world_width; y++) {
			for (u32 x = 0; x < state->params->world_width; x++) {
				u32 index = y * state->params->world_width + x;
				glBindBuffer(GL_ARRAY_BUFFER, state->chunks[index]->vbo);
				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, state->chunks[index]->ebo);
				glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof Vertex, (void *)0);
				glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof Vertex, (void *)(3 * sizeof(real32)));

				real32 model[16];
				mat4_identity(model);
				mat4_translate(model, x * state->params->chunk_tile_length, 0, y * state->params->chunk_tile_length);
				glUniformMatrix4fv(state->terrain_shader.model, 1, GL_FALSE, model);

				glDrawElements(GL_TRIANGLES, state->chunks[index]->lod_data_infos[0].quads_count * 6, GL_UNSIGNED_INT, (void *)(0));
			}
		}

		glViewport(0, 0, state->window_info.w, state->window_info.h);
		glEnable(GL_DEPTH_TEST);

		glReadPixels(0, 0, state->texture_map_data.resolution, state->texture_map_data.resolution, GL_BGR, GL_UNSIGNED_BYTE, state->texture_map_data.pixels);

		tga_file.write((char *)state->texture_map_data.pixels, state->texture_map_data.resolution * state->texture_map_data.resolution * sizeof(RGB));

		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		tga_file.close();
	}
}

static void load_custom_preset_from_file(app_state *state)
{
	std::ifstream file("custom.world", std::ios::binary);

	if (file.good()) {
		file.read((char *)&state->custom_parameters, sizeof(world_generation_parameters));
	}

	file.close();
}

static void save_custom_preset_to_file(app_state *state)
{
	std::ofstream file("custom.world", std::ios::binary);

	if (file.good()) {
		file.write((char *)&state->custom_parameters, sizeof(world_generation_parameters));
	}

	file.close();
}

static void app_render(app_state *state)
{
	real32 reflection_clip[4] = { 0, 1, 0, -state->params->water_pos.y };
	real32 refraction_clip[4] = { 0, -1, 0, state->params->water_pos.y };
	real32 no_clip[4] = { 0, -1, 0, 100000 };

	Camera camera_backup = state->cur_cam;
	Camera reflection_cam = state->cur_cam;
	real32 distance = 2.f * (state->cur_cam.pos.y - state->params->water_pos.y);
	reflection_cam.pos.y -= distance;
	reflection_cam.pitch *= -1;
	camera_update(&reflection_cam);
	camera_look_at(&reflection_cam);

	glEnable(GL_CLIP_DISTANCE0);

	// Reflection.
	glBindFramebuffer(GL_FRAMEBUFFER, state->water_frame_buffers.reflection_fbo);
	glViewport(0, 0, state->water_frame_buffers.REFLECTION_WIDTH, state->water_frame_buffers.REFLECTION_HEIGHT);

	glClearColor(state->params->skybox_colour.E[0], state->params->skybox_colour.E[1], state->params->skybox_colour.E[2], 1.f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	state->cur_cam = reflection_cam;

	glUseProgram(state->terrain_shader.program);
	glUniformMatrix4fv(state->terrain_shader.projection, 1, GL_FALSE, state->cur_cam.frustrum);
	glUniformMatrix4fv(state->terrain_shader.view, 1, GL_FALSE, state->cur_cam.view);

	for (u32 i = 0; i < state->chunk_count; i++) {
		app_render_chunk(state, reflection_clip, state->chunks[i]);
	}

	app_render_lights_and_features(state);

	// Restore camera.
	state->cur_cam = camera_backup;
	camera_update(&state->cur_cam);
	camera_look_at(&state->cur_cam);

	// Refraction.
	glBindFramebuffer(GL_FRAMEBUFFER, state->water_frame_buffers.refraction_fbo);
	glViewport(0, 0, state->water_frame_buffers.REFRACTION_WIDTH, state->water_frame_buffers.REFRACTION_HEIGHT);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glUseProgram(state->terrain_shader.program);
	glUniformMatrix4fv(state->terrain_shader.projection, 1, GL_FALSE, state->cur_cam.frustrum);
	glUniformMatrix4fv(state->terrain_shader.view, 1, GL_FALSE, state->cur_cam.view);

	for (u32 i = 0; i < state->chunk_count; i++) {
		app_render_chunk(state, refraction_clip, state->chunks[i]);
	}

	app_render_lights_and_features(state);

	// Render to frame buffer
	glViewport(0, 0, 1024, 1024);
	glBindFramebuffer(GL_FRAMEBUFFER, state->depth_map_fbo);
	glClear(GL_DEPTH_BUFFER_BIT);
	
	// Render the shadow map from the lights POV.
	real32 light_projection[16], light_view[16];
	mat4_identity(light_projection);
	mat4_identity(light_view);
	mat4_ortho(light_projection, -10.f, 10.f, -10.f, 10.f, 1.f, 100.f);
	mat4_look_at(light_view, state->light_pos, { state->world_tile_length / 2.f, 0.f, state->world_tile_length / 2.f }, { 0.f, 1.f, 0.f });

	glUseProgram(state->depth_shader.program);
	glUniformMatrix4fv(state->depth_shader.projection, 1, GL_FALSE, light_projection);
	glUniformMatrix4fv(state->depth_shader.view, 1, GL_FALSE, light_view);
	
	for (u32 i = 0; i < state->chunk_count; i++) {
		app_render_chunk(state, no_clip, state->chunks[i]);
	}
	
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, state->window_info.w, state->window_info.h);

	// Finally render to screen.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	real32 light_space_matrix[16];
	mat4_identity(light_space_matrix);
	mat4_multiply(light_space_matrix, light_projection, light_view);

	glUseProgram(state->terrain_shader.program);
	glUniformMatrix4fv(state->terrain_shader.projection, 1, GL_FALSE, state->cur_cam.frustrum);
	glUniformMatrix4fv(state->terrain_shader.view, 1, GL_FALSE, state->cur_cam.view);
	glUniformMatrix4fv(state->terrain_shader.light_space_matrix, 1, GL_FALSE, light_space_matrix);

	for (u32 i = 0; i < state->chunk_count; i++) {
		app_render_chunk(state, no_clip, state->chunks[i]);
	}

	app_render_lights_and_features(state);

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
	mat4_translate(model, state->world_tile_length / 2, state->params->water_pos.y, state->world_tile_length / 2);
	mat4_scale(model, state->world_tile_length, 1.f, state->world_tile_length);

	glUniformMatrix4fv(state->water_shader.projection, 1, GL_FALSE, state->cur_cam.frustrum);
	glUniformMatrix4fv(state->water_shader.view, 1, GL_FALSE, state->cur_cam.view);
	glUniformMatrix4fv(state->water_shader.model, 1, GL_FALSE, model);

	glUniform3fv(state->water_shader.water_colour, 1, (GLfloat *)&state->params->water_colour);

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

	u32 new_chunk_length = state->params->chunk_tile_length;

	ImGui::PushItemWidth(ui_item_width);

	if (ImGui::TreeNode("Debug")) {
		if (ImGui::Button("Toggle wireframe")) {
			state->wireframe = !state->wireframe;
		}
		if (ImGui::Button("Toggle flying")) {
			state->flying = !state->flying;
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

		ImGui::Checkbox("Ambient + Diffuse map", (bool *)&state->export_settings.texture_map);
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

		if (ImGui::Button("Go!")) {
			export_terrain(state);
		}

		ImGui::TreePop();
	}

	if (ImGui::TreeNode("Camera Settings")) {
		update_camera |= ImGui::SliderFloat("fov", &state->cur_cam.fov, 1.f, 120.f, "%.0f", ImGuiSliderFlags_None);

		ImGui::TreePop();
	}

	if (ImGui::TreeNode("Presets")) {
		bool custom, plains, desert, mountains;
		custom = plains = desert = mountains = false;

		custom = ImGui::Button("Custom"); ImGui::SameLine();
		if (ImGui::Button("Save")) {
			save_custom_preset_to_file(state);
		}

		plains = ImGui::Button("Green Plains");
		desert = ImGui::Button("Rugged Desert");
		mountains = ImGui::Button("Harsh Mountains");
		regenerate_chunks = custom || plains || desert || mountains;

		if (custom) {
			state->params = &state->custom_parameters;
		}
		else if (plains) {
			state->params = &state->green_plains_parameters;
		}
		else if (desert) {
			state->params = &state->rugged_desert_parameters;
		}
		else if (mountains) {
			state->params = &state->harsh_mountains_parameters;
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

		reseed |= ImGui::InputInt("Seed", (int *)&state->params->seed); ImGui::SameLine();
		regenerate_chunks |= ImGui::Button("Regenerate");

		static int world_width_ui = state->params->world_width;
		reinit_chunks |= ImGui::InputInt("World Width", (int *)&world_width_ui);

		if (world_width_ui != state->params->world_width) {
			state->params->world_width = world_width_ui;
		}

		ImGui::Text("Chunk size:"); ImGui::SameLine();

		if (ImGui::Button("64")) {
			new_chunk_length = 64;
			reinit_chunks |= true;
		} ImGui::SameLine();
		if (ImGui::Button("128")) {
			new_chunk_length = 128;
			reinit_chunks |= true;
		} ImGui::SameLine();
		if (ImGui::Button("256")) {
			new_chunk_length = 256;
			reinit_chunks |= true;
		} ImGui::SameLine();
		if (ImGui::Button("512")) {
			new_chunk_length = 512;
			reinit_chunks |= true;
		}
		
		ImGui::Separator();

		if (ImGui::TreeNode("Level of detail")) {
			ImGui::SliderInt("number of LODs", (int *)&state->lod_settings.details_in_use, 1, state->lod_settings.max_available_count, "%d", ImGuiSliderFlags_None);

			regenerate_lods |= ImGui::SliderInt("LOD multiplier", (int *)&state->lod_settings.detail_multiplier, 1, state->lod_settings.max_detail_multiplier, "%d", ImGuiSliderFlags_None);

			ImGui::TreePop();
		}

		if (ImGui::TreeNode("Parameters")) {
			regenerate_chunks |= ImGui::SliderFloat("x offset", &state->params->x_offset, 0, 20.f, "%.2f", ImGuiSliderFlags_None);
			regenerate_chunks |= ImGui::SliderFloat("z offset", &state->params->z_offset, 0, 20.f, "%.2f", ImGuiSliderFlags_None);
			regenerate_chunks |= ImGui::SliderFloat("scale", &state->params->scale, 0.1, 10.f, "%.2f", ImGuiSliderFlags_None);
			regenerate_chunks |= ImGui::SliderFloat("height", &state->params->y_scale, 0.f, 200.f, "%.2f", ImGuiSliderFlags_None);
			regenerate_chunks |= ImGui::SliderFloat("ruggedness", &state->params->lacunarity, 0.f, 3.f, "%.2f", ImGuiSliderFlags_None);
			regenerate_chunks |= ImGui::SliderFloat("detail", &state->params->persistence, 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
			regenerate_chunks |= ImGui::SliderFloat("elevation factor", &state->params->elevation_power, 0.f, 5.f, "%.2f", ImGuiSliderFlags_None);
			regenerate_chunks |= ImGui::SliderInt("octaves", &state->params->max_octaves, 1, 20, "%d", ImGuiSliderFlags_None);
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
				ImGui::SliderFloat("ambient", &state->params->ambient_strength, 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
				ImGui::SliderFloat("diffuse", &state->params->diffuse_strength, 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
				ImGui::SliderFloat("specular", &state->params->specular_strength, 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
				ImGui::TreePop();
			}

			ImGui::TreePop();
		}

		if (ImGui::TreeNode("Colours")) {
			if (ImGui::TreeNode("Lighting")) {
				ImGui::SliderFloat("colour red", &state->params->light_colour.E[0], 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
				ImGui::SliderFloat("colour green", &state->params->light_colour.E[1], 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
				ImGui::SliderFloat("colour blue", &state->params->light_colour.E[2], 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
				ImGui::TreePop();
			}

			if (ImGui::TreeNode("Ground")) {
				ImGui::SliderFloat("ground colour red", &state->params->ground_colour.E[0], 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
				ImGui::SliderFloat("ground colour green", &state->params->ground_colour.E[1], 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
				ImGui::SliderFloat("ground colour blue", &state->params->ground_colour.E[2], 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
				ImGui::TreePop();
			}

			if (ImGui::TreeNode("Slope")) {
				ImGui::SliderFloat("red", &state->params->slope_colour.E[0], 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
				ImGui::SliderFloat("green", &state->params->slope_colour.E[1], 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
				ImGui::SliderFloat("blue", &state->params->slope_colour.E[2], 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
				ImGui::TreePop();
			}

			if (ImGui::TreeNode("Water")) {
				ImGui::SliderFloat("red", &state->params->water_colour.E[0], 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
				ImGui::SliderFloat("green", &state->params->water_colour.E[1], 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
				ImGui::SliderFloat("blue", &state->params->water_colour.E[2], 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
				ImGui::TreePop();
			}

			if (ImGui::TreeNode("Sand")) {
				ImGui::SliderFloat("red", &state->params->sand_colour.E[0], 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
				ImGui::SliderFloat("green", &state->params->sand_colour.E[1], 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
				ImGui::SliderFloat("blue", &state->params->sand_colour.E[2], 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
				ImGui::TreePop();
			}

			if (ImGui::TreeNode("Stone")) {
				ImGui::SliderFloat("stone colour red", &state->params->stone_colour.E[0], 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
				ImGui::SliderFloat("stone colour green", &state->params->stone_colour.E[1], 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
				ImGui::SliderFloat("stone colour blue", &state->params->stone_colour.E[2], 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
				ImGui::TreePop();
			}

			if (ImGui::TreeNode("Snow")) {
				ImGui::SliderFloat("red", &state->params->snow_colour.E[0], 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
				ImGui::SliderFloat("green", &state->params->snow_colour.E[1], 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
				ImGui::SliderFloat("blue", &state->params->snow_colour.E[2], 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
				ImGui::TreePop();
			}

			if (ImGui::TreeNode("Sky")) {
				ImGui::SliderFloat("red", &state->params->skybox_colour.E[0], 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
				ImGui::SliderFloat("green", &state->params->skybox_colour.E[1], 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
				ImGui::SliderFloat("blue", &state->params->skybox_colour.E[2], 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
				ImGui::TreePop();
			}

			if (ImGui::TreeNode("Rock")) {
				ImGui::SliderFloat("red", &state->params->rock_colour.E[0], 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
				ImGui::SliderFloat("green", &state->params->rock_colour.E[1], 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
				ImGui::SliderFloat("blue", &state->params->rock_colour.E[2], 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
				ImGui::TreePop();
			}

			ImGui::TreePop();
		}

		if (ImGui::TreeNode("Heightmap")) {
			ImGui::SliderFloat("water height", &state->params->water_pos.y, 0.f, 50.f, "%.2f", ImGuiSliderFlags_None);
			ImGui::SliderFloat("sand start height", &state->params->sand_height, 0.f, 100.f, "%.2f", ImGuiSliderFlags_None);
			ImGui::SliderFloat("stone start height", &state->params->stone_height, 0.f, 250.f, "%.2f", ImGuiSliderFlags_None);
			ImGui::SliderFloat("snow start height", &state->params->snow_height, 0.f, 500.f, "%.2f", ImGuiSliderFlags_None);
			ImGui::TreePop();
		}

		if (ImGui::TreeNode("Features")) {
			if (ImGui::TreeNode("Trees")) {
				ImGui::SliderInt("tree count", (int *)&state->params->tree_count, 0, state->params->max_trees, "%d", ImGuiSliderFlags_None);
				ImGui::SliderInt("tree min height", (int *)&state->params->tree_min_height, 0, 200, "%d", ImGuiSliderFlags_None);
				ImGui::SliderInt("tree max height", (int *)&state->params->tree_max_height, 0, 200, "%d", ImGuiSliderFlags_None);

				if (ImGui::TreeNode("Colour")) {
					ImGui::SliderFloat("red", &state->params->tree_colour.E[0], 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
					ImGui::SliderFloat("green", &state->params->tree_colour.E[1], 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
					ImGui::SliderFloat("blue", &state->params->tree_colour.E[2], 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
					ImGui::TreePop();
				}

				regenerate_trees |= ImGui::Button("Regenerate");
				ImGui::TreePop();
			}

			if (ImGui::TreeNode("Rocks")) {
				ImGui::SliderInt("rock count", (int *)&state->params->rock_count, 0, state->params->max_rocks, "%d", ImGuiSliderFlags_None);
				ImGui::SliderFloat("rock size", &state->params->rock_size, 0.1f, 5.f, "%.2f", ImGuiSliderFlags_None);
				ImGui::SliderInt("rock min height", (int *)&state->params->rock_min_height, 0, 200, "%d", ImGuiSliderFlags_None);
				ImGui::SliderInt("rock max height", (int *)&state->params->rock_max_height, 0, 200, "%d", ImGuiSliderFlags_None);

				if (ImGui::TreeNode("Colour")) {
					ImGui::SliderFloat("red", &state->params->rock_colour.E[0], 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
					ImGui::SliderFloat("green", &state->params->rock_colour.E[1], 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
					ImGui::SliderFloat("blue", &state->params->rock_colour.E[2], 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
					ImGui::TreePop();
				}

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
		state->rng.seed(state->params->seed);
		seed_perlin(state->rng);
	}

	if (regenerate_chunks || reinit_chunks) {
		if (reinit_chunks) {
			if (state->params->world_width < 1) {
				state->params->world_width = 1;
			}

			init_terrain(state, new_chunk_length, state->params->world_width);
		}

		generate_world(state);
	}

	if (regenerate_lods) {
		init_lod_detail_levels(&state->lod_settings, state->params->chunk_tile_length);
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
	state->params->chunk_tile_length = chunk_tile_length;
	state->chunk_vertices_length = state->params->chunk_tile_length + 1;
	state->params->world_width = world_width;
	state->world_area = state->params->world_width * state->params->world_width;
	state->world_tile_length = state->params->chunk_tile_length * state->params->world_width;
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
	while (pow(2, 5 + state->lod_settings.max_detail_multiplier) <= state->params->chunk_tile_length) {
		state->lod_settings.max_detail_multiplier++;
	}

	state->lod_settings.max_details_count = 10;
	state->lod_settings.details = (u32 *)malloc(state->lod_settings.max_details_count * sizeof(u32));

	init_lod_detail_levels(&state->lod_settings, state->params->chunk_tile_length);
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
	for (u32 j = 0; j < state->params->world_width; j++) {
		for (u32 i = 0; i < state->params->world_width; i++) {
			u32 index = j * state->params->world_width + i;

			u64 chunk_vertices_size = (u64)state->chunk_vertices_length * state->chunk_vertices_length * sizeof(Vertex);
			u64 chunk_lods_size = (u64)state->lod_settings.max_available_count * state->params->chunk_tile_length * state->params->chunk_tile_length * sizeof(QuadIndices);
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
	state->custom_parameters.seed = 10;
	state->custom_parameters.chunk_tile_length = 64;
	state->custom_parameters.world_width = 3;
	state->custom_parameters.x_offset = 10.f;
	state->custom_parameters.z_offset = 10.f;
	state->custom_parameters.scale = 3.f;
	state->custom_parameters.lacunarity = 1.65f;
	state->custom_parameters.persistence = 0.5f;
	state->custom_parameters.elevation_power = 3.f;
	state->custom_parameters.y_scale = 35.f;
	state->custom_parameters.max_octaves = 10;
	state->custom_parameters.water_pos.x = 0;
	state->custom_parameters.water_pos.y = 1.2f * state->custom_parameters.scale;
	state->custom_parameters.water_pos.z = 0;
	state->custom_parameters.sand_height = 1.5f * state->custom_parameters.scale;
	state->custom_parameters.stone_height = 20.f * state->custom_parameters.scale;
	state->custom_parameters.snow_height = 50.f * state->custom_parameters.scale;
	state->custom_parameters.ambient_strength = 0.5f;
	state->custom_parameters.diffuse_strength = 0.5f;
	state->custom_parameters.specular_strength = 0.05f;
	state->custom_parameters.gamma_correction = 2.2f;
	state->custom_parameters.light_colour = { 1.f, 0.95f, 0.95f };
	state->custom_parameters.ground_colour = { 0.07f, 0.2f, 0.07f };
	state->custom_parameters.sand_colour = { 0.8f, 0.81f, 0.55f };
	state->custom_parameters.stone_colour = { 0.2f, 0.2f, 0.2f };
	state->custom_parameters.snow_colour = { 0.8f, 0.8f, 0.8f };
	state->custom_parameters.slope_colour = { 0.45f, 0.5f, 0.35f };
	state->custom_parameters.water_colour = { .31f, .31f, 0.35f };
	state->custom_parameters.skybox_colour = { 0.65f, 0.65f, 1.f };
	state->custom_parameters.tree_colour = { 0.65f, 0.65f, 0.3f };
	state->custom_parameters.rock_colour = { 0.3f, 0.3f, 0.3f };
	state->custom_parameters.tree_count = 0;
	state->custom_parameters.tree_min_height = state->custom_parameters.sand_height;
	state->custom_parameters.tree_max_height = state->custom_parameters.snow_height;
	state->custom_parameters.max_trees = 10000;
	state->custom_parameters.rock_count = 0;
	state->custom_parameters.rock_size = 1.f;
	state->custom_parameters.rock_min_height = state->custom_parameters.sand_height;
	state->custom_parameters.rock_max_height = state->custom_parameters.snow_height;
	state->custom_parameters.max_rocks = 1000;

	// Built in presets are based off the default custom parameters.
	state->green_plains_parameters = state->custom_parameters;
	state->green_plains_parameters.elevation_power = 2.5f;
	state->green_plains_parameters.y_scale = 40.;
	state->green_plains_parameters.lacunarity = 1.65f;
	state->green_plains_parameters.water_pos.y = 4.f;
	state->green_plains_parameters.sand_height = 4.5f;

	state->rugged_desert_parameters = state->custom_parameters;
	state->rugged_desert_parameters.persistence = 0.6f;
	state->rugged_desert_parameters.lacunarity = 1.7f;
	state->rugged_desert_parameters.y_scale = 60.f;
	state->rugged_desert_parameters.water_pos.y = -1;
	state->rugged_desert_parameters.sand_height = 110;
	state->rugged_desert_parameters.snow_height = 75;

	state->harsh_mountains_parameters = state->custom_parameters;
	state->harsh_mountains_parameters.y_scale = 120.f;
	state->harsh_mountains_parameters.lacunarity = 1.7f;
	state->harsh_mountains_parameters.persistence = 0.51f;
	state->harsh_mountains_parameters.elevation_power = 4.f;

	// Attempt to load custom parameters from file.
	load_custom_preset_from_file(state);

	state->params = &state->custom_parameters;
	// ---End of generation parameters

	init_terrain(state, state->params->chunk_tile_length, state->params->world_width);
	init_water_data(state);
	init_terrain_texture_maps(state);
	init_depth_map(state);

	state->trees = (V3 *)malloc(state->params->max_trees * sizeof V3);
	state->rocks_pos = (V3 *)malloc(state->params->max_rocks * sizeof V3);
	state->rocks_rotation = (V3 *)malloc(state->params->max_rocks * sizeof V3);

	state->rng = std::mt19937(state->params->seed);

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

	state->light_pos = { -5000.f, 5000.f, ((real32)state->params->chunk_tile_length / 2) * state->params->world_width * 1.f};

	state->export_settings = {};

	state->wireframe = false;
	state->flying = true;

	state->rock = load_object("Rock1.obj");
	create_vbos(state->rock);

	state->terrain_settings_open = true;
	state->general_settings_open = true;

	return state;
}

void app_handle_input(real32 dt, app_state *state, app_keyboard_input *keyboard)
{
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

	if (keyboard->reset.toggled) {
		seed_perlin(state->rng);
		generate_world(state);
	}

	if (keyboard->fly.toggled) {
		state->flying = !state->flying;
	}
}

void app_update(app_state *state)
{
	// Keep track of current chunk for LODs and collision.
	real32 contrained_x = min(state->world_tile_length - 1, max(0, state->cur_cam.pos.x));
	real32 contrained_z = min(state->world_tile_length - 1, max(0, state->cur_cam.pos.z));

	const u32 current_chunk_x = (u32)(contrained_x / state->params->chunk_tile_length);
	const u32 current_chunk_z = (u32)(contrained_z / state->params->chunk_tile_length);

	state->current_chunk = state->chunks[current_chunk_z * state->params->world_width + current_chunk_x];

	if (!state->flying) {
		real32 cam_pos_x_relative = contrained_x - current_chunk_x * state->params->chunk_tile_length;
		real32 cam_pos_z_relative = contrained_z - current_chunk_z * state->params->chunk_tile_length;

		const u32 i0 = (u32)cam_pos_z_relative * state->chunk_vertices_length + (u32)cam_pos_x_relative;
		QuadIndices *camera_quad = &state->current_chunk->lod_data_infos[0].quads[0];

		// Find the quad who's first vertex is the vertex bottom-left of the camera.
		for (u32 quad_index = 0; quad_index < state->current_chunk->lod_data_infos[0].quads_count; quad_index++) {
			if (state->current_chunk->lod_data_infos[0].quads[quad_index].i[2] == i0) {
				camera_quad = &state->current_chunk->lod_data_infos[0].quads[quad_index];
				break;
			}
		}

		// See mesh generation for how triangles are arranged.
		const u32 i1 = camera_quad->i[1];
		const u32 i2 = camera_quad->i[3];
		const u32 i3 = camera_quad->i[0];

		V3 p0 = state->current_chunk->vertices[i0].pos;
		V3 p1 = state->current_chunk->vertices[i1].pos;
		V3 p2 = state->current_chunk->vertices[i2].pos;
		V3 p3 = state->current_chunk->vertices[i3].pos;

		V3 n = v3_cross(p1 - p0, p2 - p1);
		n = v3_normalise(n);

		real32 d = v3_dot(n, p0);
		const real32 y = (d - (n.x * cam_pos_x_relative) - (n.z * cam_pos_z_relative)) / n.y;

		state->cur_cam.pos.y = y + 0.7f;
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