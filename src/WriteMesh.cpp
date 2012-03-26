#pragma warning(disable:4819)

#include <ogre.h>
#include <OgreStringConverter.h>
#include <OgreDefaultHardwareBufferManager.h>
#include <OgreHardwareVertexBuffer.h>
#include <OgreVertexIndexData.h>
#include <OgreResourceGroupManager.h>

#include "HodFormat.h"

using namespace std;
using namespace MATH;

void GetParentMatrix(const char* name, Matrix4* mat);

/****************************************************
	External Global Veriables
****************************************************/
extern vector<MATERIAL>			g_Materials;
extern vector<MATERIAL_INFO>	g_MatInfo;
extern vector<MESH_MULT>		g_Meshes;
extern vector<HIER_DATA>		g_HierData;

/****************************************************
	Global Variables
****************************************************/
Ogre::LogManager g_LogMgr;

struct DDPixelFormat
{
	uint32 size;
	uint32 flags;
	uint32 fourCC;
	uint32 bpp;
	uint32 redMask;
	uint32 greenMask;
	uint32 blueMask;
	uint32 alphaMask;
};

struct DDSCaps
{
	uint32 caps;
	uint32 caps2;
	uint32 caps3;
	uint32 caps4;
};

struct DDColorKey
{
	uint32 lowVal;
	uint32 highVal;
};

struct DDSurfaceDesc
{
	uint32 size;
	uint32 flags;
	uint32 height;
	uint32 width;
	uint32 pitch;
	uint32 depth;
	uint32 mipMapLevels;
	uint32 alphaBitDepth;
	uint32 reserved;
	uint32 surface;

	DDColorKey ckDestOverlay;
	DDColorKey ckDestBlt;
	DDColorKey ckSrcOverlay;
	DDColorKey ckSrcBlt;

	DDPixelFormat format;
	DDSCaps caps;

	uint32 textureStage;
};


void PrintFloat(FILE*fp, float f)
{
	if ((-0.01f < f) && (f < 0.01f) && (f != 0))
	{
		fprintf(fp, " %e", f);
	} else
	{
		fprintf(fp, " %f", f);
	}
}

void ConstructDDSHeader(MATERIAL_INFO& matInfo, DDSurfaceDesc* desc)
{
	memset(desc, 0, sizeof(DDSurfaceDesc));

	desc->size = 124;
	desc->flags = 0x0A1007;
	desc->height = matInfo.vBitmaps[0].height;
	desc->width = matInfo.vBitmaps[0].width;
	desc->pitch = desc->height * desc->width;
	//desc->depth = 0;
	desc->mipMapLevels = matInfo.nMipmaps;

	desc->format.size = 32;
	desc->format.flags = 4;				// DDPF_FOURCC
	desc->format.fourCC = 0x35545844;	// "DXT5"

	//desc->format.bpp;
	//desc->format.redMask;
	//desc->format.greenMask;
	//desc->format.blueMask;
	//desc->format.alphaMask;

	desc->caps.caps = 0x401008;			// 

}

void ExtractTextures(MATERIAL* mat)
{
	// "ship", "thruster" : main index = 0
	// change only if main index is not 0 which is default value.
	if (0 == strcmp(mat->type, "badge"))
	{
		mat->mainIndex = 1;
	} else if (0 == strcmp(mat->type, "default"))
	{
		// nothing to do
		return;
	}

	int index = mat->vSub[mat->mainIndex].index;
	MATERIAL_INFO& matInfo = g_MatInfo[index];
	DDSurfaceDesc ddSurfDesc;
	ConstructDDSHeader(matInfo, &ddSurfDesc);

	char filename[80];
	strcpy_s(filename, 80, mat->name);
	strcat_s(filename, 80, ".dds");
	FILE* fp;
	errno_t err = fopen_s(&fp, filename, "wb");
	if (fp == NULL)
	{
		printf("File Open Error: %s, errno=%d\n", filename, (int)err);
		return;
	}

	const uint32 signature = 0x20534444;	// "DDS "
	fwrite(&signature, 4, 1, fp);
	fwrite(&ddSurfDesc, sizeof(ddSurfDesc), 1, fp);

	vector<BITMAP_DATA>::const_iterator end = matInfo.vBitmaps.end();
	vector<BITMAP_DATA>::const_iterator itr = matInfo.vBitmaps.begin();

	for(; itr != end; ++itr)
	{
		fwrite(itr->data, itr->height * itr->width, 1, fp);
	}

	fclose(fp);
}


void ReassembleData()
{
	vector<MESH_MULT>::const_iterator meshEnd = g_Meshes.end();
	vector<MESH_MULT>::const_iterator meshItr = g_Meshes.begin();

	for(; meshItr != meshEnd; ++meshItr)
	{
		const char* name = meshItr->name;

		Matrix4 mat;
		GetParentMatrix(meshItr->parent, &mat);

		// take the LOD 0 meshes
		const Meshes &meshes = meshItr->meshes[0].vMeshes;

		for(size_t g=0; g<meshes.size(); ++g)
		{
			int matIndex = meshes[g].material;
			Vertices* vVertices = g_Materials[matIndex].vVertices;
			size_t offset = vVertices->size();

			Vertices::const_iterator end = meshes[g].vVertices.end();
			Vertices::const_iterator iter = meshes[g].vVertices.begin();

			for(; iter != end; ++iter)
			{
				Vector4 pos(iter->x, iter->y, iter->z, 1);
				Vector4 normal(iter->nx, iter->ny, iter->nz, 0);
				Vector4 posTranslated = mat * pos;
				Vector4 normalTranslated = mat * normal;

				VERTEX v;
				v.x = posTranslated.x;
				v.y = posTranslated.y;
				v.z = posTranslated.z;

				v.nx = normalTranslated.x;
				v.ny = normalTranslated.y;
				v.nz = normalTranslated.z;

				v.u = iter->u;
				v.v = iter->v;

				vVertices->push_back(v);
			}

			Faces::const_iterator faceEnd = meshes[g].vFaces.end();
			Faces::const_iterator faceIter = meshes[g].vFaces.begin();
			Faces* vFaces = g_Materials[matIndex].vFaces;

			for(; faceIter != faceEnd; ++faceIter)
			{
				vFaces->push_back(static_cast<uint16>(*faceIter + offset));
			}
		}
	}
}

int WriteMaterial(const char* meshName)
{
	Ogre::MaterialManager matMgr;
	Ogre::LodStrategyManager lodMgr;
	matMgr.initialise();
	Ogre::MaterialSerializer matSer;

	g_LogMgr.logMessage("Number of materials : " + Ogre::StringConverter::toString(g_Materials.size()));

	vector<MATERIAL>::iterator matEnd = g_Materials.end();
	vector<MATERIAL>::iterator matItr = g_Materials.begin();
	bool bExportMat = false;

	for(; matItr != matEnd; ++matItr)
	{
		Ogre::String matName(matItr->name);
		g_LogMgr.logMessage("Creating material " + matName);

		Ogre::MaterialPtr ogremat = matMgr.create(matName, 
			Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);

		ogremat->setAmbient(0.2f, 0.2f, 0.2f);
		ogremat->setDiffuse(0.8f, 0.8f, 0.8f, 1);
		//ogremat->setSpecular(,,, 1);
		//ogremat->setShininess();

		Ogre::TextureUnitState *tu;
		tu = ogremat->getTechnique(0)->getPass(0)->createTextureUnitState(matName + ".dds");

		matSer.queueForExport(ogremat);
		bExportMat = true;
	}

	if (bExportMat)
		matSer.exportQueued(Ogre::String(meshName)+".material");

	return 1;
}

int WriteMeshData(const char* meshName)
{
	Ogre::DefaultHardwareBufferManager defHWBufMgr;
	Ogre::LodStrategyManager lodMgr;
	Ogre::MeshManager meshMgr;

	Ogre::MeshPtr ogreMesh = Ogre::MeshManager::getSingleton().createManual(meshName,
		Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);

	Ogre::Vector3 min, max, currpos;
	Ogre::Real maxSquaredRadius;
	bool bFirst = true;

	vector<MATERIAL>::iterator matEnd = g_Materials.end();
	vector<MATERIAL>::iterator matItr = g_Materials.begin();

	for(; matItr != matEnd; ++matItr)
	{
		const Vertices* pvVertices = matItr->vVertices;
		if (pvVertices->size() == 0) continue;

		Ogre::String submeshName(matItr->name);
		g_LogMgr.logMessage("Creating SubMesh object..." + submeshName);

		Ogre::SubMesh* ogreSubMesh = ogreMesh->createSubMesh();
		ogreSubMesh->vertexData = new Ogre::VertexData();
		ogreSubMesh->vertexData->vertexCount = pvVertices->size();
		ogreSubMesh->vertexData->vertexStart = 0;

		#define POSITION_BINDING 0
		#define NORMAL_BINDING 1
		#define TEXCOORD_BINDING 2

		Ogre::VertexBufferBinding* bind = ogreSubMesh->vertexData->vertexBufferBinding;
		Ogre::VertexDeclaration* decl = ogreSubMesh->vertexData->vertexDeclaration;

		decl->addElement(POSITION_BINDING, 0, Ogre::VET_FLOAT3, Ogre::VES_POSITION);
		decl->addElement(NORMAL_BINDING, 0, Ogre::VET_FLOAT3, Ogre::VES_NORMAL);
		decl->addElement(TEXCOORD_BINDING, 0, Ogre::VET_FLOAT2, Ogre::VES_TEXTURE_COORDINATES);
		
		// Create buffers
		Ogre::HardwareVertexBufferSharedPtr pbuf = Ogre::HardwareBufferManager::getSingleton().
			createVertexBuffer(decl->getVertexSize(POSITION_BINDING), ogreSubMesh->vertexData->vertexCount,
			Ogre::HardwareBuffer::HBU_DYNAMIC, false);
		Ogre::HardwareVertexBufferSharedPtr nbuf = Ogre::HardwareBufferManager::getSingleton().
			createVertexBuffer(decl->getVertexSize(NORMAL_BINDING), ogreSubMesh->vertexData->vertexCount,
			Ogre::HardwareBuffer::HBU_DYNAMIC, false);
		Ogre::HardwareVertexBufferSharedPtr tbuf = Ogre::HardwareBufferManager::getSingleton().
			createVertexBuffer(decl->getVertexSize(TEXCOORD_BINDING), ogreSubMesh->vertexData->vertexCount,
			Ogre::HardwareBuffer::HBU_DYNAMIC, false);
		
		bind->setBinding(POSITION_BINDING, pbuf);
		bind->setBinding(NORMAL_BINDING, nbuf);
		bind->setBinding(TEXCOORD_BINDING, tbuf);

		ogreSubMesh->useSharedVertices = false;

		float* pPos = static_cast<float*>(
			pbuf->lock(Ogre::HardwareBuffer::HBL_DISCARD));
		float* pTex = static_cast<float*>(
			tbuf->lock(Ogre::HardwareBuffer::HBL_DISCARD));
		float* pNorm = static_cast<float*>(
			nbuf->lock(Ogre::HardwareBuffer::HBL_DISCARD));

		Vertices::const_iterator vertexEnd = pvVertices->end();
		Vertices::const_iterator vertexItr = pvVertices->begin();

		for(; vertexItr != vertexEnd; ++vertexItr)
		{
			// Set up position
			currpos = Ogre::Vector3(
						vertexItr->x,
						vertexItr->y,
						vertexItr->z);

			*pPos = currpos.x; ++pPos;
			*pPos = currpos.y; ++pPos;
			*pPos = currpos.z; ++pPos;

			// Deal with bounds
			if (bFirst)
			{
				min = max = currpos;
				maxSquaredRadius = currpos.squaredLength();
				bFirst = false;
			}
			else
			{
				min.makeFloor(currpos);
				max.makeCeil(currpos);
				maxSquaredRadius = std::max(maxSquaredRadius, currpos.squaredLength());
			}

			// Set up the normal
			Ogre::Vector3 normal(vertexItr->nx,vertexItr->ny, vertexItr->nz);
			normal.normalise();

			*pNorm = normal.x; ++pNorm; 
			*pNorm = normal.y; ++pNorm; 
			*pNorm = normal.z; ++pNorm; 

			// Set up texture coordinates
			*pTex = vertexItr->u; ++pTex;
			*pTex = vertexItr->v; ++pTex;
		}

		nbuf->unlock();
		tbuf->unlock();
		pbuf->unlock();

		const Faces* pvFaces = matItr->vFaces;
		ogreSubMesh->indexData->indexStart = 0;
		ogreSubMesh->indexData->indexCount = pvFaces->size();

		Ogre::HardwareIndexBufferSharedPtr ibuf;
		ibuf = Ogre::HardwareBufferManager::getSingleton().createIndexBuffer(
			Ogre::HardwareIndexBuffer::IT_16BIT,
			ogreSubMesh->indexData->indexCount,
			Ogre::HardwareBuffer::HBU_STATIC_WRITE_ONLY);
		ogreSubMesh->indexData->indexBuffer = ibuf;

		unsigned short *pShort = static_cast<unsigned short*>(
			ibuf->lock(Ogre::HardwareBuffer::HBL_DISCARD));

		Faces::const_iterator faceEnd = pvFaces->end();
		Faces::const_iterator faceItr = pvFaces->begin();

		for(; faceItr != faceEnd; ++faceItr, ++pShort)
		{
			*pShort = *faceItr;
		}

		ibuf->unlock();

		// Now use Ogre's ability to reorganize the vertex buffers the best way
		Ogre::VertexDeclaration* newDecl = 
			ogreSubMesh->vertexData->vertexDeclaration->getAutoOrganisedDeclaration(false, false);
		Ogre::BufferUsageList bufferUsages;
		for (size_t u = 0; u <= newDecl->getMaxSource(); ++u)
			bufferUsages.push_back(Ogre::HardwareBuffer::HBU_STATIC_WRITE_ONLY);
		ogreSubMesh->vertexData->reorganiseBuffers(newDecl, bufferUsages);

		g_LogMgr.logMessage(
			Ogre::StringConverter::toString(pvVertices->size()) + " vertices, "  
			+ Ogre::StringConverter::toString(pvFaces->size()/3) + " faces");
		
		ogreSubMesh->setMaterialName(matItr->name);
	
	}

	// Set bounds
	ogreMesh->_setBoundingSphereRadius(Ogre::Math::Sqrt(maxSquaredRadius));
	ogreMesh->_setBounds(Ogre::AxisAlignedBox(min, max), false);

	// Write the mesh file
	Ogre::MeshSerializer meshSer;
	meshSer.exportMesh(ogreMesh.getPointer(), Ogre::String(meshName)+".mesh");
	Ogre::MeshManager::getSingleton().remove(ogreMesh->getHandle());

	return 1;
}

void WriteTextures()
{
	//------------------------------------------------
	// extracts textures
	//------------------------------------------------
	vector<MATERIAL>::iterator matEnd = g_Materials.end();
	vector<MATERIAL>::iterator matItr = g_Materials.begin();

	for(; matItr != matEnd; ++matItr)
	{
		ExtractTextures(&(*matItr));
	}
}

int WriteMesh(const char* meshName)
{
	g_LogMgr.createLog("HOD2Mesh.log", true);
	g_LogMgr.logMessage("OGRE HOD Exporter Log");
	g_LogMgr.logMessage("---------------------------");

	Ogre::ResourceGroupManager resGrpMgr;

	WriteMaterial(meshName);

	ReassembleData();

	WriteTextures();

	WriteMeshData(meshName);

	return 1;
}