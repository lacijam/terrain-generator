#ifndef OPENGL_UTIL_H
#define OPENGL_UTIL_H

#include "types.h"

extern bool gl_check_shader_compile_log(u32 shader);
extern bool gl_check_program_link_log(u32 program);
extern u32 gl_compile_shader_from_source(const char *source, u32 program, s32 type);

#endif