#include "opengl.h"

static bool gl_check_shader_compile_log(unsigned shader)
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

static bool gl_check_program_link_log(unsigned program)
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

static void APIENTRY  
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

static unsigned gl_load_shader_from_file(const char *filename, unsigned program, int type)
{
	FILE* file = NULL;
    fopen_s(&file, filename, "rb");
    if (!file) {
        printf("Something went wrong loading shader '%s'", filename);
        return false;
    }

    int len = _filelength(_fileno(file)) + 1;
    char* data = (char*)malloc(len);
    int read = 0;
    int pos = 0;

    do {
        read = fread(data + pos, 1, len - pos, file);
        if (read > 0) {
            pos += read;
        }
    } while (read > 0 && pos != len);

    //@NOTE: Is this needed?
    data[len - 1] = '\0';

    fclose(file);

    unsigned shader = glCreateShader(type);
	if (!shader) {
        printf("Failed to create shader!\n");
		return 0;
	}
	
    glShaderSource(shader, 1, &data, 0);
    glCompileShader(shader);
    gl_check_shader_compile_log(shader);

    free(data);

    return shader;
}