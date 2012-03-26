#ifndef _HOD_FORMAT_H_
#define _HOD_FORMAT_H_

#include "Matrix4.h"
#include <vector>

#define NAME_MAXLEN 80
#define MAT_TYPE_MAXLEN 20
#define SUBMAT_NAME_MAXLEN 20

typedef unsigned char  uint8;
typedef unsigned short uint16;
typedef unsigned int   uint32;

typedef union
{
	unsigned char bytes[4];
	unsigned int  dw;
} DWORD;

typedef struct
{
	float x, y, z;		// position
	float rx, ry, rz;	// rotation
	float sx, sy, sz;	// scale
	uint32 jx, jy, jz;	// joint? shear?
	uint8 bx, by, bz;	// ??
} HIER_INFO;

typedef struct
{
	char name[NAME_MAXLEN];
	char parent[NAME_MAXLEN];
	float x, y, z;		// offset
	float rx, ry, rz;	// rotation
	float sx, sy, sz;	// scale
	MATH::Matrix4 mat;
} HIER_DATA;

typedef struct
{
	float x, y, z;		// position
	float rsv1;
	float nx, ny, nz;	// normal
	float rsv2;
	float u, v;			// texture coord
} VERTEX;

typedef struct
{
	int		index;
	char	name[SUBMAT_NAME_MAXLEN];
} SUBMATERIAL;

typedef std::vector<SUBMATERIAL> SubMaterialArrary;
typedef std::vector<VERTEX> Vertices;
typedef std::vector<uint16> Faces;


typedef struct
{
	char	name[NAME_MAXLEN];
	char	type[MAT_TYPE_MAXLEN];
	int		nSubs;		// number of sub materials
	int		mainIndex;	// main sub material index
	SubMaterialArrary vSub;
	Vertices* vVertices;
	Faces* vFaces;
} MATERIAL;

typedef struct
{
	uint16 width;
	uint16 height;
	uint8* data;
} BITMAP_DATA;

typedef struct
{
	char name[NAME_MAXLEN];
	char type[5];
	int  nMipmaps;
	std::vector<BITMAP_DATA> vBitmaps;
} MATERIAL_INFO;

typedef struct
{
	int material;
	int vType;		// vertex type
	int nVertices;
	int iType;		// index type
	int nFaces;
	Vertices vVertices;
	Faces vFaces;
} MESH;

typedef std::vector<MESH> Meshes;

typedef struct
{
	int lodLevel;
	int nGroups;
	int nVertices;		// total number of vertices of vMeshes vector
	Meshes vMeshes;
} LOD_MESH;

typedef std::vector<LOD_MESH> LodMeshes;

typedef struct
{
	char name[NAME_MAXLEN];
	char parent[NAME_MAXLEN];
	int  nLods;
	LodMeshes meshes;
} MESH_MULT;


#endif