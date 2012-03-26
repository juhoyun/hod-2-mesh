#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <io.h>
#include <string.h>

#include "HodFormat.h"

using namespace std;
using namespace MATH;

#define ID_FORM		0x4D524F46		// "FORM"
#define ID_VERS		0x53524556		// "VERS"
#define ID_NAME		0x454D414E		// "NAME"
#define ID_NRML		0x4C4D524E		// "NRML"
#define ID_HVMD		0x444D5648		// "HVMD"
#define ID_STAT		0x54415453		// "STAT"
#define ID_LMIP		0x50494D4C		// "LMIP"
#define ID_MULT		0x544C554D		// "MULT"
#define ID_GOBG		0x47424F47		// "GOBG"
#define ID_HIER		0x52454948		// "HIER"
#define ID_DTRM		0x4D525444		// "DTRM"
#define ID_INFO		0x4F464E49		// "INFO"
#define ID_BMSH		0x48534D42		// "BMSH"
#define ID_BNDV		0x56444E42		// "BNDV"
#define ID_COLD		0x444C4F43		// "COLD"
#define ID_GLOW		0x574F4C47		// "GLOW"
#define ID_INFO		0x4F464E49		// "INFO"


/****************************************************
		Global Variables
*****************************************************/
static int g_NumMipmap = 0;
static int g_NumMaterial = 0;
static int g_FuncDepth = 0;

static uint32 g_CurBlock = 0;

static FILE* g_DebugFp = NULL;

vector<MATERIAL>		g_Materials;
vector<MATERIAL_INFO>	g_MatInfo;
vector<MESH_MULT>		g_Meshes;
vector<HIER_DATA>		g_HierData;

/****************************************************
		Forward declaration
*****************************************************/
uint32 Analyze_HOD(FILE *fp, const uint32 block_len);

void ChangeEndian(uint32* value)
{
	uDWORD* v = (uDWORD *)value;
	uint8 temp;

	// big endian -> little endian
	temp = v->bytes[0];
	v->bytes[0] = v->bytes[3];
	v->bytes[3] = temp;

	temp = v->bytes[1];
	v->bytes[1] = v->bytes[2];
	v->bytes[2] = temp;
}

uint32 GetString(FILE *fp, char* buf, uint32 buf_len)
{
	uint32 length;

	fread(&length, 4, 1, fp);

	if (length < (buf_len-1))
	{
		fread(buf, length, 1, fp);
		buf[length] = 0;
	} else
	{
		fread(buf, buf_len, 1, fp);
		buf[buf_len - 1] = 0;
		fseek(fp, length-buf_len, SEEK_CUR);
	}

	return (length + 4);
}

#define DEPTH_STEP 4
#define IncIndent() (g_FuncDepth += DEPTH_STEP)

void Print(const char* str, ...)
{
	if (g_DebugFp == NULL) return;

	int len = 0, i;
	char buf[200] = {0, };
	va_list va;

	va_start(va, str);
	len = vsprintf_s(buf, 200, str, va);
	buf[len] = 0;
	va_end(va);

	for(i=0; i<g_FuncDepth; i++) 
		fputc(' ', g_DebugFp);
	fprintf(g_DebugFp, buf);
}

void DecIndent(void)
{
	if (g_FuncDepth >= DEPTH_STEP)
	{
		g_FuncDepth -= DEPTH_STEP;
	} else
	{
		// unexpected situation
		Print("Invalid depth: %d\n", g_FuncDepth);
	}
}

/******************************************************

*******************************************************/

uint32 Parse_STAT(FILE *fp, const uint32 block_len)
{
	uint32 i;
	uint32 length, signature, length_processed;
	uint32 nLayers, index;
	char block_name[5] = "    ";
	char mat_name[80];
	char mat_type[80];
	char layer_name[80];

	MATERIAL mat;
	SUBMATERIAL sub;

	Print("STAT %d\n", block_len);

	g_NumMaterial++;
	// read off the signature: 0xE9030000
	fread(&signature, 4, 1, fp);
	// length of name
	fread(&length, 4, 1, fp);
	if (length > 80) 
	{
		Print("Too big length: %d\n", length);
	}
	fread(mat_name, length, 1, fp);
	mat_name[length] = 0;

	length_processed = length + 8;

	fread(&length, 4, 1, fp);
	fread(mat_type, length, 1, fp);
	mat_type[length] = 0;
	Print("%s: %s\n", mat_name, mat_type);

	// number of layer
	fread(&nLayers, 4, 1, fp);
	g_NumMipmap += nLayers;

	strcpy_s(mat.name, NAME_MAXLEN, mat_name);
	strcpy_s(mat.type, MAT_TYPE_MAXLEN,  mat_type);
	mat.nSubs = nLayers;

	length_processed += length + 8;

	IncIndent();

	for(i=0; i<nLayers; i++)
	{
		// skip 8 bytes: 05 00 00 00 04 00 00 00 
		fseek(fp, 8, SEEK_CUR);
		fread(&index, 4, 1, fp);
		fread(&length, 4, 1, fp);
		fread(layer_name, length, 1, fp);
		layer_name[length] = 0;
		Print("[%d] %s\n", index, layer_name);

		sub.index = index;
		strcpy_s(sub.name, SUBMAT_NAME_MAXLEN, layer_name);
		mat.vSub.push_back(sub);

		length_processed += length + 16;
	}

	mat.vVertices = new Vertices;
	mat.vFaces = new Faces;
	mat.mainIndex = 0;

	g_Materials.push_back(mat);

	DecIndent();

	if (length_processed != block_len)
	{
		Print("Length mismatch: processed=%d, block=%d\n", 
			length_processed, block_len);
	}

	return length_processed;
}

void RemovePath(char* path)
{
	char* last_slash;
	char* str = path;
	int length;

	while(*str++)
	{
		if (*str == '/')
			last_slash = str;
	}

	length = (int)(strlen(last_slash + 1) + 1);

	memcpy(path, last_slash+1, length);
}

uint32 Parse_LMIP(FILE *fp, uint32 block_len)
{
	uint32 length, levels, width, height, mipmap;
	uint32 length_processed;
	char filename[300];
	char type[5] = "    ";

	static int numLMIP=0;

	MATERIAL_INFO info;

	Print("LMIP %d [%d/%d]\n", block_len, numLMIP+1, g_NumMipmap);
	numLMIP++;

	fread(&length, 4, 1, fp);
	if (length > 300)
	{
		Print("Too big length: %d\n", length);
	}
	fread(filename, length, 1, fp);
	filename[length] = 0;
	RemovePath(filename);

	IncIndent();
	Print("%s\n", filename);
	fread(type, 4, 1, fp);
	fread(&levels, 4, 1, fp);
	Print("%s, levels=%d\n", type, levels);

	strcpy_s(info.name, NAME_MAXLEN, filename);
	strcpy_s(info.type, 5, type);
	info.nMipmaps = levels;

	length_processed = length + 12;

	IncIndent();
	for(mipmap=0; mipmap<levels; mipmap++)
	{
		BITMAP_DATA bitmap;

		fread(&width, 4, 1, fp);
		fread(&height, 4, 1, fp);

		bitmap.width = width;
		bitmap.height = height;
		bitmap.data = new uint8[width * height];

		fread(bitmap.data, width * height, 1, fp);

		info.vBitmaps.push_back(bitmap);
		
		Print("[%d] %d*%d\n", mipmap+1, width, height);

		length_processed += 8 + width * height;
	}
	DecIndent();
	DecIndent();

	g_MatInfo.push_back(info);

	return length_processed;
}

typedef struct
{
	uint32		signature;	// 00 00 05 78
	uint32		level;		// LOD level
	uint32		groups;		// Number of texture group
} BMSH_HEADER;

typedef struct
{
	uint32		mat_id;		// material ID
	uint32		type;		// vertex type: 0x1B / 0x0B
	uint32		vertices;	// number of vertices
} MESH_HEADER;

uint32 Parse_BMSH(FILE *fp, uint32 block_len)
{
	uint32 group, length_processed;
	BMSH_HEADER hdr;
	MESH_HEADER meshHdr;
	uint16 index_id;
	uint32 index_type, index_length;

	LOD_MESH lodMesh;

	fread(&hdr, sizeof(hdr), 1, fp);
	length_processed = sizeof(hdr);

	Print("BMSH LOD %d, %d Texture Groups\n", 
		hdr.level, hdr.groups);

	lodMesh.lodLevel = hdr.level;
	lodMesh.nGroups = hdr.groups;

	IncIndent();

	for(group=0; group<hdr.groups; group++)
	{
		MESH mesh;

		fread(&meshHdr, sizeof(meshHdr), 1, fp);
		Print("[%d] Mat %d, Type 0x%02X, %d vertices\n",
			group, meshHdr.mat_id, meshHdr.type, meshHdr.vertices);

		mesh.material = meshHdr.mat_id;
		mesh.vType = meshHdr.type;
		mesh.nVertices = meshHdr.vertices;

		uint32 v;
		for(v=0; v<meshHdr.vertices; ++v)
		{
			VERTEX vertex;
			fread(&vertex, sizeof(vertex), 1, fp);
			mesh.vVertices.push_back(vertex);
		}

		// now, handle face list
		fread(&index_id, 2, 1, fp);
		fread(&index_type, 4, 1, fp);
		fread(&index_length, 4, 1, fp);

		mesh.iType = index_type;
		mesh.nFaces = index_length;

		Print("    Idx %d, Type %d, %d entries\n",
			index_id, index_type, index_length);

		//fseek(fp, index_length*2, SEEK_CUR);
		uint32 idx;
		for(idx=0; idx<index_length; ++idx)
		{
			uint16 u16;
			fread(&u16, 2, 1, fp);
			mesh.vFaces.push_back(u16);
		}

		lodMesh.vMeshes.push_back(mesh);

		length_processed += sizeof(meshHdr) + meshHdr.vertices * 40 + 10 + index_length*2;
	}
	DecIndent();

	// set up the last element;
	if ((g_CurBlock == ID_HVMD) && (!g_Meshes.empty()))
		g_Meshes.back().meshes.push_back(lodMesh);

	return length_processed;
}

uint32 Parse_MULT(FILE *fp, uint32 id, uint32 block_len)
{
	uint32 signature, length, length_processed;
	uint32 nLods;
	char mesh_name[80];
	char joint_name[80];
	uDWORD dw;

	MESH_MULT mesh;
	
	fread(&signature, 4, 1, fp);

	length = GetString(fp, mesh_name, 80);
	length_processed = length + 4;

	length = GetString(fp, joint_name, 80);
	length_processed += length;

	if (id == ID_MULT)
	{
		fread(&nLods, 4, 1, fp);
		length_processed += 4;
	} else
	{
		// in case of GOBG, there is no lods
		nLods = 1;
	}
	
	dw.dw = id;
	Print("%c%c%c%c %d\n", 
		dw.bytes[0], dw.bytes[1], dw.bytes[2], dw.bytes[3], block_len);
	IncIndent();
	Print("%s / %s %d LODs\n", mesh_name, joint_name, nLods);

	strcpy_s(mesh.name, NAME_MAXLEN, mesh_name);
	strcpy_s(mesh.parent, NAME_MAXLEN, joint_name);
	mesh.nLods = nLods;

	g_Meshes.push_back(mesh);

	length_processed += Analyze_HOD(fp, block_len - length_processed);
	
	DecIndent();

	return length_processed;
}

void GetParentMatrix(const char* name, Matrix4* mat)
{
	HIER_DATA data;
	vector<HIER_DATA>::iterator end = g_HierData.end();
	vector<HIER_DATA>::iterator iter;

	for(iter=g_HierData.begin(); iter != end; ++iter)
	{
		if (0 == strcmp(name, iter->name))
		{
			// match found
			*mat = iter->mat;
			return;
		}
	}

	// match not found, this should be error except the 'Root' case
	mat->setIdentity();
}

uint32 Parse_HIER(FILE *fp, uint32 block_len)
{
	uint32 length_processed;
	uint32 length, nJoints, joint;

	HIER_INFO info;
	HIER_DATA data;

	Print("HIER %d\n", block_len);

	fread(&nJoints, 4, 1, fp);
	length_processed = 4;

	IncIndent();

	for(joint=0; joint<nJoints; joint++)
	{
		length = GetString(fp, data.name, NAME_MAXLEN);
		Print("%s", data.name);

		length_processed += length;

		length = GetString(fp, data.parent, NAME_MAXLEN);
		if (length > 4)
		{
			if (g_DebugFp)
				fprintf(g_DebugFp, " / %s", data.parent);
		}
		length_processed += length;
		if (g_DebugFp)
			fprintf(g_DebugFp, "\n");

		// use constant 51 instead of sizeof(info)=52
		fread(&info, 51, 1, fp);
		length_processed += 51;
		IncIndent();
		Print("Offset:   %f %f %f\n", info.x, info.y, info.z);
		Print("Rotation: %f %f %f\n", info.rx, info.ry, info.rz);
		Print("Scale:    %f %f %f\n", info.sx, info.sy, info.sz);
		DecIndent();

		data.x = info.x;
		data.y = info.y;
		data.z = info.z;

		data.rx = info.rx;
		data.ry = info.ry;
		data.rz = info.rz;

		data.sx = info.sx;
		data.sy = info.sy;
		data.sz = info.sz;

		Matrix4 scale, rotation, translation, parent;

		translation.makeTrans(data.x, data.y, data.z);
		rotation.setRotation(data.rx, data.ry, data.rz);
		scale.setIdentity();
		scale.setScale(data.sx, data.sy, data.sz);

		GetParentMatrix(data.parent, &parent);

		data.mat = parent * (translation * (rotation * scale));

		g_HierData.push_back(data);
	}

	DecIndent();

	return length_processed;
}


uint32 Parse_INFO(FILE *fp, const uint32 block_len)
{
#define INFO_GLOW	0xE8030000
#define INFO_OWNR	0x524E574F

	uint32 u32_id, temp;
	uint32 length_processed;
	char buf[80];

	fread(&u32_id, 4, 1, fp);
	length_processed = 4;
	Print("INFO %d\n", block_len-4);

	switch(u32_id)
	{
	case INFO_GLOW:
		IncIndent();
		length_processed += GetString(fp, buf, 80);
		Print("Mesh:  %s\n", buf);
		length_processed += GetString(fp, buf, 80);
		Print("Joint: %s\n", buf);
		// read off unknown dw: 0x00000002
		fread(&temp, 4, 1, fp);
		length_processed += 4;
		DecIndent();
		break;
	case INFO_OWNR:
		// read 0x0D000000
		fread(&temp, 4, 1, fp);
		length_processed += 4;
		length_processed += GetString(fp, buf, 80);
		Print("    OWNR %s\n", buf);
		break;
	default:
		Print("Unknown ID: %X\n", u32_id);
		fseek(fp, block_len - length_processed, SEEK_CUR);
		length_processed = block_len;
		break;
	}

	return length_processed;
}

uint32 Analyze_HOD(FILE *fp, const uint32 block_len)
{
	char id[5] = "    ";
	uint32 u32_id, length, temp;
	uDWORD dw;
	uint32 length_processed = 0;

	static char buf[100];

	while((!feof(fp)) && (length_processed < block_len))
	{

		if (0 == fread(&u32_id, 4, 1, fp))
		{
			// nothing to be read
			break;
		}

		switch(u32_id)
		{
		case ID_FORM:
		case ID_NRML:
			fread(&length, 4, 1, fp);
			ChangeEndian(&length);

			dw.dw = u32_id;
			Print("%c%c%c%c %d\n", 
				dw.bytes[0], dw.bytes[1], dw.bytes[2], dw.bytes[3], length);
			
			IncIndent();
			// recursive function call
			length_processed += Analyze_HOD(fp, length);

			DecIndent();
			// add the length of id & length field
			length_processed += 8;
			break;
		case ID_VERS:
			fread(&temp, 4, 1, fp);
			Print("Version: %08X\n", temp);
			length_processed += 8;
			break;
		case ID_NAME:
			length = block_len - 4;
			if (length < 100)
			{
				fread(buf, length, 1, fp);
				buf[length] = 0;
			} else
			{
				fread(buf, 99, 1, fp);
				buf[99] = 0;
				// skip remaining bytes
				fseek(fp, length-99, SEEK_CUR);
			}
			Print("Name: %s\n", buf);
			length_processed += block_len;
			break;
		case ID_HVMD:
		case ID_DTRM:
		case ID_GLOW:
			g_CurBlock = u32_id;
			dw.dw = u32_id;
			Print("%c%c%c%c %d\n", 
				dw.bytes[0], dw.bytes[1], dw.bytes[2], dw.bytes[3], block_len);
			IncIndent();
			// recursive function call
			length_processed += Analyze_HOD(fp, block_len-4);

			DecIndent();
			// add the length of id field
			length_processed += 4;
			break;
		case ID_STAT:
			length_processed += Parse_STAT(fp, block_len-4);
			length_processed += 4;
			break;
		case ID_LMIP:
			fread(&length, 4, 1, fp);
			ChangeEndian(&length);
			length_processed += Parse_LMIP(fp, length);
			length_processed += 8;
			break;
		case ID_MULT:
		case ID_GOBG:
			length_processed += Parse_MULT(fp, u32_id, block_len-4);
			length_processed += 4;
			break;
		case ID_BMSH:
			length_processed += Parse_BMSH(fp, block_len-4);
			length_processed += 4;
			break;
		case ID_HIER:
			fread(&length, 4, 1, fp);
			ChangeEndian(&length);
			length_processed += Parse_HIER(fp, length);
			length_processed += 8;
			break;
		case ID_BNDV:
			fread(&length, 4, 1, fp);
			ChangeEndian(&length);
			length_processed += length; //Parse_BNDV(fp, length);
			fseek(fp, length, SEEK_CUR);
			length_processed += 8;
			break;
		case ID_INFO:
			length_processed += Parse_INFO(fp, block_len-4);
			length_processed += 4;
			break;
		default:
			dw.dw = u32_id;
			Print("Unknown ID: %c%c%c%c\n", dw.bytes[0], dw.bytes[1], dw.bytes[2], dw.bytes[3]);
			// here, print out the length including id field(4 bytes)
			Print("%d bytes left in this block\n", block_len-length_processed);

			fseek(fp, block_len-length_processed-4, SEEK_CUR);
			length_processed = block_len;
			break;
		}
	}

	if (length_processed != block_len)
	{
		Print("Length mismatch: processed=%d, block=%d\n", 
			length_processed, block_len);
	}

	return length_processed;
}

int ReadHOD(const char* hod_filename)
{
	char filename[255];
	strcpy_s(filename, 255, hod_filename);
	strcat_s(filename, 255, ".hod");

	FILE *fp;
	errno_t err = fopen_s(&fp, filename, "rb");
	if (fp == NULL)
	{
		printf("File Open Error: %s, errno = %d\n", filename, (int)err);
		return 0;
	}

	char dbg_filename[255];
	strcpy_s(dbg_filename, 255, hod_filename);
	strcat_s(dbg_filename, 255, ".txt");
	err = fopen_s(&g_DebugFp, dbg_filename, "w");
	if (g_DebugFp == NULL)
	{
		printf("File Open Error: %s, errno = %d\n", dbg_filename, (int)err);
		// do not return
	}

	uint32 filesize = (uint32)_filelength(_fileno(fp));

	Analyze_HOD(fp, filesize);

	fclose(fp);
	if (g_DebugFp)
		fclose(g_DebugFp);

	return 1;
}
