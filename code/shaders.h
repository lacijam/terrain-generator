#ifndef SHADERS_H
#define SHADERS_H

namespace Shaders {
    const char *const DEFAULT_VERTEX_SHADER_SOURCE = R"(
    #version 330

    layout (location = 0) in vec3 a_pos;
    layout (location = 1) in vec3 a_nor;

    out vec3 v_pos;
    out vec3 v_nor;
    out vec4 frag_pos_light_space;

    uniform mat4 projection;
    uniform mat4 view;
    uniform mat4 model;
    uniform mat4 light_space_matrix;
    
    uniform vec4 plane; 

    void main()
    {
        vec4 world_position = model * vec4(a_pos, 1.0);

        v_pos = vec3(world_position);
        v_nor = a_nor;
        frag_pos_light_space = light_space_matrix * world_position;

        gl_ClipDistance[0] = dot(world_position, plane);

        gl_Position = projection * view * world_position;
    }
    )";

    const char *const DEFAULT_FRAGMENT_SHADER_SOURCE = R"(
    #version 330
		
    in vec3 v_pos;
    in vec3 v_nor;
    in vec4 frag_pos_light_space;

    uniform float ambient_strength;
    uniform float diffuse_strength;
    uniform float specular_strength;
    uniform float gamma_correction;

    uniform vec3 light_pos;
    uniform vec3 light_colour;
    uniform vec3 ground_colour;
    uniform vec3 sand_colour;
    uniform vec3 stone_colour;
    uniform vec3 snow_colour;
    uniform vec3 slope_colour;
    uniform vec3 view_position;

    uniform float water_height;
    uniform float sand_height;
    uniform float snow_height;
    uniform float stone_height;

    uniform sampler2D shadow_map;

    out vec4 frag;

    float bias(float x, float b) {
        b = -log2(1.0 - b);
        return 1.0 - pow(1.0 - pow(x, 1./b), b);
    }

    float shadow_calculation(vec4 f_pos_light_space, vec3 light_dir)
    {
        vec3 proj_coords = frag_pos_light_space.xyz / frag_pos_light_space.w;
        proj_coords = proj_coords * 0.5f + 0.5f;

        if (proj_coords.z > 1.f)
            return 0.f;

        float closest_depth = texture(shadow_map, proj_coords.xy).r;
        float current_depth = proj_coords.z;
        float bias = max(0.05f * (1.f - dot(v_nor, light_dir)), 0.005f);
        return current_depth - bias > closest_depth ? 1.f : 0.f;
    }

    void main()
    {
        vec3 ambient = ambient_strength * light_colour;

        vec3 light_dir = normalize(light_pos - v_pos);
        float diff = max(dot(v_nor, light_dir), 0.0f);
        vec3 diffuse = diff * light_colour * diffuse_strength;

        vec3 view_dir = normalize(view_position + v_pos);
        vec3 halfway_dir = normalize(light_dir + view_dir);
        float spec = pow(max(dot(v_nor, halfway_dir), 0.f), 3);
        vec3 specular = specular_strength * spec * light_colour;
        
        vec3 terrain_colour = mix(ground_colour, slope_colour, 1.0f - dot(v_nor, vec3(0.f, 1.f, 0.f)));
        terrain_colour = mix(terrain_colour, stone_colour, min(1, v_pos.y / stone_height));

        if (v_pos.y > snow_height) {
            terrain_colour = mix(terrain_colour, snow_colour, min(1, bias((v_pos.y - snow_height) / snow_height, 0.6)));
        }

        terrain_colour = mix(terrain_colour, sand_colour, 1.f - min(v_pos.y / sand_height, 1.f));

        float shadow = shadow_calculation(frag_pos_light_space, light_dir);
        vec3 lighting = (ambient + (1.f - shadow) * (diffuse + specular));        
        terrain_colour *= lighting;
        terrain_colour = pow(terrain_colour, vec3(1.f / gamma_correction));

        frag = vec4(terrain_colour, 1.f);
    }
    )";

    const char *const SIMPLE_VERTEX_SHADER_SOURCE = R"(
    #version 330

    layout (location = 0) in vec3 a_pos;
    layout (location = 1) in vec3 a_nor;

    out vec3 v_pos;
    out vec3 v_nor;

    uniform mat4 projection;
    uniform mat4 view;
    uniform mat4 model;
    
    void main()
    {
        vec4 world_position = model * vec4(a_pos, 1.f);
        vec4 world_normal = model * vec4(a_nor, 0.f);

        v_pos = vec3(world_position);
        v_nor = vec3(world_normal);

        gl_Position = projection * view * world_position;
    }
    )";

    const char *const SIMPLE_FRAGMENT_SHADER_SOURCE = R"(
    #version 330

    in vec3 v_pos;
    in vec3 v_nor;

    out vec4 frag;

    uniform float ambient_strength;
    uniform float diffuse_strength;
    uniform float gamma_correction;

    uniform vec3 light_pos;
    uniform vec3 light_colour;
    uniform vec3 object_colour;

    void main()
    {
        vec3 ambient = ambient_strength * light_colour;

        vec3 light_dir = normalize(light_pos - v_pos);
        float diff = max(dot(v_nor, light_dir), 0.0f);
        vec3 diffuse = diff * light_colour * diffuse_strength;

        vec3 lighting = (ambient + diffuse);        
        vec3 colour = object_colour * lighting;
        colour = pow(colour, vec3(1.f / gamma_correction));

        frag = vec4(colour, 1.f);
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

    const char *const DEPTH_VERTEX_SHADER_SOURCE = R"(
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

    const char *const DEPTH_FRAGMENT_SHADER_SOURCE = R"(
    #version 330

    void main()
    {
    }
    )";
};

#endif