#include "camera.h"

static void camera_init(Camera *cam)
{
	cam->pos = { 0.f, 0.f, 0.f };
	cam->front = { 0.f, 0.f, -1.f };
	cam->up = { 0.f, 1.f, 0.f };
	Matrix::identity(cam->view);
	Matrix::identity(cam->frustrum);
	cam->yaw = 0;
	cam->pitch = 0;
	cam->vel = 40.f;
}

static void camera_frustrum(Camera *cam, unsigned cx, unsigned cy)
{
	assert(cam && cx > 0 && cy > 0);
	float near_clip = 0.01f;
	float far_clip = 1000.f;
	float fov_y = (90.f * (float)M_PI / 180.f);
	float aspect = (float)cx / cy;
	float top = near_clip * tanf(fov_y / 2);
	float bottom = -1 * top;
	float left = bottom * aspect;
	float right = top * aspect;
	Matrix::frustrum(cam->frustrum, left, right, bottom, top, near_clip, far_clip);
}

static void camera_update(Camera *cam)
{
	if (cam->pitch > 89.f) {
		cam->pitch = 89.f;
	} else if (cam->pitch <= -89.f) {
		cam->pitch = -89.f;
	}

	V3 direction;
	direction.x = cosf(Matrix::radians(cam->yaw)) * cosf(Matrix::radians(cam->pitch));
	direction.y = sinf(Matrix::radians(cam->pitch));
	direction.z = sinf(Matrix::radians(cam->yaw)) * cosf(Matrix::radians(cam->pitch));
	cam->front = Matrix::normalise(direction);
}