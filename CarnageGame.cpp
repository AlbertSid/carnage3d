#include "stdafx.h"
#include "CarnageGame.h"

CarnageGame gCarnageGame;

bool CarnageGame::Initialize()
{

    Bitmap bmp;
    bool isSuccess = bmp.LoadFromFile("textures/flowey.png");

    debug_assert(isSuccess);
    return true;
}

void CarnageGame::Deinit()
{
}

void CarnageGame::UpdateFrame(Timespan deltaTime)
{
}

void CarnageGame::InputEvent(KeyInputEvent& inputEvent)
{
}

void CarnageGame::InputEvent(MouseButtonInputEvent& inputEvent)
{
}

void CarnageGame::InputEvent(MouseMovedInputEvent& inputEvent)
{
}

void CarnageGame::InputEvent(MouseScrollInputEvent& inputEvent)
{
}

void CarnageGame::InputEvent(KeyCharEvent& inputEvent)
{
}