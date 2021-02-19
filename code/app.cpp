#include "app.h"

#include <assert.h>
#include <stdlib.h>
#include <mutex>
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

void* my_malloc(app_memory* memory, u64 size)
{
#ifdef _DEBUG
	assert(memory->free_offset + size < memory->permenant_storage_size);
#endif
	void* p = (void*)((char*)(memory->permenant_storage) + memory->free_offset);
	memory->free_offset += size;
	return p;
}

static real32 noise(V2 p) {
	return 0.5f + perlin(p);
}

static void app_generate_terrain_chunk(
	app_state *state
	, app_memory *memory
	, Chunk *chunk)
{
	for (u32 j = 0; j < state->chunk_vertices_length; j++) {
		for (u32 i = 0; i < state->chunk_vertices_length; i++) {
			u32 index = j * state->chunk_vertices_length + i;

			real32 x = (chunk->x + (real32)i / state->chunk_tile_length) / state->params->scale;
			real32 y = (chunk->y + (real32)j / state->chunk_tile_length) / state->params->scale;

			real32 total = 0;
			real32 frequency = 1;
			real32 amplitude = 1;
			real32 total_amplitude = 0;

			for (u32 octave = 0; octave < state->params->max_octaves; octave++) {
				total += noise({ (real32)(frequency * x), (real32)(frequency * y) }) * amplitude;
				total_amplitude += amplitude;
				amplitude *= state->params->persistence;
				frequency *= state->params->lacunarity;
			}

			real32 octave_result = total / total_amplitude;

			if (octave_result < 0) {
				octave_result = 0;
			}

			const real32 elevation = ((powf(octave_result, state->params->elevation_power)) * state->params->y_scale * state->params->scale);

			chunk->vertices[index].pos.x = i;
			chunk->vertices[index].pos.y = elevation;
			chunk->vertices[index].pos.z = j;
			chunk->vertices[index].nor = {};
		}
	}

	u64 lod_offset = 0;
	for (u32 lod_detail_index = 0; lod_detail_index < state->lod_settings.max_available_count; lod_detail_index++) {
		const u32 detail = state->lod_settings.details[lod_detail_index];

		u32 indices_length = state->chunk_vertices_length - detail;

		for (u32 j = 0; j < indices_length; j += detail) {
			for (u32 i = 0; i < indices_length; i += detail) {
				u32 data_pos = lod_offset + ((j / detail) * (state->chunk_tile_length / detail) + (i / detail));

				u32 v0 = j * state->chunk_vertices_length + i;
				u32 v1 = j * state->chunk_vertices_length + i + detail;
				u32 v2 = (j + detail) * state->chunk_vertices_length + i;
				u32 v3 = (j + detail) * state->chunk_vertices_length + i + detail;

				chunk->lods[data_pos].i[0] = v3;
				chunk->lods[data_pos].i[1] = v1;
				chunk->lods[data_pos].i[2] = v0;
				chunk->lods[data_pos].i[3] = v2;
				chunk->lods[data_pos].i[4] = v3;
				chunk->lods[data_pos].i[5] = v0;
			}
		}

		chunk->lod_offsets[lod_detail_index] = lod_offset;

		lod_offset += (state->chunk_tile_length / detail) * (state->chunk_tile_length / detail);
	}

	chunk->lod_index_count = lod_offset;

	// Calculate normals for each vertex for each sum normals of surrounding.
	for (u32 j = 0; j < state->chunk_tile_length; j++) {
		for (u32 i = 0; i < state->chunk_tile_length; i++) {
			u32 pos = j * state->chunk_tile_length + i;
			for (u32 tri = 0; tri < 2; tri++) {
				Vertex* a = &chunk->vertices[chunk->lods[pos].i[tri * 3 + 0]];
				Vertex* b = &chunk->vertices[chunk->lods[pos].i[tri * 3 + 1]];
				Vertex* c = &chunk->vertices[chunk->lods[pos].i[tri * 3 + 2]];
				V3 cp = v3_cross(b->pos - a->pos, c->pos - a->pos);
				a->nor += cp;
				b->nor += cp;
				c->nor += cp;
			}
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

static void app_init_shaders(app_state *state)
{
	state->terrain_shader.program = create_shader(Shaders::DEFAULT_VERTEX_SHADER_SOURCE, Shaders::DEFAULT_FRAGMENT_SHADER_SOURCE);
	state->terrain_shader.projection  = glGetUniformLocation(state->terrain_shader.program, "projection");
	state->terrain_shader.view        = glGetUniformLocation(state->terrain_shader.program, "view");
	state->terrain_shader.model       = glGetUniformLocation(state->terrain_shader.program, "model");
	state->terrain_shader.model       = glGetUniformLocation(state->terrain_shader.program, "model");
	state->terrain_shader.light_pos   = glGetUniformLocation(state->terrain_shader.program, "light_pos");
	state->terrain_shader.plane       = glGetUniformLocation(state->terrain_shader.program, "plane");
	state->terrain_shader.sand_height = glGetUniformLocation(state->terrain_shader.program, "sand_height");
	state->terrain_shader.snow_height = glGetUniformLocation(state->terrain_shader.program, "snow_height");
	state->terrain_shader.light_colour = glGetUniformLocation(state->terrain_shader.program, "light_colour");
	state->terrain_shader.grass_colour = glGetUniformLocation(state->terrain_shader.program, "grass_colour");
	state->terrain_shader.slope_colour = glGetUniformLocation(state->terrain_shader.program, "slope_colour");
	state->terrain_shader.sand_colour  = glGetUniformLocation(state->terrain_shader.program, "sand_colour");
	state->terrain_shader.snow_colour  = glGetUniformLocation(state->terrain_shader.program, "snow_colour");
	state->terrain_shader.ambient_strength = glGetUniformLocation(state->terrain_shader.program, "ambient_strength");
	state->terrain_shader.diffuse_strength = glGetUniformLocation(state->terrain_shader.program, "diffuse_strength");
	state->terrain_shader.specular_strength = glGetUniformLocation(state->terrain_shader.program, "specular_strength");
	state->terrain_shader.view_position = glGetUniformLocation(state->terrain_shader.program, "view_position");

	state->simple_shader.program = create_shader(Shaders::SIMPLE_VERTEX_SHADER_SOURCE, Shaders::SIMPLE_FRAGMENT_SHADER_SOURCE);
	state->simple_shader.projection = glGetUniformLocation(state->simple_shader.program, "projection");
	state->simple_shader.view       = glGetUniformLocation(state->simple_shader.program, "view");
	state->simple_shader.model      = glGetUniformLocation(state->simple_shader.program, "model");
	state->simple_shader.plane      = glGetUniformLocation(state->simple_shader.program, "plane");

	state->water_shader.program = create_shader(Shaders::WATER_VERTEX_SHADER_SOURCE, Shaders::WATER_FRAGMENT_SHADER_SOURCE);
	state->water_shader.projection         = glGetUniformLocation(state->water_shader.program, "projection");
	state->water_shader.view               = glGetUniformLocation(state->water_shader.program, "view");
	state->water_shader.model              = glGetUniformLocation(state->water_shader.program, "model");
	state->water_shader.reflection_texture = glGetUniformLocation(state->water_shader.program, "reflection_texture");
	state->water_shader.refraction_texture = glGetUniformLocation(state->water_shader.program, "refraction_texture");
	state->water_shader.water_colour       = glGetUniformLocation(state->water_shader.program, "water_colour");
}

static u32 create_framebuffer_texture(u32 width, u32 height)
{
	u32 tex = 0;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
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

static void init_terrain_texture_map_data(app_state* state, app_memory *memory)
{
	u32 pixels_size = state->texture_map_data.MAX_RESOLUTION * state->texture_map_data.MAX_RESOLUTION * sizeof(RGB);
	state->texture_map_data.pixels = (RGB*)my_malloc(memory, pixels_size);

	ZeroMemory(state->texture_map_data.pixels, pixels_size);

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
		glDeleteBuffers(1, &state->chunks[i].ebo);
		glDeleteBuffers(1, &state->chunks[i].vbo);
	}

	glDeleteVertexArrays(1, &state->terrain_vao);

	glDeleteBuffers(1, &state->quad_vbo);
	glDeleteBuffers(1, &state->quad_ebo);
	glDeleteVertexArrays(1, &state->simple_vao);

    glDeleteProgram(state->terrain_shader.program);
    glDeleteProgram(state->simple_shader.program);
    glDeleteProgram(state->water_shader.program);
}

static void app_render_chunk(app_state *state, real32 *clip, Chunk *chunk)
{
	glUseProgram(state->terrain_shader.program);

	glUniform4fv(state->terrain_shader.plane, 1, clip);

	glUniform1f(state->terrain_shader.ambient_strength, state->params->ambient_strength);
	glUniform1f(state->terrain_shader.diffuse_strength, state->params->diffuse_strength);
	glUniform1f(state->terrain_shader.specular_strength, state->params->specular_strength);

	glUniform3fv(state->terrain_shader.light_pos, 1, (GLfloat*)(&state->light_pos));
	glUniform1f(state->terrain_shader.sand_height, state->params->sand_height);
	glUniform1f(state->terrain_shader.snow_height, state->params->snow_height);

	glUniform3fv(state->terrain_shader.light_colour, 1, (GLfloat*)&state->params->light_colour);
	glUniform3fv(state->terrain_shader.slope_colour, 1, (GLfloat*)&state->params->slope_colour);
	glUniform3fv(state->terrain_shader.grass_colour, 1, (GLfloat*)&state->params->grass_colour);
	glUniform3fv(state->terrain_shader.sand_colour, 1, (GLfloat*)&state->params->sand_colour);
	glUniform3fv(state->terrain_shader.snow_colour, 1, (GLfloat*)&state->params->snow_colour);

	glUniform3fv(state->terrain_shader.view_position, 1, (GLfloat*)&state->cur_cam.pos);

	glBindVertexArray(state->terrain_vao);

	u32 chunk_index = chunk->y * state->world_width + chunk->x;

	glBindBuffer(GL_ARRAY_BUFFER, state->chunks[chunk_index].vbo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, state->chunks[chunk_index].ebo);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof Vertex, (void*)0);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof Vertex, (void*)(3 * sizeof(real32)));

	real32 model[16];
	mat4_identity(model);
	mat4_translate(model, chunk->x * state->chunk_tile_length, 0, chunk->y * state->chunk_tile_length);

	glUniformMatrix4fv(state->terrain_shader.projection, 1, GL_FALSE, state->cur_cam.frustrum);
	glUniformMatrix4fv(state->terrain_shader.view, 1, GL_FALSE, state->cur_cam.view);
	glUniformMatrix4fv(state->terrain_shader.model, 1, GL_FALSE, model);

	// Default LOD is the highest quality.
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

	// The width in vertices of the LOD.
	const u32 lod_tile_length = state->chunk_tile_length / state->lod_settings.details[chunk_lod_detail];
	// Number of indices in a chunk LOD.
	const u32 lod_indices_area = lod_tile_length * lod_tile_length * 6;

	const u64 chunk_offset_in_bytes = chunk->lod_offsets[chunk_lod_detail] * sizeof(QuadIndices);

	// Find the offset of the LOD data we want to use be looping through every LOD level
	// before the one we want and calculating the sum of the total size.
	glDrawElements(GL_TRIANGLES, lod_indices_area, GL_UNSIGNED_INT, (void*)(chunk_offset_in_bytes));

	if (state->wireframe) {
		glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
		V3 grid_colour = { 1.f, 1.f, 1.f };
		glUniform3fv(state->terrain_shader.light_colour, 1, (GLfloat*)&grid_colour);
		glUniform3fv(state->terrain_shader.slope_colour, 1, (GLfloat*)&grid_colour);
		glUniform3fv(state->terrain_shader.grass_colour, 1, (GLfloat*)&grid_colour);
		glUniform3fv(state->terrain_shader.sand_colour, 1, (GLfloat*)&grid_colour);
		glUniform3fv(state->terrain_shader.snow_colour, 1, (GLfloat*)&grid_colour);
		glDrawElements(GL_TRIANGLES, lod_indices_area, GL_UNSIGNED_INT, (void*)(chunk_offset_in_bytes));
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	}
}

static void app_render_lights_and_features(app_state *state)
{	
	// Lights
	glUseProgram(state->simple_shader.program);

	glBindVertexArray(state->simple_vao);

	real32 model[16];
	mat4_identity(model);
	mat4_translate(model, state->light_pos.x, state->light_pos.y, state->light_pos.z);

	glUniformMatrix4fv(state->simple_shader.projection, 1, GL_FALSE, state->cur_cam.frustrum);
	glUniformMatrix4fv(state->simple_shader.view, 1, GL_FALSE, state->cur_cam.view);
	glUniformMatrix4fv(state->simple_shader.model, 1, GL_FALSE, model);
	glDrawArrays(GL_TRIANGLES, 0, 3);

	glUseProgram(state->simple_shader.program);
	glBindVertexArray(state->simple_vao);

	glUniformMatrix4fv(state->simple_shader.projection, 1, GL_FALSE, state->cur_cam.frustrum);
	glUniformMatrix4fv(state->simple_shader.view, 1, GL_FALSE, state->cur_cam.view);
	// End of lights

	// Features
	glDisable(GL_CULL_FACE);

	for (u32 i = 0; i < state->params->tree_count; i++) {
		if (state->trees[i].y <= state->params->tree_min_height || state->trees[i].y > state->params->tree_max_height) {
			continue;
		}

		const u32 scale = 6.f * state->params->scale;

		real32 model[16];
		mat4_identity(model);
		mat4_translate(model, state->trees[i].x, state->trees[i].y + scale / 2, state->trees[i].z);
		mat4_scale(model, 1.f, scale, 1.f);
		glUniformMatrix4fv(state->simple_shader.model, 1, GL_FALSE, model);
		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

		mat4_rotate_y(model, 90.f);
		glUniformMatrix4fv(state->simple_shader.model, 1, GL_FALSE, model);
		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
	}

	glEnable(GL_CULL_FACE);
	// End of features
}


static void generate_trees(app_state* state)
{
	u32 num_vertices = state->chunk_vertices_length;

	for (u32 i = 0; i < state->params->max_trees; i++) {
		real32 x, y, z;
		y = 0;
		u32 attempt = 0;
		while (y < state->params->tree_min_height || y > state->params->tree_max_height) {
			if (attempt++ > 50) {
				break;
			}

			Chunk* chunk;
			const u32 chunk_x = rand() % (state->world_width);
			const u32 chunk_z = rand() % (state->world_width);
			chunk = &state->chunks[chunk_z * state->world_width + chunk_x];

			Vertex* v = &chunk->vertices[(rand() % state->chunk_tile_length) * num_vertices + (rand() % state->chunk_tile_length)];
			x = chunk_x * state->chunk_tile_length + v->pos.x;
			y = v->pos.y;
			z = chunk_z * state->chunk_tile_length + v->pos.z;
		}

		state->trees[i].x = x;
		state->trees[i].y = y;
		state->trees[i].z = z;
	}
}

static void generate_world(app_state* state, app_memory* memory)
{
	for (u32 j = 0; j < state->world_width; j++) {
		for (u32 i = 0; i < state->world_width; i++) {
			state->generation_threads[j * state->world_width + i] = std::thread(app_generate_terrain_chunk, state, memory, &state->chunks[j * state->world_width + i]);
		}
	}

	for (u32 i = 0; i < state->world_area; i++) {
		state->generation_threads[i].join();
	}

	glBindVertexArray(state->terrain_vao);

	for (u32 j = 0; j < state->world_width; j++) {
		for (u32 i = 0; i < state->world_width; i++) {
			u32 index = j * state->world_width + i;

			Chunk* chunk = &state->chunks[index];

			glBindBuffer(GL_ARRAY_BUFFER, chunk->vbo);
			glBufferData(GL_ARRAY_BUFFER, chunk->vertices_count * sizeof Vertex, chunk->vertices, GL_STATIC_DRAW);
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof Vertex, (void*)0);
			glEnableVertexAttribArray(0);

			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, chunk->ebo);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, chunk->lod_index_count * sizeof(QuadIndices), chunk->lods, GL_STATIC_DRAW);
			glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof Vertex, (void*)(3 * sizeof(real32)));
			glEnableVertexAttribArray(1);
		}
	}

	generate_trees(state);
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

static void export_terrain_as_obj(app_state* state)
{
	std::ofstream object_file("terrain.obj", std::ios::out);

	if (object_file.good()) {
		object_file << "mtllib terrain.mtl" << std::endl;
		object_file << "usemtl textured" << std::endl;
		object_file << "o Terrain" << std::endl;

		u32 num_vertices = (state->chunk_vertices_length) * (state->chunk_vertices_length);

		for (u32 chunk_z = 0; chunk_z < state->world_width; chunk_z++) {
			for (u32 chunk_x = 0; chunk_x < state->world_width; chunk_x++) {
				u32 chunk_index = chunk_z * state->world_width + chunk_x;

				object_file << "# Chunk" << chunk_index << " vertices" << std::endl;

				for (u32 vertex = 0; vertex < num_vertices; vertex++) {
					Vertex* current_vertex = &state->chunks[chunk_index].vertices[vertex];
					// Offset chunk vertices by world position.
					real32 x = current_vertex->pos.x + chunk_x * (state->chunk_vertices_length);
					real32 y = current_vertex->pos.y;
					real32 z = current_vertex->pos.z + chunk_z * (state->chunk_vertices_length);

					object_file << "v " << x << " " << y << " " << z;
					object_file << std::endl;
				}
			}
		}

		if (state->export_settings.texture_map) {
			for (u32 chunk_z = 0; chunk_z < state->world_width; chunk_z++) {
				for (u32 chunk_x = 0; chunk_x < state->world_width; chunk_x++) {
					for (u32 vertex_row = 0; vertex_row < state->chunk_vertices_length; vertex_row++) {
						for (u32 vertex_col = 0; vertex_col < state->chunk_vertices_length; vertex_col++) {
							real32 u = (real32)(chunk_z * state->chunk_vertices_length + vertex_row) / (state->world_width * state->chunk_vertices_length);
							real32 v = (real32)(chunk_x * state->chunk_vertices_length + vertex_col) / (state->world_width * state->chunk_vertices_length);

							object_file << "vt " << u << " " << v << std::endl;
						}
					}
				}
			}
		}

		if (state->export_settings.with_normals) {
			for (u32 chunk_z = 0; chunk_z < state->world_width; chunk_z++) {
				for (u32 chunk_x = 0; chunk_x < state->world_width; chunk_x++) {
					u32 chunk_index = chunk_z * state->world_width + chunk_x;

					for (u32 vertex = 0; vertex < num_vertices; vertex++) {
						Vertex* current_vertex = &state->chunks[chunk_index].vertices[vertex];
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

		for (u32 chunk_z = 0; chunk_z < state->world_width; chunk_z++) {
			for (u32 chunk_x = 0; chunk_x < state->world_width; chunk_x++) {
				u32 chunk_index = chunk_z * state->world_width + chunk_x;
				
				Chunk* current_chunk = &state->chunks[chunk_index];

				u32 num_lods_to_export = 1;

				if (state->export_settings.lods) {
					num_lods_to_export = state->lod_settings.details_in_use;
				}

				// Each LOD has a group in that object.
				for (u32 lod_detail_index = 0; lod_detail_index < num_lods_to_export; lod_detail_index++) {
					object_file << "g Chunk" << chunk_index << "LOD" << lod_detail_index << std::endl;

					const u32 lod_detail = state->lod_settings.details[lod_detail_index];
					
					u32 num_indices = (state->chunk_tile_length / lod_detail) * (state->chunk_tile_length / lod_detail);

					for (u32 index = 0; index < num_indices; index++) {
						QuadIndices* current_quad = &current_chunk->lods[current_chunk->lod_offsets[lod_detail_index] + index];
						u64 chunk_vertices_size = chunk_index * num_vertices;

						u32 f0 = current_quad->i[0] + 1 + chunk_vertices_size;
						u32 f1 = current_quad->i[1] + 1 + chunk_vertices_size;
						u32 f2 = current_quad->i[2] + 1 + chunk_vertices_size;
						object_file << face_string_func(f0, f1, f2);

						u32 f3 = current_quad->i[3] + 1 + chunk_vertices_size;
						u32 f4 = current_quad->i[4] + 1 + chunk_vertices_size;
						u32 f5 = current_quad->i[5] + 1 + chunk_vertices_size;
						object_file << face_string_func(f3, f4, f5);
					}
				}
			}
		}
	}

	std::ofstream material_file("terrain.mtl", std::ios::out);

	if (material_file.good()) {
		material_file << "newmtl textured" << std::endl;
		material_file << "Ka 1.000 1.000 1.000" << std::endl;
		material_file << "Kd 1.000 1.000 1.000" << std::endl;
		material_file << "Ks 0.000 0.000 0.000" << std::endl;
		material_file << "d 1.000" << std::endl;
		material_file << "illum 2" << std::endl;

		if (state->export_settings.texture_map) {
			material_file << "map_Ka terrain.tga" << std::endl;
			material_file << "map_Kd terrain.tga" << std::endl;
		}
	}

	material_file.close();

	if (state->export_settings.texture_map) {
		std::ofstream tga_file("terrain.tga", std::ios::binary);
		if (!tga_file) return;

		char header[18] = { 0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };
		header[12] = state->texture_map_data.resolution & 0xFF;
		header[13] = (state->texture_map_data.resolution >> 8) & 0xFF;
		header[14] = (state->texture_map_data.resolution) & 0xFF;
		header[15] = (state->texture_map_data.resolution >> 8) & 0xFF;
		header[16] = 24;

		tga_file.write((char*)header, 18);

		glBindVertexArray(state->terrain_vao);
		glUseProgram(state->terrain_shader.program);

		glBindFramebuffer(GL_FRAMEBUFFER, state->texture_map_data.fbo);

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		real32 no_clip[4] = { 0, -1, 0, 100000 };
		glUniform4fv(state->terrain_shader.plane, 1, no_clip);

		glUniform1f(state->terrain_shader.ambient_strength, state->params->ambient_strength);
		glUniform1f(state->terrain_shader.diffuse_strength, state->params->diffuse_strength);
		glUniform1f(state->terrain_shader.specular_strength, state->params->specular_strength);

		glUniform3fv(state->terrain_shader.light_pos, 1, (GLfloat*)(&state->light_pos));
		glUniform1f(state->terrain_shader.sand_height, state->params->sand_height);
		glUniform1f(state->terrain_shader.snow_height, state->params->snow_height);

		glUniform3fv(state->terrain_shader.light_colour, 1, (GLfloat*)&state->params->light_colour);
		glUniform3fv(state->terrain_shader.slope_colour, 1, (GLfloat*)&state->params->slope_colour);
		glUniform3fv(state->terrain_shader.grass_colour, 1, (GLfloat*)&state->params->grass_colour);
		glUniform3fv(state->terrain_shader.sand_colour, 1, (GLfloat*)&state->params->sand_colour);
		glUniform3fv(state->terrain_shader.snow_colour, 1, (GLfloat*)&state->params->snow_colour);

		glUniform3fv(state->terrain_shader.view_position, 1, (GLfloat*)&state->cur_cam.pos);

		Camera copy_cam = state->cur_cam;
		camera_ortho(&copy_cam, state->world_tile_length, state->world_tile_length);
		copy_cam.pos = { 0, 9000, 0 };
		copy_cam.front = { 0, -1, 0 };
		copy_cam.up = { 1, 0, 0 };
		camera_look_at(&copy_cam); glUniformMatrix4fv(state->terrain_shader.view, 1, GL_FALSE, copy_cam.view);
		glUniformMatrix4fv(state->terrain_shader.projection, 1, GL_FALSE, copy_cam.ortho);
		
		glDisable(GL_DEPTH_TEST);
		glViewport(0, 0, state->texture_map_data.resolution, state->texture_map_data.resolution);

		for (u32 y = 0; y < state->world_width; y++) {
			for (u32 x = 0; x < state->world_width; x++) {
				u32 index = y * state->world_width + x;
				glBindBuffer(GL_ARRAY_BUFFER, state->chunks[index].vbo);
				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, state->chunks[index].ebo);
				glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof Vertex, (void*)0);
				glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof Vertex, (void*)(3 * sizeof(real32)));

				real32 model[16];
				mat4_identity(model);
				mat4_translate(model, x * state->chunk_tile_length, 0, y * state->chunk_tile_length);
				glUniformMatrix4fv(state->terrain_shader.model, 1, GL_FALSE, model);
				
				glDrawElements(GL_TRIANGLES, (state->chunk_tile_length * state->chunk_tile_length * 6), GL_UNSIGNED_INT, (void*)(0));
			}
		}
		
		glViewport(0, 0, state->window_info.w, state->window_info.h);
		glEnable(GL_DEPTH_TEST);

		glReadPixels(0, 0, state->texture_map_data.resolution, state->texture_map_data.resolution, GL_BGR, GL_UNSIGNED_BYTE, state->texture_map_data.pixels);

		tga_file.write((char*)state->texture_map_data.pixels, state->texture_map_data.resolution * state->texture_map_data.resolution * sizeof(RGB));

		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		tga_file.close();
	}

	object_file.close();
}

static void app_render(app_state *state, app_memory *memory)
{
	real32 reflection_clip[4] = { 0, 1, 0, -state->params->water_height };
	real32 refraction_clip[4] = { 0, -1, 0, state->params->water_height }; 
	real32 no_clip[4] = { 0, -1, 0, 100000 };

	Camera camera_backup = state->cur_cam;
	Camera reflection_cam = state->cur_cam;
	real32 distance = 2.f * (state->cur_cam.pos.y - state->params->water_height);
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

	for (u32 i = 0; i < state->chunk_count; i++) {
		app_render_chunk(state, reflection_clip, &state->chunks[i]);
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

	for (u32 i = 0; i < state->chunk_count; i++) {
		app_render_chunk(state, refraction_clip, &state->chunks[i]);
	}

	app_render_lights_and_features(state);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, state->window_info.w, state->window_info.h);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	for (u32 i = 0; i < state->chunk_count; i++) {
		app_render_chunk(state, no_clip, &state->chunks[i]);
	}

	app_render_lights_and_features(state);
	
	glDisable(GL_CLIP_DISTANCE0);

	// Water
	glUseProgram(state->water_shader.program);
	glBindVertexArray(state->simple_vao);

	real32 model[16];
	mat4_identity(model);
	mat4_translate(model, state->world_tile_length / 2, state->params->water_height, state->world_tile_length / 2);
	mat4_scale(model, state->world_tile_length, 1.f, state->world_tile_length);
	mat4_rotate_x(model, 90.f);

	glUniformMatrix4fv(state->water_shader.projection, 1, GL_FALSE, state->cur_cam.frustrum);
	glUniformMatrix4fv(state->water_shader.view, 1, GL_FALSE, state->cur_cam.view);
	glUniformMatrix4fv(state->water_shader.model, 1, GL_FALSE, model);

	glUniform3fv(state->water_shader.water_colour, 1, (GLfloat*)&state->params->water_colour);

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

	window_flags |= ImGuiWindowFlags_NoTitleBar;
	window_flags |= ImGuiWindowFlags_NoScrollbar;
	window_flags |= ImGuiWindowFlags_MenuBar;

	ImGui::SetNextWindowPos(ImVec2(650, 20), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(200, 300), ImGuiCond_FirstUseEver);

	bool p_open = true;
	if (!ImGui::Begin("Settings", &p_open, window_flags)) {
		ImGui::End();
		return;
	}

	bool regenerate_chunks = false;
	bool regenerate_trees = false;

	if (ImGui::TreeNode("Export")) {
		ImGui::Checkbox("Include normals", (bool*)&state->export_settings.with_normals);
		ImGui::Checkbox("Ambient + Diffuse map", (bool*)&state->export_settings.texture_map);

		if (state->export_settings.texture_map) {
			ImGui::SameLine();
			const char* resolutions[] = { "256", "512", "1024", "2048", "4096", "8192", "16384" };
			static int resolution_current = 0;
			if (ImGui::Combo(" ", &resolution_current, resolutions, IM_ARRAYSIZE(resolutions))) {
				state->texture_map_data.resolution = atoi(resolutions[resolution_current]);
				printf("%d", state->texture_map_data.resolution);
				create_terrain_texture_map_texture(&state->texture_map_data);
			}
		}

		ImGui::Checkbox("LODs", (bool*)&state->export_settings.lods);

		if (ImGui::Button("Go!")) {
			export_terrain_as_obj(state);
		}

		ImGui::TreePop();
	}

	if (ImGui::TreeNode("LOD")) {
		ImGui::SliderInt("number of LODs", (int*)&state->lod_settings.details_in_use, 1, state->lod_settings.max_available_count, "%d", ImGuiSliderFlags_None);

		ImGui::TreePop();
	}

	if (ImGui::TreeNode("Presets")) {
		bool custom, plains, desert, mountains;
		custom = plains = desert = mountains = false;

		custom = ImGui::Button("Custom");
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

	if (ImGui::TreeNode("Perlin Noise Parameters")) {
		regenerate_chunks |= ImGui::SliderFloat("scale", &state->params->scale, 0.1, 10.f, "%.2f", ImGuiSliderFlags_None);
		regenerate_chunks |= ImGui::SliderFloat("y scale", &state->params->y_scale, 0.f, 1000.f, "%.2f", ImGuiSliderFlags_None);
		regenerate_chunks |= ImGui::SliderFloat("lacunarity", &state->params->lacunarity, 0.f, 3.f, "%.2f", ImGuiSliderFlags_None);
		regenerate_chunks |= ImGui::SliderFloat("persistence", &state->params->persistence, 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
		regenerate_chunks |= ImGui::SliderFloat("elevation", &state->params->elevation_power, 0.f, 5.f, "%.2f", ImGuiSliderFlags_None);
		regenerate_chunks |= ImGui::SliderInt("octaves", &state->params->max_octaves, 1, 20, "%d", ImGuiSliderFlags_None);
		ImGui::TreePop();
	}

	if (ImGui::TreeNode("Lighting & Colours")) {
		ImGui::SliderFloat("ambient strength", &state->params->ambient_strength, 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
		ImGui::SliderFloat("diffuse strength", &state->params->diffuse_strength, 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
		ImGui::SliderFloat("specular strength", &state->params->specular_strength, 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);

		if (ImGui::TreeNode("Grass")) {
			ImGui::SliderFloat("grass colour red", &state->params->grass_colour.E[0], 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
			ImGui::SliderFloat("grass colour green", &state->params->grass_colour.E[1], 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
			ImGui::SliderFloat("grass colour blue", &state->params->grass_colour.E[2], 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
			ImGui::TreePop();
		}

		if (ImGui::TreeNode("Slope")) {
			ImGui::SliderFloat("slope colour red", &state->params->slope_colour.E[0], 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
			ImGui::SliderFloat("slope colour green", &state->params->slope_colour.E[1], 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
			ImGui::SliderFloat("slope colour blue", &state->params->slope_colour.E[2], 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
			ImGui::TreePop();
		}

		if (ImGui::TreeNode("Water")) {
			ImGui::SliderFloat("water colour red", &state->params->water_colour.E[0], 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
			ImGui::SliderFloat("water colour green", &state->params->water_colour.E[1], 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
			ImGui::SliderFloat("water colour blue", &state->params->water_colour.E[2], 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
			ImGui::TreePop();
		}

		if (ImGui::TreeNode("Sand")) {
			ImGui::SliderFloat("sand colour red", &state->params->sand_colour.E[0], 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
			ImGui::SliderFloat("sand colour green", &state->params->sand_colour.E[1], 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
			ImGui::SliderFloat("sand colour blue", &state->params->sand_colour.E[2], 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
			ImGui::TreePop();
		}

		if (ImGui::TreeNode("Snow")) {
			ImGui::SliderFloat("snow colour red", &state->params->snow_colour.E[0], 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
			ImGui::SliderFloat("snow colour green", &state->params->snow_colour.E[1], 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
			ImGui::SliderFloat("snow colour blue", &state->params->snow_colour.E[2], 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
			ImGui::TreePop();
		}

		if (ImGui::TreeNode("Skybox")) {
			ImGui::SliderFloat("skybox colour red", &state->params->skybox_colour.E[0], 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
			ImGui::SliderFloat("skybox colour green", &state->params->skybox_colour.E[1], 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
			ImGui::SliderFloat("skybox colour blue", &state->params->skybox_colour.E[2], 0.f, 1.f, "%.2f", ImGuiSliderFlags_None);
			ImGui::TreePop();
		}

		ImGui::TreePop();
	}

	if (ImGui::TreeNode("Heightmap")) {
		ImGui::SliderFloat("water height", &state->params->water_height, -50.f, 300.f, "%.2f", ImGuiSliderFlags_None);
		ImGui::SliderFloat("sand height", &state->params->sand_height, 0.f, 300.f, "%.2f", ImGuiSliderFlags_None);
		ImGui::SliderFloat("snow height", &state->params->snow_height, 0.f, 2000.f, "%.2f", ImGuiSliderFlags_None);
		ImGui::TreePop();
	}

	if (ImGui::TreeNode("Features")) {
		if (ImGui::TreeNode("Trees")) {
			ImGui::SliderInt("tree count", (int*)&state->params->tree_count, 0, state->params->max_trees, "%d", ImGuiSliderFlags_None);
			ImGui::SliderInt("tree min height", (int*)&state->params->tree_min_height, 0, 1000, "%d", ImGuiSliderFlags_None);
			ImGui::SliderInt("tree max height", (int*)&state->params->tree_max_height, 0, 1000, "%d", ImGuiSliderFlags_None);
			regenerate_trees |= ImGui::Button("Regenerate");
			ImGui::TreePop();
		}
		ImGui::TreePop();
	}

	ImGui::End();

	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

	if (regenerate_chunks) {
		generate_world(state, memory);
	}

	if (regenerate_trees) {
		generate_trees(state);
	}
	// End of UI
}

void app_init(app_state* state, app_memory* memory)
{
	memory->free_offset = sizeof(app_state);

	init_rng();

	glGenVertexArrays(1, &state->simple_vao);
	glBindVertexArray(state->simple_vao);

	real32 quad_verts[12] = {
		 0.5f,  0.5f, 0.f,
		 0.5f, -0.5f, 0.f,
		-0.5f, -0.5f, 0.f,
		-0.5f,  0.5f, 0.f,
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

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(real32), (void*)0);
	glEnableVertexAttribArray(0);

	app_init_shaders(state);

	state->chunk_tile_length = 256;
	state->chunk_vertices_length = state->chunk_tile_length + 1;
	state->chunk_count = 0;
	state->world_width = 5;
	state->world_area = state->world_width * state->world_width;
	state->world_tile_length = state->chunk_tile_length * state->world_width;

	state->generation_threads = (std::thread*)my_malloc(memory, state->world_area * sizeof(std::thread));

	// Reserve space for upto 10 LOD levels.
	state->lod_settings.max_details_count = 32;
	state->lod_settings.details = (u32*)my_malloc(memory, state->lod_settings.max_details_count * sizeof(u32));

	// LOD detail levels have to be powers of 2 or everything blows up.
	u32 detail = 1;
	for (u32 i = 0; i < state->lod_settings.max_details_count; i++) {
		state->lod_settings.details[i] = detail;
		detail *= 2;
	}

	// Find the most LODs possible for the chunk size.
	for (u32 i = 0; i < state->lod_settings.max_details_count; i++) {
		state->lod_settings.max_available_count = i;

		if (state->lod_settings.details[i] > state->chunk_tile_length) {
			break;
		}
	}

	state->lod_settings.details_in_use = state->lod_settings.max_available_count;
	// end of LOD setting configuration.

	state->light_pos = { ((real32)state->chunk_tile_length / 2) * state->world_width, 5000.f, ((real32)state->chunk_tile_length / 2) * state->world_width };

	init_water_data(state);
	
	init_terrain_texture_map_data(state, memory);

	// Check we have enough space to vertices and LODS.
	u64 world_vertex_size = state->world_area * (state->chunk_vertices_length) * (state->chunk_vertices_length) * sizeof Vertex;
	u64 world_indices_size = 0;
	for (u32 i = 0; i < state->lod_settings.max_available_count; i++) {
		world_indices_size += state->world_area * (state->chunk_tile_length / state->lod_settings.details[i]) * (state->chunk_tile_length / state->lod_settings.details[i]) * sizeof QuadIndices;
	}

	// We need a big 'ol grid with an area equal to the maximum amount of vertices in the world.
	// This mesh will be sampled and used to generate the optmized chunk meshes.
	state->perlin_noise_vertices = (Vertex*)my_malloc(memory, world_vertex_size);
	state->perlin_noise_mesh = (QuadIndices*)my_malloc(memory, world_indices_size);

	glGenVertexArrays(1, &state->terrain_vao);

	state->chunks = (Chunk*)my_malloc(memory, state->world_area * sizeof(Chunk));

	for (u32 j = 0; j < state->world_width; j++) {
		for (u32 i = 0; i < state->world_width; i++) {
			u32 index = j * state->world_width + i;
			glGenBuffers(1, &state->chunks[index].vbo);
			glGenBuffers(1, &state->chunks[index].ebo);
			state->chunks[index].x = i;
			state->chunks[index].y = j;

			u64 chunk_vertices_size = (state->chunk_vertices_length) * (state->chunk_vertices_length) * sizeof(Vertex);

			u64 chunk_lods_size = 0;
			for (u32 lod_detail_index = 0; lod_detail_index < state->lod_settings.max_available_count; lod_detail_index++) {
				const u32 detail = state->lod_settings.details[lod_detail_index];
				chunk_lods_size += (state->chunk_tile_length / detail) * (state->chunk_tile_length / detail) * sizeof(QuadIndices);
			}

			state->chunks[index].vertices_count = (state->chunk_vertices_length) * (state->chunk_vertices_length);
			state->chunks[index].lod_index_count = 0;
			state->chunks[index].vertices = (Vertex*)(my_malloc(memory, chunk_vertices_size));
			state->chunks[index].lods = (QuadIndices*)(my_malloc(memory, chunk_lods_size));
			state->chunks[index].lod_offsets = (u64*)my_malloc(memory, state->lod_settings.max_available_count * sizeof(u64));

			state->chunk_count++;
		}
	}

	state->custom_parameters.scale = state->chunk_tile_length / 100.f; // Makes the scale reasonable for most world sizes
	state->custom_parameters.lacunarity = 1.6f;
	state->custom_parameters.persistence = 0.45f;
	state->custom_parameters.elevation_power = 3.f;
	state->custom_parameters.y_scale = 75.f;
	state->custom_parameters.max_octaves = 16;
	state->custom_parameters.water_height = 3.f * state->custom_parameters.scale;
	state->custom_parameters.sand_height = 4.f * state->custom_parameters.scale;
	state->custom_parameters.snow_height = 200.f * state->custom_parameters.scale;
	state->custom_parameters.ambient_strength = 1.f;
	state->custom_parameters.diffuse_strength = 0.3f;
	state->custom_parameters.specular_strength = 0.75f;
	state->custom_parameters.light_colour = { 1.f, 0.8f, 0.7f };
	state->custom_parameters.grass_colour = { 0.15f, 0.23f, 0.13f };
	state->custom_parameters.sand_colour = { 0.8f, 0.81f, 0.55f };
	state->custom_parameters.snow_colour = { 0.8f, 0.8f, 0.8f };
	state->custom_parameters.slope_colour = { 0.7f, 0.67f, 0.56f };
	state->custom_parameters.water_colour = { .2f, .2f, 0.4f };
	state->custom_parameters.skybox_colour = { 0.65f, 0.65f, 1.f };
	state->custom_parameters.tree_count = 0;
	state->custom_parameters.tree_min_height = state->custom_parameters.sand_height;
	state->custom_parameters.tree_max_height = state->custom_parameters.snow_height;
	state->custom_parameters.max_trees = 10000;

	CopyMemory(&state->green_plains_parameters, &state->custom_parameters, sizeof(world_generation_parameters));
	state->custom_parameters.elevation_power = 2.5f;
	state->green_plains_parameters.y_scale = 20.;
	state->green_plains_parameters.lacunarity = 1.6f;

	CopyMemory(&state->rugged_desert_parameters, &state->custom_parameters, sizeof(world_generation_parameters));
	state->rugged_desert_parameters.persistence = 0.55f;
	state->rugged_desert_parameters.lacunarity = 1.7f;
	state->rugged_desert_parameters.y_scale = 50.f;
	state->rugged_desert_parameters.sand_height = 500;
	state->rugged_desert_parameters.water_height = 0;

	CopyMemory(&state->harsh_mountains_parameters, &state->custom_parameters, sizeof(world_generation_parameters));
	state->harsh_mountains_parameters.y_scale = 140.f;
	state->harsh_mountains_parameters.lacunarity = 1.7f;
	state->harsh_mountains_parameters.persistence = 0.51f;
	state->harsh_mountains_parameters.elevation_power = 4.f;

	state->params = &state->harsh_mountains_parameters;

	state->trees = (V3*)my_malloc(memory, state->params->max_trees * sizeof V3);

	generate_world(state, memory);

	camera_init(&state->cur_cam);
	state->cur_cam.pos = { 0, 200.f, 0 };
	state->cur_cam.front = { 0.601920426f, -0.556864262f, 0.572358370f };
	state->cur_cam.yaw = 43.5579033f;
	state->cur_cam.pitch = -33.8392143;

	glViewport(0, 0, state->window_info.w, state->window_info.h);
	camera_frustrum(&state->cur_cam, state->window_info.w, state->window_info.h);
	camera_ortho(&state->cur_cam, state->window_info.w, state->window_info.h);

	state->export_settings = {};

	state->wireframe = false;
	state->flying = true;
}

void app_handle_input(real32 dt, app_state* state, app_memory *memory, app_keyboard_input* keyboard)
{
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
		generate_world(state, memory);
	}

	if (keyboard->fly.toggled) {
		state->flying = !state->flying;
	}

	// Out of bounds check
	if (state->cur_cam.pos.x >= state->world_tile_length) {
		state->cur_cam.pos.x = state->world_tile_length - 1;
	} else if (state->cur_cam.pos.x < 0) {
		state->cur_cam.pos.x = 0;
	}

	if (state->cur_cam.pos.z >= state->world_tile_length) {
		state->cur_cam.pos.z = state->world_tile_length - 1;
	} else if (state->cur_cam.pos.z < 0) {
		state->cur_cam.pos.z = 0;
	}
}

void app_update(app_state *state)
{
	// Keep track of current chunk for LODs and collision.
	const real32 num_vertices = state->chunk_vertices_length;
	const u32 cam_chunk_x = (u32)(state->cur_cam.pos.x / state->chunk_tile_length);
	const u32 cam_chunk_z = (u32)(state->cur_cam.pos.z / state->chunk_tile_length);

	state->current_chunk = &state->chunks[cam_chunk_z * state->world_width + cam_chunk_x];

	if (!state->flying) {
		state->cur_cam.vel = 5.f;

		real32 cam_pos_x_relative = state->cur_cam.pos.x - cam_chunk_x * state->chunk_tile_length;
		real32 cam_pos_z_relative = state->cur_cam.pos.z - cam_chunk_z * state->chunk_tile_length;

		const u32 i0 = (u32)cam_pos_z_relative * num_vertices + (u32)cam_pos_x_relative;
		const u32 i1 = i0 + 1;
		const u32 i2 = i0 + num_vertices;
		const u32 i3 = i2 + 1;

		V3 p0 = state->current_chunk->vertices[i0].pos;
		V3 p1 = state->current_chunk->vertices[i1].pos;
		V3 p2 = state->current_chunk->vertices[i2].pos;
		V3 p3 = state->current_chunk->vertices[i3].pos;

		V3 n = v3_cross(p1 - p0, p2 - p1);
		n = v3_normalise(n);

		real32 d = v3_dot(n, p0);
		const real32 y = (d - (n.x * cam_pos_x_relative) - (n.z * cam_pos_z_relative)) / n.y;

		state->cur_cam.pos.y = y + 1.5f;
	} else {
		state->cur_cam.vel = 300.f;
	}
}

void app_update_and_render(real32 dt, app_input *input, app_memory *memory, app_window_info *window_info)
{
	app_state *state = (app_state*)memory->permenant_storage;

	state->window_info = *window_info;

	if (!window_info->running) {
		app_on_destroy(state);
		return;
	}

	if (!memory->is_initialized) {
		app_init(state, memory);
		memory->is_initialized = true;
	}

	if (window_info->resize) {
		glViewport(0, 0, window_info->w, window_info->h);
		camera_frustrum(&state->cur_cam, window_info->w, window_info->h);
	}

    app_keyboard_input *keyboard = &input->keyboard;
	app_handle_input(dt, state, memory, keyboard);
	app_update(state);
	app_render(state, memory);
}