#pragma once

#include "v3.h"
#include "matrix.h"

struct Camera {
	V3 pos;
	V3 front;
	V3 up;
	float view[16];
	float frustrum[16];
	float yaw, pitch;
	float vel;
	float fov_y;
	float near_clip;
	float far_clip;
	float view_cx_center;
	float view_cy_center;
};

Camera *camera_create();
void camera_destroy(Camera *cam);
void camera_frustrum(Camera *cam, unsigned cx, unsigned cy);
void camera_update(Camera *cam, int mx, int my);

inline void camera_move_forward(Camera *cam, float dt)
{
	cam->pos += cam->vel * dt * cam->front;
}

inline void camera_move_backward(Camera *cam, float dt)
{
	cam->pos -= cam->vel * dt * cam->front;
}

inline void camera_move_left(Camera *cam, float dt)
{
	cam->pos -= Matrix::normalise(Matrix::cross(cam->front, cam->up)) * cam->vel * dt;
}

inline void camera_move_right(Camera *cam, float dt)
{
	cam->pos += Matrix::normalise(Matrix::cross(cam->front, cam->up)) * cam->vel * dt;
}

inline void camera_look_at(Camera *cam)
{
	Matrix::look_at(cam->view, cam->pos, cam->pos + cam->front, cam->up);
}