#ifndef SHADERS_H
#define SHADERS_H

namespace Shaders {
    const char *const DEFAULT_VERTEX_SHADER_SOURCE = R"(
    #version 330

    layout (location = 0) in vec3 a_pos;
    layout (location = 1) in vec3 a_nor;
    out vec3 v_pos;
    out vec3 v_nor;
    out vec3 v_light_pos;

    uniform mat4 model;
    uniform mat4 transform;

    void main()
    {
        v_pos = vec3(model * vec4(a_pos, 1.0));
        v_nor = a_nor;

        gl_Position = transform * vec4(a_pos, 1.0);
    }
    )";

    const char *const DEFAULT_FRAGMENT_SHADER_SOURCE = R"(
    #version 330
		
    in vec3 v_pos;
    in vec3 v_nor;

    uniform vec3 object_colour;
    uniform vec3 light_colour;
    uniform vec3 light_pos;

    out vec4 fragment;

    void main()
    {
        vec3 c = object_colour;
        float dist = distance(light_pos, v_pos);
        float attenuation = 1.0f / (1.0f + 0.001 * dist + 0.0001 * dist * dist);
        vec3 light_dir = normalize(light_pos - v_pos);
        vec3 light_colour = vec3(1.0f);
        float diffuse_strength = 5.f;
        c = object_colour;
        float diff = max(dot(v_nor, light_dir), 0.0f);
        vec3 diffuse = diff * light_colour * attenuation * diffuse_strength;
        float ambient_strength = 0.2f;
        vec3 ambient = ambient_strength * light_colour;
        fragment = vec4((ambient + diffuse) * c, 1.0);
    }
    )";

    const char *const LIGHT_VERTEX_SHADER_SOURCE = R"(
    #version 330

    layout (location = 0) in vec3 a_pos;

    uniform mat4 transform;

    void main()
    {
        gl_Position = transform * vec4(a_pos, 1.0);
    }
    )";

    const char *const LIGHT_FRAGMENT_SHADER_SOURCE = R"(
    #version 330

    out vec4 frag;

    void main()
    {
        frag = vec4(1.0f);
    }
    )";
};

#endif