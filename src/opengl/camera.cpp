#include "camera.h"
#include "matrix.h"

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
	cam->fov_y = (90.f * (float)M_PI / 180.f);
	cam->near_clip = 0.01f;
	cam->far_clip = 100.f;
	float aspect = (float)cx / cy;
	float top = cam->near_clip * tanf(cam->fov_y / 2);
	float bottom = -1 * top;
	float left = bottom * aspect;
	float right = top * aspect;
	Matrix::frustrum(cam->frustrum, left, right, bottom, top, cam->near_clip, cam->far_clip);
}
