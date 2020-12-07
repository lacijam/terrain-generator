#pragma once

struct V3 {
	union {
		real32 E[3];
		struct {
    		float x, y, z;
		};
	};
};