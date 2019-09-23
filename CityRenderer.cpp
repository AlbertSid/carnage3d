#include "stdafx.h"
#include "CityRenderer.h"
#include "RenderingManager.h"
#include "GpuBuffer.h"
#include "CarnageGame.h"
#include "SpriteManager.h"
#include "GpuTexture2D.h"
#include "GameCheatsWindow.h"
#include "PhysicsObject.h"
#include "PhysicsManager.h"

const unsigned int NumVerticesPerSprite = 4;
const unsigned int NumIndicesPerSprite = 6;

CityRenderer::CityRenderer()
{
    mDrawSpritesList.reserve(2048);
}

bool CityRenderer::Initialize()
{
    mCityMeshBufferV = gGraphicsDevice.CreateBuffer(eBufferContent_Vertices);
    debug_assert(mCityMeshBufferV);

    mCityMeshBufferI = gGraphicsDevice.CreateBuffer(eBufferContent_Indices);
    debug_assert(mCityMeshBufferI);

    if (mCityMeshBufferV == nullptr || mCityMeshBufferI == nullptr)
        return false;

    if (!mSpritesVertexCache.Initialize())
    {
        gConsole.LogMessage(eLogMessage_Warning, "Cannot initialize sprites vertex cache");
        return false;
    }

    mCityMapRectangle.SetNull();
    return true;
}

void CityRenderer::Deinit()
{
    mSpritesVertexCache.Deinit();
    if (mCityMeshBufferV)
    {
        gGraphicsDevice.DestroyBuffer(mCityMeshBufferV);
        mCityMeshBufferV = nullptr;
    }

    if (mCityMeshBufferI)
    {
        gGraphicsDevice.DestroyBuffer(mCityMeshBufferI);
        mCityMeshBufferI = nullptr;
    }

    for (int iLayer = 0; iLayer < MAP_LAYERS_COUNT; ++iLayer)
    {
        mCityMeshData[iLayer].SetNull();
    }
    mCityMapRectangle.SetNull();
}

void CityRenderer::RenderFrame()
{
    BuildMapMesh();

    RenderFrameBegin();

    IssuePedsSprites();
    IssueMapObjectsSprites();
    IssueCarsSprites();
    IssueProjectilesSprites();

    if (!mDrawSpritesList.empty())
    {
        SortDrawSpritesList();
        GenerateDrawSpritesBatches();
        RenderDrawSpritesBatches();
    }

    DrawCityMesh();
    RenderFrameEnd();
}

void CityRenderer::CommitCityMeshData()
{
    int totalIndexCount = 0;
    int totalVertexCount = 0;

    for (int iLayer = 0; iLayer < MAP_LAYERS_COUNT; ++iLayer)
    {
        int numVertices = mCityMeshData[iLayer].mBlocksVertices.size();
        totalVertexCount += numVertices;

        int numIndices = mCityMeshData[iLayer].mBlocksIndices.size();
        totalIndexCount += numIndices;
    }

    if (totalIndexCount == 0 || totalVertexCount == 0)
        return;

    int totalIndexDataBytes = totalIndexCount * Sizeof_DrawIndex_t;
    int totalVertexDataBytes = totalVertexCount * Sizeof_CityVertex3D;

    // upload vertex data
    mCityMeshBufferV->Setup(eBufferUsage_Static, totalVertexDataBytes, nullptr);
    if (void* pdata = mCityMeshBufferV->Lock(BufferAccess_Write))
    {
        char* pcursor = static_cast<char*>(pdata);
        for (int iLayer = 0; iLayer < MAP_LAYERS_COUNT; ++iLayer)
        {
            int dataLength = mCityMeshData[iLayer].mBlocksVertices.size() * Sizeof_CityVertex3D;
            if (mCityMeshData[iLayer].mBlocksVertices.empty())
                continue;
            memcpy(pcursor, mCityMeshData[iLayer].mBlocksVertices.data(), dataLength);
            pcursor += dataLength;
        }
        mCityMeshBufferV->Unlock();
    }

    // upload index data
    mCityMeshBufferI->Setup(eBufferUsage_Static, totalIndexDataBytes, nullptr);
    if (void* pdata = mCityMeshBufferI->Lock(BufferAccess_Write))
    {
        char* pcursor = static_cast<char*>(pdata);
        for (int iLayer = 0; iLayer < MAP_LAYERS_COUNT; ++iLayer)
        {
            int dataLength = mCityMeshData[iLayer].mBlocksIndices.size() * Sizeof_DrawIndex_t;
            if (mCityMeshData[iLayer].mBlocksIndices.empty())
                continue;
            memcpy(pcursor, mCityMeshData[iLayer].mBlocksIndices.data(), dataLength);
            pcursor += dataLength;
        }
        mCityMeshBufferI->Unlock();
    }
}

void CityRenderer::BuildMapMesh()
{
    if (gGameCheatsWindow.mGenerateFullMeshForMap && mCityMapRectangle.h == 0 && mCityMapRectangle.w == 0)
    {
        mCityMapRectangle.x = 0;
        mCityMapRectangle.y = 0;
        mCityMapRectangle.w = MAP_DIMENSIONS;
        mCityMapRectangle.h = MAP_DIMENSIONS;  

        gConsole.LogMessage(eLogMessage_Debug, "City mesh invalidated [full]");
        for (int i = 0; i < MAP_LAYERS_COUNT; ++i)
        {
            GameMapHelpers::BuildMapMesh(gGameMap, mCityMapRectangle, i, gRenderManager.mCityRenderer.mCityMeshData[i]);
        }
        gRenderManager.mCityRenderer.CommitCityMeshData();
        return;
    }

    int viewBlocks = 14;

    int tilex = static_cast<int>(gCamera.mPosition.x / MAP_BLOCK_LENGTH);
    int tiley = static_cast<int>(gCamera.mPosition.z / MAP_BLOCK_LENGTH);

    Rect2D rcMapView { -viewBlocks / 2 + tilex, -viewBlocks / 2 + tiley, viewBlocks, viewBlocks };

    bool invalidateCache = mCityMapRectangle.w == 0 || mCityMapRectangle.h == 0 || 
        rcMapView.x < mCityMapRectangle.x || rcMapView.y < mCityMapRectangle.y ||
        rcMapView.x + rcMapView.w > mCityMapRectangle.x + mCityMapRectangle.w ||
        rcMapView.y + rcMapView.h > mCityMapRectangle.y + mCityMapRectangle.h;

    if (!invalidateCache)
        return;

    gConsole.LogMessage(eLogMessage_Debug, "City mesh invalidated [partial]");

    int cacheNumBlocks = 32;
    mCityMapRectangle.x = (-cacheNumBlocks / 2) + tilex;
    mCityMapRectangle.y = (-cacheNumBlocks / 2) + tiley;
    mCityMapRectangle.w = cacheNumBlocks;
    mCityMapRectangle.h = cacheNumBlocks;

    for (int i = 0; i < MAP_LAYERS_COUNT; ++i)
    {
        GameMapHelpers::BuildMapMesh(gGameMap, mCityMapRectangle, i, gRenderManager.mCityRenderer.mCityMeshData[i]);
    }
    gRenderManager.mCityRenderer.CommitCityMeshData();
}

void CityRenderer::RenderFrameBegin()
{
}

void CityRenderer::RenderFrameEnd()
{
    mSpritesVertexCache.FlushCache();
    mDrawSpritesList.clear();
    mDrawSpritesBatchesList.clear();
    mDrawSpritesVertices.clear();
    mDrawSpritesIndices.clear();
}

void CityRenderer::DrawCityMesh()
{
    RenderStates cityMeshRenderStates;

    gGraphicsDevice.SetRenderStates(cityMeshRenderStates);

    gRenderManager.mCityMeshProgram.Activate();
    gRenderManager.mCityMeshProgram.UploadCameraTransformMatrices();
    gRenderManager.mCityMeshProgram.SetTextureMappingEnabled(gSpriteManager.mBlocksTextureArray != nullptr);

    if (mCityMeshBufferV && mCityMeshBufferI)
    {
        gGraphicsDevice.BindVertexBuffer(mCityMeshBufferV, CityVertex3D_Format::Get());
        gGraphicsDevice.BindIndexBuffer(mCityMeshBufferI);
        gGraphicsDevice.BindTexture(eTextureUnit_0, gSpriteManager.mBlocksTextureArray);
        gGraphicsDevice.BindTexture(eTextureUnit_1, gSpriteManager.mBlocksIndicesTable);

        int currBaseVertex = 0;
        int currIndexOffset = 0;
        
        for (int i = 0; i < MAP_LAYERS_COUNT; ++i)
        {
            int numIndices = mCityMeshData[i].mBlocksIndices.size();
            int numVertices = mCityMeshData[i].mBlocksVertices.size();
            if (gGameCheatsWindow.mDrawMapLayers[i])
            {
                int currIndexOffsetBytes = currIndexOffset * Sizeof_DrawIndex_t;
                gGraphicsDevice.RenderIndexedPrimitives(ePrimitiveType_Triangles, eIndicesType_i32, currIndexOffsetBytes, numIndices, currBaseVertex);
            }
            currIndexOffset += numIndices;
            currBaseVertex += numVertices;
        }
    }
    gRenderManager.mCityMeshProgram.Deactivate();
}

void CityRenderer::DrawSprite(GpuTexture2D* texture, const Rect2D& rc, const glm::vec3& position, bool centerOrigin, float sprScale, float heading)
{
    float tinvx = 1.0f / texture->mSize.x;
    float tinvy = 1.0f / texture->mSize.y;

    // setup draw sprite record
    DrawSpriteRec rec;
        rec.mPosition = position;
        rec.mSize.x = rc.w * sprScale;
        rec.mSize.y = rc.h * sprScale;
        rec.mCenterOffset.x = centerOrigin ? (-rec.mSize.x * 0.5f) : 0.0f;
        rec.mCenterOffset.y = centerOrigin ? (-rec.mSize.y * 0.5f) : 0.0f;
        rec.mTcUv0.x = rc.x * tinvx;
        rec.mTcUv0.y = rc.y * tinvy;
        rec.mTcUv1.x = (rc.x + rc.w) * tinvx;
        rec.mTcUv1.y = (rc.y + rc.h) * tinvy;
        rec.mRotate = heading;
        rec.mSpriteTexture = texture;
    mDrawSpritesList.push_back(rec);
}

void CityRenderer::IssuePedsSprites()
{
    float spriteScale = (1.0f / MAP_PIXELS_PER_TILE);
    for (Pedestrian* currPedestrian: gCarnageGame.mPedsManager.mActivePedestriansList)
    {
        int spriteLinearIndex = gGameMap.mStyleData.GetSpriteIndex(eSpriteType_Ped, currPedestrian->mAnimation.mCurrentFrame);
        
        float rotationAngle = glm::radians(currPedestrian->mPhysicalBody->GetAngleDegrees() - SPRITE_ZERO_ANGLE);

        glm::vec3 position = currPedestrian->mPhysicalBody->GetPosition();
        position.y = ComputeDrawHeight(currPedestrian, position, rotationAngle);

        DrawSprite(gSpriteManager.mObjectsSpritesheet.mSpritesheetTexture, 
            gSpriteManager.mObjectsSpritesheet.mEtries[spriteLinearIndex].mRectangle, position, true, spriteScale, rotationAngle);
    }
}

void CityRenderer::IssueCarsSprites()
{
    float spriteScale = (1.0f / MAP_PIXELS_PER_TILE);
    for (Vehicle* currVehicle: gCarnageGame.mCarsManager.mActiveCarsList)
    {
        int spriteLinearIndex = gGameMap.mStyleData.GetCarSpriteIndex(currVehicle->mCarStyle->mVType, 
            currVehicle->mCarStyle->mModel, 
            currVehicle->mCarStyle->mSprNum);
        
        float rotationAngle = glm::radians(currVehicle->mPhysicalBody->GetAngleDegrees() - SPRITE_ZERO_ANGLE);

        glm::vec3 position = currVehicle->mPhysicalBody->GetPosition();
        position.y = ComputeDrawHeight(currVehicle, position, rotationAngle);
        DrawSprite(gSpriteManager.mObjectsSpritesheet.mSpritesheetTexture, 
            gSpriteManager.mObjectsSpritesheet.mEtries[spriteLinearIndex].mRectangle, position, true, spriteScale, rotationAngle);
    }
}

void CityRenderer::IssueMapObjectsSprites()
{
}

void CityRenderer::IssueProjectilesSprites()
{
}

void CityRenderer::SortDrawSpritesList()
{
    std::sort(mDrawSpritesList.begin(), mDrawSpritesList.end(), [](const DrawSpriteRec& lhs, const DrawSpriteRec& rhs)
        {
            return lhs.mSpriteTexture < rhs.mSpriteTexture;
        });
}

void CityRenderer::GenerateDrawSpritesBatches()
{
    int numSprites = mDrawSpritesList.size();

    int totalVertexCount = numSprites * NumVerticesPerSprite; 
    debug_assert(totalVertexCount > 0);

    int totalIndexCount = numSprites * NumIndicesPerSprite; 
    debug_assert(totalIndexCount > 0);

    // allocate memory for mesh data
    mDrawSpritesVertices.resize(totalVertexCount);
    SpriteVertex3D* vertexData = mDrawSpritesVertices.data();

    mDrawSpritesIndices.resize(totalIndexCount);
    DrawIndex_t* indexData = mDrawSpritesIndices.data();

    // initial batch
    mDrawSpritesBatchesList.clear();
    mDrawSpritesBatchesList.emplace_back();
    DrawSpriteBatch* currentBatch = &mDrawSpritesBatchesList.back();
    currentBatch->mFirstVertex = 0;
    currentBatch->mFirstIndex = 0;
    currentBatch->mVertexCount = 0;
    currentBatch->mIndexCount = 0;
    currentBatch->mSpriteTexture = mDrawSpritesList[0].mSpriteTexture;

    for (int isprite = 0; isprite < numSprites; ++isprite)
    {
        const DrawSpriteRec& sprite = mDrawSpritesList[isprite];
        // start new batch
        if (sprite.mSpriteTexture != currentBatch->mSpriteTexture)
        {
            DrawSpriteBatch newBatch;
            newBatch.mFirstVertex = currentBatch->mVertexCount;
            newBatch.mFirstIndex = currentBatch->mIndexCount;
            newBatch.mVertexCount = 0;
            newBatch.mIndexCount = 0;
            newBatch.mSpriteTexture = sprite.mSpriteTexture;
            mDrawSpritesBatchesList.push_back(newBatch);
            currentBatch = &mDrawSpritesBatchesList.back();
        }

        currentBatch->mVertexCount += NumVerticesPerSprite;   
        currentBatch->mIndexCount += NumIndicesPerSprite;

        int vertexOffset = isprite * NumVerticesPerSprite;

        vertexData[vertexOffset + 0].mTexcoord.x = sprite.mTcUv0.x;
        vertexData[vertexOffset + 0].mTexcoord.y = sprite.mTcUv0.y;
        vertexData[vertexOffset + 0].mPosition.y = sprite.mPosition.y;

        vertexData[vertexOffset + 1].mTexcoord.x = sprite.mTcUv1.x;
        vertexData[vertexOffset + 1].mTexcoord.y = sprite.mTcUv0.y;
        vertexData[vertexOffset + 1].mPosition.y = sprite.mPosition.y;

        vertexData[vertexOffset + 2].mTexcoord.x = sprite.mTcUv0.x;
        vertexData[vertexOffset + 2].mTexcoord.y = sprite.mTcUv1.y;
        vertexData[vertexOffset + 2].mPosition.y = sprite.mPosition.y;

        vertexData[vertexOffset + 3].mTexcoord.x = sprite.mTcUv1.x;
        vertexData[vertexOffset + 3].mTexcoord.y = sprite.mTcUv1.y;
        vertexData[vertexOffset + 3].mPosition.y = sprite.mPosition.y;

        const glm::vec2 positions[4] = 
        {
            {sprite.mPosition.x + sprite.mCenterOffset.x,                   sprite.mPosition.z + sprite.mCenterOffset.y},
            {sprite.mPosition.x + sprite.mSize.x + sprite.mCenterOffset.x,  sprite.mPosition.z + sprite.mCenterOffset.y},
            {sprite.mPosition.x + sprite.mCenterOffset.x,                   sprite.mPosition.z + sprite.mSize.y + sprite.mCenterOffset.y},
            {sprite.mPosition.x + sprite.mSize.x + sprite.mCenterOffset.x,  sprite.mPosition.z + sprite.mSize.y + sprite.mCenterOffset.y},
        };
        glm::vec2 rotateCenter { sprite.mPosition.x, sprite.mPosition.z };
        if (fabs(sprite.mRotate) > 0.01f) // has rotation
        {
            for (int i = 0; i < 4; ++i)
            {
                glm::vec2 currPos = cxx::rotate_around_center(positions[i], rotateCenter, sprite.mRotate);

                vertexData[vertexOffset + i].mPosition.x = currPos.x;
                vertexData[vertexOffset + i].mPosition.z = currPos.y;
            }
        }
        else // no rotation
        {
            for (int i = 0; i < 4; ++i)
            {
                vertexData[vertexOffset + i].mPosition.x = positions[i].x;
                vertexData[vertexOffset + i].mPosition.z = positions[i].y;
            }
        }

        // setup indices
        int indexOffset = isprite * NumIndicesPerSprite;
        indexData[indexOffset + 0] = vertexOffset + 0;
        indexData[indexOffset + 1] = vertexOffset + 1;
        indexData[indexOffset + 2] = vertexOffset + 2;
        indexData[indexOffset + 3] = vertexOffset + 1;
        indexData[indexOffset + 4] = vertexOffset + 2;
        indexData[indexOffset + 5] = vertexOffset + 3;
    }
}

void CityRenderer::RenderDrawSpritesBatches()
{
    RenderStates cityMeshRenderStates;
    cityMeshRenderStates.Disable(RenderStateFlags_FaceCulling);
    gGraphicsDevice.SetRenderStates(cityMeshRenderStates);

    gRenderManager.mSpritesProgram.Activate();
    gRenderManager.mSpritesProgram.UploadCameraTransformMatrices();

    GpuTextureArray2D* blocksTextureArray = gSpriteManager.mBlocksTextureArray;
    gRenderManager.mSpritesProgram.SetTextureMappingEnabled(blocksTextureArray != nullptr);

    TransientBuffer vBuffer;
    TransientBuffer iBuffer;
    if (!mSpritesVertexCache.AllocVertex(Sizeof_SpriteVertex3D * mDrawSpritesVertices.size(), mDrawSpritesVertices.data(), vBuffer))
    {
        debug_assert(false);
        return;
    }

    if (!mSpritesVertexCache.AllocIndex(Sizeof_DrawIndex_t * mDrawSpritesIndices.size(), mDrawSpritesIndices.data(), iBuffer))
    {
        debug_assert(false);
        return;
    }

    SpriteVertex3D_Format vFormat;
    vFormat.mBaseOffset = vBuffer.mBufferDataOffset;

    gGraphicsDevice.BindVertexBuffer(vBuffer.mGraphicsBuffer, vFormat);
    gGraphicsDevice.BindIndexBuffer(iBuffer.mGraphicsBuffer);

    for (const DrawSpriteBatch& currBatch: mDrawSpritesBatchesList)
    {
        gGraphicsDevice.BindTexture(eTextureUnit_0, currBatch.mSpriteTexture);

        unsigned int idxBufferOffset = iBuffer.mBufferDataOffset + Sizeof_DrawIndex_t * currBatch.mFirstIndex;
        gGraphicsDevice.RenderIndexedPrimitives(ePrimitiveType_Triangles, eIndicesType_i32, idxBufferOffset, currBatch.mIndexCount);
    }

    gRenderManager.mSpritesProgram.Deactivate();
}

void CityRenderer::InvalidateMapMesh()
{
    mCityMapRectangle.SetNull();
}

float CityRenderer::ComputeDrawHeight(Pedestrian* pedestrian, const glm::vec3& position, float angleRadians)
{
    float halfBox = PED_SPRITE_DRAW_BOX_SIZE * 0.5f;

    //glm::vec3 points[4] = {
    //    { 0.0f,     position.y + 0.01f, -halfBox },
    //    { halfBox,  position.y + 0.01f, 0.0f },
    //    { 0.0f,     position.y + 0.01f, halfBox },
    //    { -halfBox, position.y + 0.01f, 0.0f },
    //};

    glm::vec3 points[4] = {
        { -halfBox, position.y + 0.01f, -halfBox },
        { halfBox,  position.y + 0.01f, -halfBox },
        { halfBox,  position.y + 0.01f, halfBox },
        { -halfBox, position.y + 0.01f, halfBox },
    };

    float maxHeight = position.y;
    for (glm::vec3& currPoint: points)
    {
        //currPoint = glm::rotate(currPoint, angleRadians, glm::vec3(0.0f, -1.0f, 0.0f)); // dont rotate for peds
        currPoint.x += position.x;
        currPoint.z += position.z;

        // get height
        float height = gGameMap.GetHeightAtPosition(currPoint);
        if (height > maxHeight)
        {
            maxHeight = height;
        }
    }
#if 1
    // debug
    for (int i = 0; i < 4; ++i)
    {
        gRenderManager.mDebugRenderer.DrawLine(points[i], points[(i + 1) % 4], COLOR_RED);
    }
#endif
    return maxHeight + 0.01f;
}

float CityRenderer::ComputeDrawHeight(Vehicle* car, const glm::vec3& position, float angleRadians)
{
    float halfW = (1.0f * car->mCarStyle->mWidth) / MAP_BLOCK_TEXTURE_DIMS * 0.5f;
    float halfH = (1.0f * car->mCarStyle->mHeight) / MAP_BLOCK_TEXTURE_DIMS * 0.5f;

    glm::vec3 points[4] = {
        { -halfW, position.y + 0.01f, -halfH },
        { halfW,  position.y + 0.01f, -halfH },
        { halfW,  position.y + 0.01f, halfH },
        { -halfW, position.y + 0.01f, halfH },
    };

    float maxHeight = position.y;
    for (glm::vec3& currPoint: points)
    {
        currPoint = glm::rotate(currPoint, angleRadians, glm::vec3(0.0f, -1.0f, 0.0f));
        currPoint.x += position.x;
        currPoint.z += position.z;
    }
#if 1
    // debug
    for (int i = 0; i < 4; ++i)
    {
        gRenderManager.mDebugRenderer.DrawLine(points[i], points[(i + 1) % 4], COLOR_RED);
    }
#endif
    return position.y + 0.02f;
}
