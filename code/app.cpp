#include "app.h"

struct Mesh {
    real32 *vertices;
    real32 *indices;
};

static void app_generate_terrain_mesh(game_state *state)
{
	struct Vertex {
		V3 pos;
		V3 nor;
	};

	Vertex vertices[state->chunk_height * state->chunk_width];
	for (int j = 0; j < state->chunk_height; j++) {
		for (int i = 0; i < state->chunk_width; i++) {
			unsigned index = j * state->chunk_width + i;
			vertices[index].pos.x = i;
			vertices[index].pos.y = (rand() % 1000) / 1000.f;
			vertices[index].pos.z = j;
			vertices[index].nor = {};
		}
	}

	struct QuadIndices {
		unsigned i[6];
	};

	const int row_quads = state->chunk_height - 1;
	const int col_quads = state->chunk_width - 1;
	QuadIndices indices[row_quads * col_quads];
	for (int j = 0; j < row_quads; j++) {
		for (int i = 0; i < col_quads; i++) {
			unsigned pos = j * col_quads + i;
			indices[pos].i[0] = j * state->chunk_width + i;
			indices[pos].i[1] = j * state->chunk_width + i + 1;
			indices[pos].i[2] = (j + 1) * state->chunk_width + i;
			
			indices[pos].i[3] = j * state->chunk_width + i + 1;
			indices[pos].i[4] = (j + 1) * state->chunk_width + i + 1;
			indices[pos].i[5] = (j + 1) * state->chunk_width + i;
		}
	}

	for (int j = 0; j < row_quads; j++) {
		for (int i = 0; i < col_quads; i++) {
			unsigned pos = j * col_quads + i;
			for (int tri = 0; tri < 2; tri++) {
				Vertex *a = &vertices[indices[pos].i[tri * 3 + 0]];
				Vertex *b = &vertices[indices[pos].i[tri * 3 + 1]];
				Vertex *c = &vertices[indices[pos].i[tri * 3 + 2]];
				V3 cp = Matrix::cross(b->pos - a->pos, c->pos - a->pos);
				cp = cp * -1.f;
				a->nor += cp;
				b->nor += cp;
				c->nor += cp;
			}
		}
	}

	for (int j = 0; j < state->chunk_height; j++) {
		for (int i = 0; i < state->chunk_width; i++) {
			unsigned index = j * state->chunk_width + i;
			vertices[index].nor = Matrix::normalise(vertices[index].nor);
		}
	}

	glGenVertexArrays(1, &state->vao);
	glBindVertexArray(state->vao);

	glGenBuffers(1, &state->vbo);
	glBindBuffer(GL_ARRAY_BUFFER, state->vbo);
	glBufferData(GL_ARRAY_BUFFER, state->chunk_height * state->chunk_width * 6 * sizeof(float), vertices, GL_STATIC_DRAW);

	glGenBuffers(1, &state->ebo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, state->ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, row_quads * col_quads * 6 * sizeof(unsigned), indices, GL_STATIC_DRAW);

	glVertexAttribPointer(state->a_pos, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(state->a_pos);

	glVertexAttribPointer(state->a_nor, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(state->a_nor);

	glBindVertexArray(0);

	// temp light vao.
	glGenVertexArrays(1, &state->lvao);
	glBindVertexArray(state->lvao);

	float light_verts[9] = {
		-0.5f, -0.5f, 0.0,
		 0.5f, -0.5f, 0.0,
		 0.0f,  0.5f, 0.0
	};

	glGenBuffers(1, &state->lvbo);
	glBindBuffer(GL_ARRAY_BUFFER, state->lvbo);
	glBufferData(GL_ARRAY_BUFFER, 9 * sizeof(float), light_verts, GL_STATIC_DRAW);

	glVertexAttribPointer(state->a_lpos, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(state->a_lpos);
}

void app_init_shaders(game_state *state)
{
    unsigned default_vertex_shader;
	unsigned default_fragment_shader;

	state->program = glCreateProgram();
	default_vertex_shader = gl_load_shader_from_file("default_vertex_shader.gl", state->program, GL_VERTEX_SHADER);
	default_fragment_shader = gl_load_shader_from_file("default_fragment_shader.gl", state->program, GL_FRAGMENT_SHADER);

	glAttachShader(state->program, default_vertex_shader);
	glAttachShader(state->program, default_fragment_shader);
	glLinkProgram(state->program);
    glUseProgram(state->program);
    if (!gl_check_program_link_log(state->program)) {
		// Error!
	}

	state->a_pos = glGetAttribLocation(state->program, "a_pos");
	state->a_nor = glGetAttribLocation(state->program, "a_nor");

	state->transform_loc = glGetUniformLocation(state->program, "transform");
	state->object_colour_loc = glGetUniformLocation(state->program, "object_colour");
	state->model_loc = glGetUniformLocation(state->program, "model");
	state->light_pos_loc = glGetUniformLocation(state->program, "light_pos");

	state->lighting_program = glCreateProgram();
	unsigned light_vertex_shader = gl_load_shader_from_file("light_vertex_shader.gl", state->lighting_program, GL_VERTEX_SHADER);
	unsigned light_fragment_shader = gl_load_shader_from_file("light_fragment_shader.gl", state->lighting_program, GL_FRAGMENT_SHADER);
	glAttachShader(state->lighting_program, light_vertex_shader);
	glAttachShader(state->lighting_program, light_fragment_shader);
	glLinkProgram(state->lighting_program);
    glUseProgram(state->lighting_program);
    if (!gl_check_program_link_log(state->lighting_program)) {
		// Error
	}

	state->a_lpos = glGetAttribLocation(state->lighting_program, "a_pos");

	glDeleteShader(default_vertex_shader);
	glDeleteShader(light_vertex_shader);
	glDeleteShader(light_fragment_shader);
	glDeleteShader(default_fragment_shader);
}

static void app_on_destroy(game_state *state)
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


static void render_terrain(game_state *state)
{
	//temp
	static real32 i = 0;
	static real32 dir = 1;
    
	if (i > 100.f || i < 0.f) {
		dir *= -1;
	}

	i += 1.f * dir;
	//

    V3 light_pos = { i, 10.0f, 0.0f };

    glClearColor(0.2f, 0.2f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glUseProgram(state->program);
	glBindVertexArray(state->vao);

    float terrain_colour[3] = { 0.1f, 1.0f, 0.1f };

    glUniform3fv(state->object_colour_loc, 1, terrain_colour);
    glUniform3fv(state->light_pos_loc, 1, (GLfloat*)(&light_pos));

    for (int j = 0; j < 5; j++) {
        for (int i = 0; i < 5; i++) {
            float model[16];
            Matrix::identity(model);
            Matrix::translate(model, i * (state->chunk_width - 1), 0.f, j * (state->chunk_height -1));
            Matrix::scale(model, 1.f, 1.f, 1.f);

            glUniformMatrix4fv(state->model_loc, 1, GL_FALSE, model);

            float mv[16], mvp[16];
            Matrix::identity(mv);
            Matrix::identity(mvp);
            Matrix::multiply(mv, state->cur_cam.view, model);
            Matrix::multiply(mvp, state->cur_cam.frustrum, mv);
            glUniformMatrix4fv(state->transform_loc, 1, GL_FALSE, mvp);

            glDrawElements(GL_TRIANGLES, (state->chunk_width - 1) * (state->chunk_height - 1) * 6, GL_UNSIGNED_INT, NULL);
        }
    }


	// Temp drawing light.
    glUseProgram(state->lighting_program);

	glBindVertexArray(state->lvao);

	float model[16];
	Matrix::identity(model);
	Matrix::translate(model, light_pos.x, light_pos.y, light_pos.z);
	Matrix::scale(model, 10.f, 10.f, 10.f);

	float mv[16], mvp[16];
	Matrix::identity(mv);
	Matrix::identity(mvp);
	Matrix::multiply(mv, state->cur_cam.view, model);
	Matrix::multiply(mvp, state->cur_cam.frustrum, mv);
	glUniformMatrix4fv(glGetUniformLocation(state->lighting_program, "transform"), 1, GL_FALSE, mvp);
	glDrawArrays(GL_TRIANGLES, 0, 3);
}

static void app_update_and_render(float dt, game_input *input, game_memory *memory, game_window_info *window_info)
{
	game_state *state = (game_state*)memory->permenant_storage;
	if (!memory->is_initialized) {
		app_init_shaders(state);
		app_generate_terrain_mesh(state);
		camera_init(&state->cur_cam);
		state->cur_cam.pos = { -20.f, 50.f, -20.f};
		state->cur_cam.front = { 1.f, 0.f, 1.f};
		memory->is_initialized = true;
	}

	if (window_info->resize) {
		glViewport(0, 0, window_info->w, window_info->h);
		camera_frustrum(&state->cur_cam, window_info->w, window_info->h);
	}

	float cam_look_speed = 100.f;

    game_keyboard_input *keyboard = &input->keyboard;
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
        state->cur_cam.pitch += cam_look_speed * dt;
    } else if (keyboard->cam_down.ended_down) {
        state->cur_cam.pitch -= cam_look_speed * dt;
    }

    if (keyboard->cam_left.ended_down) {
        state->cur_cam.yaw -= cam_look_speed * dt;
    } else if (keyboard->cam_right.ended_down) {
        state->cur_cam.yaw += cam_look_speed * dt;
    }

    camera_update(&state->cur_cam);

    camera_look_at(&state->cur_cam);
    
    render_terrain(state);
}