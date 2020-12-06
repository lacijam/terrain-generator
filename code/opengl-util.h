#pragma once

#include "opengl.h"

bool gl_check_shader_compile_log(unsigned shader);
bool gl_check_program_link_log(unsigned program);
bool gl_create_and_attach_shader(unsigned program_id, const char* src, GLenum shader_type);
void APIENTRY gl_message_callback(GLenum, GLenum, GLuint, GLenum, GLsizei, const GLchar*, const void*);