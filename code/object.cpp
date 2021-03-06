#include <string>
#include <fstream>
#include <assert.h>

#include "win32-opengl.h"
#include "object.h"
#include "app.h"

void add_vertex(Object *obj, std::string line) 
{
    assert(line.length() && (line[0] & 0xFFDF) == 'V');
    SpookyVertex *vertex = (SpookyVertex*)malloc(sizeof(SpookyVertex));

    if (vertex) {
        memset(&vertex->nor, 0, 3 * sizeof(real32));

        const char *text = line.c_str();

        s32 index = 0;
        vertex->pos.E[index++] = atof(text + 1);
        s32 start = 1;

        do {
            while (text[start] == ' ' || text[start] == '\t' || text[start] == '-') {
                assert(start < strlen(text));
                ++start;
            }

            while (text[start] != ' ' && text[start] != '\t') {
                assert(start < strlen(text));
                ++start;
            }

            vertex->pos.E[index++] = atof(text + start);
        } while (index < 3);

        obj->vertices.push_back(vertex);
    }
}

void add_polygon(Object *obj, std::string line) 
{
    assert(line.length() && (line[0] & 0xFFDF) == 'F');
    Poly *polygon = (Poly*)malloc(sizeof(Poly));

    if (polygon) {
        const char *text = line.c_str();
    
        s32 index = 0;
        polygon->indices[index++] = atoi(text + 1) - 1;
        s32 start = 1;

        do {
            while (text[start] == ' ' || text[start] == '\t' || text[start] == '-') {
                assert(start < strlen(text));
                ++start;
            }

            while (text[start] != ' ' && text[start] != '\t') {
                assert(start < strlen(text));
                ++start;
            }

            polygon->indices[index++] = atoi(text + start) - 1;
        } while (index < 3);

        obj->polygons.push_back(polygon);
    }
}

Object *load_object(const char *filename)
{
    Object *obj = new Object();
    
    std::ifstream file(filename);

    if (file) {
        std::string line;
        std::getline(file, line);

        while (!file.eof()) {
            if (line.length()) {
                switch (line[0]) {
                    case 'v': case 'V': add_vertex(obj, line); break;
                    case 'f': case 'F': add_polygon(obj, line); break;
                }
            }

            std::getline(file, line);
        }

        file.close();

        //Calculate normals for each vertex for each sum normals of surrounding.
        for (u32 i = 0; i < obj->polygons.size(); i++) {
                SpookyVertex *a = obj->vertices[obj->polygons[i]->indices[0]];
                SpookyVertex *b = obj->vertices[obj->polygons[i]->indices[1]];
                SpookyVertex *c = obj->vertices[obj->polygons[i]->indices[2]];

                V3 cp = v3_cross(b->pos - a->pos, c->pos - a->pos);
                a->nor += cp;
                b->nor += cp;
                c->nor += cp;
        }

        // Average sum of normals for each vertex.
        for (u32 i = 0; i < obj->vertices.size(); i++) {
            obj->vertices[i]->nor = v3_normalise(obj->vertices[i]->nor);
        }
    }

    return obj;
}

void create_vbos(Object *obj)
{
    s32 vertex_size = sizeof(real32) * obj->vertices.size() * 6;
    real32 *vert_data = (real32*)malloc(vertex_size);

    s32 polygon_size = sizeof(u32) * obj->polygons.size() * 3;
    u32 *poly_data = (u32*)malloc(polygon_size);

    if (vert_data && poly_data) {
        glGenBuffers(2, obj->vbos);

        s32 offset = 0;
        for (s32 i = 0; i < obj->vertices.size(); i++, offset += 6) {
            memcpy(vert_data + offset, obj->vertices[i], 6 * sizeof(real32));
        }

        glBindBuffer(GL_ARRAY_BUFFER, obj->vbos[0]);
        glBufferData(GL_ARRAY_BUFFER, vertex_size, vert_data, GL_STATIC_DRAW);

        offset = 0;
        for (s32 i = 0; i < obj->polygons.size(); i++, offset += 3) {
            memcpy(poly_data + offset, obj->polygons[i]->indices, 3 * sizeof(u32));
        }

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, obj->vbos[1]);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, polygon_size, poly_data, GL_STATIC_DRAW);
    }

    free(vert_data);
    free(poly_data);
}

void draw_object(Object *obj, app_state *state, real32 *model)
{
	glBindBuffer(GL_ARRAY_BUFFER, obj->vbos[0]);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(real32), (void*)0);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(real32), (void*)(3 * sizeof(real32)));
	glEnableVertexAttribArray(1);

	glUniformMatrix4fv(state->simple_shader.projection, 1, GL_FALSE, state->cur_cam.frustrum);
	glUniformMatrix4fv(state->simple_shader.view, 1, GL_FALSE, state->cur_cam.view);
	glUniformMatrix4fv(state->simple_shader.model, 1, GL_FALSE, model);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, obj->vbos[1]);
	glDrawElements(GL_TRIANGLES, 3 * obj->polygons.size(), GL_UNSIGNED_INT, 0);
}