#include "camera.h"

#include <assert.h>

void camera_init(Camera *cam)
{
	cam->pos = { 0.f, 0.f, 0.f };
	cam->front = { 0.f, 0.f, -1.f };
	cam->up = { 0.f, 1.f, 0.f };
	mat4_identity(cam->view);
	mat4_identity(cam->frustrum);
	mat4_identity(cam->ortho);
	cam->yaw = 0;
	cam->pitch = 0;
	cam->fly_speed = 100.f;
	cam->walk_speed = 3.f;
	cam->look_speed = 100.f;
	cam->fov = 60.f;
	cam->flying = true;
}

void camera_frustrum(Camera *cam, u32 cx, u32 cy)
{
	real32 near_clip = .05f;
	real32 far_clip = 10000.f;
	real32 fov_y = (cam->fov * (real32)M_PI / 180.f);
	real32 aspect = (real32)cx / cy;
	real32 top = near_clip * tanf(fov_y / 2);
	real32 bottom = -1 * top;
	real32 left = bottom * aspect;
	real32 right = top * aspect;
	mat4_frustrum(cam->frustrum, left, right, bottom, top, near_clip, far_clip);
}

void camera_ortho(Camera* cam, u32 cx, u32 cy)
{
	assert(cam && cx > 0 && cy > 0);
	mat4_ortho(cam->ortho, 0, cx, 0, cy, .5f, 10000.f);
}

void camera_update(Camera *cam)
{
	if (cam->pitch >= 89.f) {
		cam->pitch = 89.f;
	} else if (cam->pitch <= -89.f) {
		cam->pitch = -89.f;
	}

	V3 direction;
	direction.x = cosf(radians(cam->yaw)) * cosf(radians(cam->pitch));
	direction.y = sinf(radians(cam->pitch));
	direction.z = sinf(radians(cam->yaw)) * cosf(radians(cam->pitch));
	cam->front = v3_normalise(direction);
}

inline void camera_move_forward(Camera *cam, real32 dt)
{
	real32 vel = cam->walk_speed;

	if (cam->flying) {
		vel = cam->fly_speed;
	}

	cam->pos += vel * dt * cam->front;
}

inline void camera_move_backward(Camera *cam, real32 dt)
{
	real32 vel = cam->walk_speed;

	if (cam->flying) {
		vel = cam->fly_speed;
	}

	cam->pos -= vel * dt * cam->front;
}

inline void camera_move_left(Camera *cam, real32 dt)
{
	real32 vel = cam->walk_speed;

	if (cam->flying) {
		vel = cam->fly_speed;
	}

	cam->pos -= v3_normalise(v3_cross(cam->front, cam->up)) * vel * dt;
}

inline void camera_move_right(Camera *cam, real32 dt)
{
	real32 vel = cam->walk_speed;

	if (cam->flying) {
		vel = cam->fly_speed;
	}

	cam->pos += v3_normalise(v3_cross(cam->front, cam->up)) * vel * dt;
}

inline void camera_look_at(Camera *cam)
{
	mat4_look_at(cam->view, cam->pos, cam->pos + cam->front, cam->up);
}