#pragma once

struct Camera {
	V3 pos;
	V3 front;
	V3 up;
	real32 view[16];
	real32 frustrum[16];
	real32 yaw, pitch;
	real32 vel;
};