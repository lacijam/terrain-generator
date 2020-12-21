#include "opengl-util.h"

#include <stdio.h>
#include <stdlib.h>

#include "win32-opengl.h"

bool gl_check_shader_compile_log(u32 shader)
{
	s32 success;
	char info_log[512];
	glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glGetShaderInfoLog(shader, 512, 0, info_log);
		printf(">>>Shader compilation error: %s", info_log);
	};

	return success;
}

bool gl_check_program_link_log(u32 program)
{
	s32 success;
	char info_log[512];
	glGetProgramiv(program, GL_LINK_STATUS, &success);
	if (!success)
	{
		glGetProgramInfoLog(program, 512, NULL, info_log);
		printf(">>>Shader program linking error: %s", info_log);
	}

	return success;
}

u32 gl_compile_shader_from_source(const char *source, u32 program, s32 type)
{
    u32 shader = glCreateShader(type);
	if (!shader) {
        printf("Failed to create shader!\n");
		return 0;
	}
	
    glShaderSource(shader, 1, &source, 0);
    glCompileShader(shader);
    gl_check_shader_compile_log(shader);

    return shader;
}