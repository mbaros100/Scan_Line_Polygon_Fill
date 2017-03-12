//
// Created by vahagng on 2/19/17.
//

#ifndef SCANLINE_DEBUG_STRUCT_H
#define SCANLINE_DEBUG_STRUCT_H

typedef struct _point
{
	int x;
	int y;
	struct _point *next;
} point;

typedef struct _edge
{
	int ymax;
	int x;
	int dx;
	int dy;
	int sign;
	int sum;
	struct _edge *next;
} edge;

#endif //SCANLINE_DEBUG_STRUCT_H
