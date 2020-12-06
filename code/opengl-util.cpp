#include <Windows.h>
#include <stdio.h>

#include "opengl-util.h"


bool gl_check_shader_compile_log(unsigned shader)
{
	int success;
	char info_log[512];
	glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glGetShaderInfoLog(shader, 512, 0, info_log);
		printf(">>>Shader compilation error: %s", info_log);
	};

	return success;
}

bool gl_check_program_link_log(unsigned program)
{
	int success;
	char info_log[512];
	glGetProgramiv(program, GL_LINK_STATUS, &success);
	if (!success)
	{
		glGetProgramInfoLog(program, 512, NULL, info_log);
		printf(">>>Shader program linking error: %s", info_log);
	}

	return success;
}

bool gl_create_and_attach_shader(unsigned program_id, const char* src, GLenum shader_type)
{
    unsigned shader = glCreateShader(shader_type);
	if (!shader) {
		return false;
	}
	
    glShaderSource(shader, 1, &src, 0);
    glCompileShader(shader);
    glAttachShader(program_id, shader);
    if (!gl_check_shader_compile_log(shader)) {
        return false;
    }

    return true;
}

void APIENTRY  
gl_message_callback(GLenum source,
                 GLenum type,
                 GLuint id,
                 GLenum severity,
                 GLsizei length,
                 const GLchar* message,
                 const void* userParam)
{
	fprintf(stderr, ">>>GLCALLBACK: %s id=%u %s\n",
           (type == GL_DEBUG_TYPE_ERROR ? "ERROR" : "" ),
            id, message);
}