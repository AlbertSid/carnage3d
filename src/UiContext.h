#pragma once

class GameCamera2D;
class SpriteBatch;

class UiContext
{
public:
    GameCamera2D& mCamera;
    SpriteBatch& mSpriteBatch;

public:
    UiContext(GameCamera2D& camera, SpriteBatch& spriteBatch)
        : mCamera(camera)
        , mSpriteBatch(spriteBatch)
    {
    }

    // helpers
    inline int GetScreenSizex() const { return mCamera.mViewportRect.w; }
    inline int GetScreenSizey() const { return mCamera.mViewportRect.h; }

    // convert
    inline void NormalizedToScreenPoint(float xcoord, float ycoord, int& outx, int& outy) const
    {
        outx = static_cast<int>(mCamera.mViewportRect.w * xcoord);
        outy = static_cast<int>(mCamera.mViewportRect.h * ycoord);
    }
    inline void ScreenPointToNormalized(int xcoord, int ycoord, float& outx, float& outy) const
    {
        debug_assert(mCamera.mViewportRect.w);
        debug_assert(mCamera.mViewportRect.h);

        outx = (xcoord * 1.0f) / mCamera.mViewportRect.w;
        outy = (ycoord * 1.0f) / mCamera.mViewportRect.h;
    }
};