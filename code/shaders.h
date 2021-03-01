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

    uniform mat4 projection;
    uniform mat4 view;
    uniform mat4 model;
    
    uniform vec4 plane; 

    void main()
    {
        vec4 world_position = model * vec4(a_pos, 1.0);

        v_pos = vec3(world_position);
        v_nor = a_nor;

        gl_ClipDistance[0] = dot(world_position, plane);

        gl_Position = projection * view * model * vec4(a_pos, 1.0);
    }
    )";

    const char *const DEFAULT_FRAGMENT_SHADER_SOURCE = R"(
    #version 330
		
    in vec3 v_pos;
    in vec3 v_nor;

    uniform float ambient_strength;
    uniform float diffuse_strength;
    uniform float specular_strength;

    uniform vec3 light_pos;
    uniform vec3 light_colour;
    uniform vec3 grass_colour;
    uniform vec3 sand_colour;
    uniform vec3 snow_colour;
    uniform vec3 slope_colour;

    uniform float water_height;
    uniform float sand_height;
    uniform float snow_height;

    uniform vec3 view_position;

    out vec4 frag;

    void main()
    {
        //float dist = distance(light_pos, v_pos);
        //float attenuation = 1.0f / (1.0f + 0.001 * dist + 0.0001 * dist * dist);
        float attenuation = 1.0f;
        
        vec3 light_colour = vec3(1.0f, 0.9f, 0.8f);
        vec3 ambient = ambient_strength * light_colour;

        vec3 light_dir = normalize(light_pos - v_pos);
        float diff = max(dot(v_nor, light_dir), 0.0f);
        vec3 diffuse = diff * light_colour * attenuation * diffuse_strength;

        vec3 view_dir = normalize(view_position + v_pos);
        vec3 reflect_dir = reflect(-light_dir, v_nor);
        float spec = pow(max(dot(view_dir, reflect_dir), 0.f), 3);
        vec3 specular = specular_strength * spec * light_colour;
        
        vec4 stone_colour_rgba = vec4(0.3f, 0.3f, 0.3f, 1.f);
        vec4 grass_colour_rgba = vec4(grass_colour, 1.0f);
        vec4 slope_colour_rgba = vec4(slope_colour, 1.0f);
        vec4 snow_colour_rgba = vec4(snow_colour, 1.0f);
        vec4 sand_colour_rgba = vec4(sand_colour, 1.0f);

        vec4 blended = mix(grass_colour_rgba, slope_colour_rgba, 1.0f - dot(v_nor, vec3(0.f, 1.f, 0.f)));
        blended = mix(blended, stone_colour_rgba, min(v_pos.y / snow_height, 1.f));
        //blended = mix(blended, snow_colour_rgba, min(v_pos.y / snow_height, 0.2f));

        blended = mix(blended, sand_colour_rgba, 1.f - min(v_pos.y / sand_height, 1.f));

        frag = vec4(ambient + diffuse + specular, 1.f) * blended;
    }
    )";

    const char *const SIMPLE_VERTEX_SHADER_SOURCE = R"(
    #version 330

    layout (location = 0) in vec3 a_pos;

    uniform mat4 projection;
    uniform mat4 view;
    uniform mat4 model;

    void main()
    {
        gl_Position = projection * view * model * vec4(a_pos, 1.0);
    }
    )";

    const char *const SIMPLE_FRAGMENT_SHADER_SOURCE = R"(
    #version 330

    out vec4 frag;

    void main()
    {
        frag = vec4(1.0f);
    }
    )";

    const char *const WATER_VERTEX_SHADER_SOURCE = R"(
    #version 330

    layout (location = 0) in vec3 a_pos;

    out vec4 clip_space;

    uniform mat4 projection;
    uniform mat4 view;
    uniform mat4 model;

    void main()
    {
        clip_space = projection * view * model * vec4(a_pos, 1.0);
        gl_Position = clip_space;
    }
    )";

    const char *const WATER_FRAGMENT_SHADER_SOURCE = R"(
    #version 330

    in vec4 clip_space;

    out vec4 frag;

    uniform sampler2D reflection_texture;
    uniform sampler2D refraction_texture;

    uniform vec3 water_colour;

    void main()
    {
        vec2 ndc = (clip_space.xy / clip_space.w) / 2.f + 0.5f;
        vec2 reflect_tex_coords = vec2(ndc.x, -ndc.y);
        vec2 refract_tex_coords = vec2(ndc.x, ndc.y);

        vec4 reflect_colour = texture(reflection_texture, reflect_tex_coords);
        vec4 refract_colour = texture(refraction_texture, refract_tex_coords);

        vec4 water_colour_rgba = vec4(water_colour, 1.0f);

        frag = mix(reflect_colour, refract_colour, 0.5f);
        frag = mix(frag, water_colour_rgba, 0.5f);
    }
    )";
};

#endif