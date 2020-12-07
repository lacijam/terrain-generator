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