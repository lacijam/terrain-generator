#pragma once

struct V3 {
	union {
		real32 E[3];
		struct {
    		real32 x, y, z;
		};
	};
};