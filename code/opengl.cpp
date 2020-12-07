#include <Windows.h>
#include "opengl.h"

#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <io.h>

#define GLF(name, uppername) PFNGL##uppername##PROC gl##name
#define GL_FUNCS \
GLF(GetStringi, GETSTRINGI);\
GLF(CreateProgram, CREATEPROGRAM);\
GLF(CreateShader, CREATESHADER);\
GLF(GenBuffers, GENBUFFERS);\
GLF(GenVertexArrays, GENVERTEXARRAYS);\
GLF(BindBuffer, BINDBUFFER);\
GLF(BindVertexArray, BINDVERTEXARRAY);\
GLF(BufferData, BUFFERDATA);\
GLF(VertexAttribPointer, VERTEXATTRIBPOINTER);\
GLF(EnableVertexAttribArray, ENABLEVERTEXATTRIBARRAY);\
GLF(CompileShader, COMPILESHADER);\
GLF(ShaderSource, SHADERSOURCE);\
GLF(AttachShader, ATTACHSHADER);\
GLF(DetachShader, DETACHSHADER);\
GLF(DeleteShader, DELETESHADER);\
GLF(DeleteVertexArrays, DELETEVERTEXARRAYS);\
GLF(DeleteBuffers, DELETEBUFFERS);\
GLF(DeleteProgram, DELETEPROGRAM);\
GLF(LinkProgram, LINKPROGRAM);\
GLF(UseProgram, USEPROGRAM);\
GLF(GetShaderiv, GETSHADERIV);\
GLF(GetShaderInfoLog, GETSHADERINFOLOG);\
GLF(GetProgramiv, GETPROGRAMIV);\
GLF(GetProgramInfoLog, GETPROGRAMINFOLOG);\
GLF(GetAttribLocation, GETATTRIBLOCATION);\
GLF(GetUniformLocation, GETUNIFORMLOCATION);\
GLF(UniformMatrix4fv, UNIFORMMATRIX4FV);\
GLF(Uniform3fv, UNIFORM3FV);\
GLF(DebugMessageCallback, DEBUGMESSAGECALLBACK);
GL_FUNCS
#undef GLF
#define WGLF(name, uppername) PFNWGL##uppername##PROC wgl##name
#define WGL_FUNCS \
WGLF(GetExtensionsStringARB, GETEXTENSIONSSTRINGARB);\
WGLF(ChoosePixelFormatARB, CHOOSEPIXELFORMATARB);\
WGLF(CreateContextAttribsARB, CREATECONTEXTATTRIBSARB);\
WGLF(SwapIntervalEXT, SWAPINTERVALEXT);
WGL_FUNCS
#undef WGLF

static void *GetAnyGLFuncAddress(const char *name)
{
    void *p = (void *)wglGetProcAddress(name);
    if (p == 0 ||
        (p == (void *)0x1) || (p == (void *)0x2) || (p == (void *)0x3) ||
        (p == (void *)-1) )
    {
        HMODULE module = LoadLibraryA("opengl32.dll");
        p = (void *)GetProcAddress(module, name);
    }

    return p;
}

void gl_load_extensions()
{
    #define GLF(name, uppername) gl##name = (PFNGL##uppername##PROC)GetAnyGLFuncAddress("gl"#name"")
    GL_FUNCS
    #undef GLF

    #define WGLF(name, uppername) wgl##name = (PFNWGL##uppername##PROC)GetAnyGLFuncAddress("wgl"#name"")
    WGL_FUNCS
    #undef WGLF
}

bool wgl_is_supported(const char *str)
{
    char *wext_str = _strdup(wglGetExtensionsStringARB(wglGetCurrentDC()));
    char *next = NULL;
    char *wext = strtok_s(wext_str, " ", &next);
    bool found = false;
    
    while (wext != NULL && !found) {
        wext = strtok_s(NULL, " ", &next);
        if (strcmp(wext, str)) {
            found = true;
        }
    }

    free(wext_str);

    return found;
}

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

// bool gl_compile_shader(unsigned program_id, const char* src, GLenum shader_type)
// {
//     unsigned shader = glCreateShader(shader_type);
// 	if (!shader) {
// 		return false;
// 	}
	
//     glShaderSource(shader, 1, &src, 0);
//     glCompileShader(shader);
//     glAttachShader(program_id, shader);
//     if (!gl_check_shader_compile_log(shader)) {
//         return false;
//     }

//     return true;
// }

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

unsigned gl_load_shader_from_file(const char *filename, unsigned program, int type)
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