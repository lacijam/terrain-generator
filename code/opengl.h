#pragma once

#include <gl/GL.h>
#include "gl/glext.h"
#include "gl/wglext.h"

void gl_load_extensions();
bool wgl_is_supported(const char*);
bool gl_check_shader_compile_log(unsigned shader);
bool gl_check_program_link_log(unsigned program);
//bool gl_create_and_attach_shader(unsigned program_id, const char* src, GLenum shader_type);
void APIENTRY gl_message_callback(GLenum, GLenum, GLuint, GLenum, GLsizei, const GLchar*, const void*);
unsigned gl_load_shader_from_file(const char *filename, unsigned program, int type);

#define GLF(name, uppername) extern PFNGL##uppername##PROC gl##name
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
#undef GL_FUNCS
#undef GLF
#define WGLF(name, uppername) extern PFNWGL##uppername##PROC wgl##name
#define WGL_FUNCS \
WGLF(GetExtensionsStringARB, GETEXTENSIONSSTRINGARB);\
WGLF(ChoosePixelFormatARB, CHOOSEPIXELFORMATARB);\
WGLF(CreateContextAttribsARB, CREATECONTEXTATTRIBSARB);\
WGLF(SwapIntervalEXT, SWAPINTERVALEXT);
WGL_FUNCS
#undef WGL_FUNCS
#undef WGLF