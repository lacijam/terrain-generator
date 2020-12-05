#pragma once

#include "v3.h"

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
};

Camera *camera_create();
void camera_destroy(Camera *cam);
void camera_frustrum(Camera *cam, unsigned cx, unsigned cy);