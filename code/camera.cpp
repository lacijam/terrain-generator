#include "camera.h"

#include <windows.h>
#include <assert.h>

Camera *camera_create()
{
	Camera *cam = (Camera*)HeapAlloc(GetProcessHeap(), 0, sizeof(Camera));
	cam->pos = { 0.f, 0.f, 0.f };
	cam->front = { 0.f, 0.f, -1.f };
	cam->up = { 0.f, 1.f, 0.f };
	Matrix::identity(cam->view);
	Matrix::identity(cam->frustrum);
	cam->view_cx_center = 0;
	cam->view_cy_center = 0;
	cam->yaw = 0;
	cam->pitch = 0;
	cam->vel = 40.f;
	cam->fov_y = 0;
	cam->near_clip = 0;
	cam->far_clip = 0;
	return cam;
}

void camera_destroy(Camera *cam)
{
	assert(cam);
	HeapFree(GetProcessHeap(), 0, cam);
}

void camera_frustrum(Camera *cam, unsigned cx, unsigned cy)
{
	assert(cam && cx > 0 && cy > 0);
	cam->fov_y = (90.f * (float)M_PI / 180.f);
	cam->near_clip = 0.01f;
	cam->far_clip = 100.f;
	cam->view_cx_center = cx / 2;
	cam->view_cy_center = cy / 2;
	float aspect = (float)cx / cy;
	float top = cam->near_clip * tanf(cam->fov_y / 2);
	float bottom = -1 * top;
	float left = bottom * aspect;
	float right = top * aspect;
	Matrix::frustrum(cam->frustrum, left, right, bottom, top, cam->near_clip, cam->far_clip);
}

void camera_update(Camera *cam, int mx, int my)
{
	float x_off = mx - cam->view_cx_center;
	float y_off = cam->view_cy_center - my;

	cam->yaw += x_off * 0.1f;
	cam->pitch += y_off * 0.1f;
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