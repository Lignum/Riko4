#define _CRT_SECURE_NO_WARNINGS

#include "SDL2/SDL.h"
#include "SDL_gpu/SDL_gpu.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

extern GPU_Target *renderer;
extern bool shaderOn;

// Loads a shader and prepends version/compatibility info before compiling it.
// Normally, you can just use GPU_LoadShader() for shader source files or GPU_CompileShader() for strings.
// However, some hardware (certain ATI/AMD cards) does not let you put non-#version preprocessing at the top of the file.
// Therefore, I need to prepend the version info here so I can support both GLSL and GLSLES with one shader file.
Uint32 load_shader(GPU_ShaderEnum shader_type, const char* filename) {
    SDL_RWops* rwops;
    Uint32 shader;
    char* source;
    int header_size, file_size;
    const char* header = "";
    GPU_Renderer* renderer = GPU_GetCurrentRenderer();

    // Open file
    rwops = SDL_RWFromFile(filename, "rb");
    if (rwops == NULL)
    {
        GPU_PushErrorCode("load_shader", GPU_ERROR_FILE_NOT_FOUND, "Shader file \"%s\" not found", filename);
        return 0;
    }

    // Get file size
    file_size = SDL_RWseek(rwops, 0, SEEK_END);
    SDL_RWseek(rwops, 0, SEEK_SET);

    // Get size from header
    if (renderer->shader_language == GPU_LANGUAGE_GLSL)
    {
        if (renderer->max_shader_version >= 120)
            header = "#version 120\n";
        else
            header = "#version 110\n";  // Maybe this is good enough?
    }
    else if (renderer->shader_language == GPU_LANGUAGE_GLSLES)
        header = "#version 100\nprecision mediump int;\nprecision mediump float;\n";

    header_size = strlen(header);

    // Allocate source buffer
    source = (char*)malloc(sizeof(char)*(header_size + file_size + 1));

    // Prepend header
    strcpy(source, header);

    // Read in source code
    SDL_RWread(rwops, source + strlen(source), 1, file_size);
    source[header_size + file_size] = '\0';

    // Compile the shader
    shader = GPU_CompileShader(shader_type, source);

    // Clean up
    free(source);
    SDL_RWclose(rwops);

    return shader;
}

GPU_ShaderBlock load_shader_program(Uint32* p, const char* vertex_shader_file, const char* fragment_shader_file) {
    Uint32 v, f;
    v = load_shader(GPU_VERTEX_SHADER, vertex_shader_file);

    if (!v)
        GPU_LogError("Failed to load vertex shader (%s): %s\n", vertex_shader_file, GPU_GetShaderMessage());

    f = load_shader(GPU_FRAGMENT_SHADER, fragment_shader_file);

    if (!f)
        GPU_LogError("Failed to load fragment shader (%s): %s\n", fragment_shader_file, GPU_GetShaderMessage());

    *p = GPU_LinkShaders(v, f);

    if (!*p)
    {
        GPU_ShaderBlock b = { -1, -1, -1, -1 };
        GPU_LogError("Failed to link shader program (%s + %s): %s\n", vertex_shader_file, fragment_shader_file, GPU_GetShaderMessage());
        return b;
    }

    GPU_ShaderBlock block = GPU_LoadShaderBlock(*p, "gpu_Vertex", "gpu_TexCoord", "gpu_Color", "gpu_ModelViewProjectionMatrix");
    GPU_ActivateShaderProgram(*p, &block);

    return block;

}

void free_shader(Uint32 p) {
    GPU_FreeShaderProgram(p);
}

Uint32 screen_shader;
GPU_ShaderBlock screen_block;
void initShader() {
    screen_block = load_shader_program(&screen_shader, "data/shaders/common.vert", "data/shaders/common.frag");
    float res[] = { renderer->base_w, renderer->base_h };
    GPU_SetUniformfv(GPU_GetUniformLocation(screen_shader, "resolution"), 2, 1, &res[0]);
    GPU_SetUniformi(GPU_GetUniformLocation(screen_shader, "crteffect"), shaderOn);
}

void updateShader() {
    GPU_DeactivateShaderProgram();
    GPU_ActivateShaderProgram(screen_shader, &screen_block);
}
