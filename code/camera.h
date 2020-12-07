#pragma once

struct Camera {
	V3 pos;
	V3 front;
	V3 up;
	float view[16];
	float frustrum[16];
	float yaw, pitch;
	float vel;
};

static void camera_create(Camera *cam);
static void camera_frustrum(Camera *cam, unsigned cx, unsigned cy);
static void camera_update(Camera *cam);

static inline void camera_move_forward(Camera *cam, float dt)
{
	cam->pos += cam->vel * dt * cam->front;
}

static inline void camera_move_backward(Camera *cam, float dt)
{
	cam->pos -= cam->vel * dt * cam->front;
}

static inline void camera_move_left(Camera *cam, float dt)
{
	cam->pos -= Matrix::normalise(Matrix::cross(cam->front, cam->up)) * cam->vel * dt;
}

static inline void camera_move_right(Camera *cam, float dt)
{
	cam->pos += Matrix::normalise(Matrix::cross(cam->front, cam->up)) * cam->vel * dt;
}

static inline void camera_look_at(Camera *cam)
{
	Matrix::look_at(cam->view, cam->pos, cam->pos + cam->front, cam->up);
}