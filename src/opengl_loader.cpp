#include <Windows.h>
#include "opengl_loader.h"

#include <ctype.h>
#include <string.h>
#include <stdio.h>

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

void load_gl_extensions()
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