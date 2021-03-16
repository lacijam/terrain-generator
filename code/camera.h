#ifndef CAMERA_H
#define CAMERA_H

#include "maths.h"
#include "types.h"

struct Camera {
	V3 pos;
	V3 front;
	V3 up;
	real32 view[16];
	real32 frustrum[16];
	real32 ortho[16];
	real32 yaw, pitch;
	real32 fly_speed;
	real32 walk_speed;
	real32 look_speed;
	real32 fov;
	bool32 flying;
};

extern void camera_init(Camera *cam);
extern void camera_frustrum(Camera* cam, u32 cx, u32 cy);
extern void camera_ortho(Camera *cam, u32 cx, u32 cy);
extern void camera_update(Camera *cam);
extern void camera_move_forward(Camera *cam, real32 dt);
extern void camera_move_backward(Camera *cam, real32 dt);
extern void camera_move_left(Camera *cam, real32 dt);
extern void camera_move_right(Camera *cam, real32 dt);
extern void camera_look_at(Camera *cam);

#endif