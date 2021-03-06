#ifndef WIN32_OPENGL_H
#define WIN32_OPENGL_H

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <gl/GL.h>
#include "gl/glext.h"
#include "gl/wglext.h"

#define GLF(name, uppername) extern PFNGL##uppername##PROC gl##name
#define GL_FUNCS \
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
GLF(Uniform4fv, UNIFORM4FV);\
GLF(DebugMessageCallback, DEBUGMESSAGECALLBACK);\
GLF(Uniform1i, UNIFORM1I);\
GLF(Uniform1f, UNIFORM1F);\
GLF(BindSampler, BINDSAMPLER);\
GLF(ActiveTexture, ACTIVETEXTURE);\
GLF(DrawElementsBaseVertex, DRAWELEMENTSBASEVERTEX);\
GLF(BlendEquation, BLENDEQUATION);\
GLF(BlendEquationSeparate, BLENDEQUATIONSEPARATE);\
GLF(BlendFuncSeparate, BLENDFUNCSEPARATE);\
GLF(GenRenderbuffers, GENRENDERBUFFERS);\
GLF(FramebufferTexture, FRAMEBUFFERTEXTURE);\
GLF(DeleteFramebuffers, DELETEFRAMEBUFFERS);\
GLF(BindRenderbuffer, BINDRENDERBUFFER);\
GLF(BindFramebuffer, BINDFRAMEBUFFER);\
GLF(GenFramebuffers, GENFRAMEBUFFERS);\
GLF(RenderbufferStorage, RENDERBUFFERSTORAGE);\
GLF(FramebufferRenderbuffer, FRAMEBUFFERRENDERBUFFER);\
GLF(BufferSubData, BUFFERSUBDATA);\
GLF(FramebufferTexture2D, FRAMEBUFFERTEXTURE2D);
GL_FUNCS
#undef GLF

// wgl extensions.
#define WGLF(name, uppername) extern PFNWGL##uppername##PROC wgl##name
#define WGL_FUNCS \
WGLF(GetExtensionsStringARB, GETEXTENSIONSSTRINGARB);\
WGLF(ChoosePixelFormatARB, CHOOSEPIXELFORMATARB);\
WGLF(CreateContextAttribsARB, CREATECONTEXTATTRIBSARB);\
WGLF(SwapIntervalEXT, SWAPINTERVALEXT);
WGL_FUNCS
#undef WGLF

extern void win32_init_opengl_extensions();
extern HGLRC win32_create_gl_context(HWND hwnd);

#endif