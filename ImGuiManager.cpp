#include "stdafx.h"
#include "ImGuiManager.h"
#include "RenderSystem.h"
#include "GpuProgram.h"
#include "GpuBuffer.h"

ImGuiManager gImGuiManager;

// imgui specific data size constants
const unsigned int Sizeof_ImGuiVertex = sizeof(ImDrawVert);
const unsigned int Sizeof_ImGuiIndex = sizeof(ImDrawIdx);

bool ImGuiManager::Initialize()
{
    // allocate buffers
    mVertexBuffer = gGraphicsDevice.CreateBuffer();
    mIndexBuffer = gGraphicsDevice.CreateBuffer();

    // initialize imgui context
    IMGUI_CHECKVERSION();

    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    //io.IniFilename = nullptr; // disable saving state
    //io.LogFilename = nullptr; // disable saving log

    io.BackendRendererName          = "imgui_impl_opengl3";
    io.BackendFlags                 = ImGuiBackendFlags_HasMouseCursors | ImGuiBackendFlags_HasSetMousePos;
    io.ConfigFlags                  = ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableSetMousePos;
    io.KeyMap[ImGuiKey_Tab]         = KEYCODE_TAB;
    io.KeyMap[ImGuiKey_LeftArrow]   = KEYCODE_LEFT;
    io.KeyMap[ImGuiKey_RightArrow]  = KEYCODE_RIGHT;
    io.KeyMap[ImGuiKey_UpArrow]     = KEYCODE_UP;
    io.KeyMap[ImGuiKey_DownArrow]   = KEYCODE_DOWN;
    io.KeyMap[ImGuiKey_PageUp]      = KEYCODE_PAGE_UP;
    io.KeyMap[ImGuiKey_PageDown]    = KEYCODE_PAGE_DOWN;
    io.KeyMap[ImGuiKey_Home]        = KEYCODE_HOME;
    io.KeyMap[ImGuiKey_End]         = KEYCODE_END;
    io.KeyMap[ImGuiKey_Insert]      = KEYCODE_INSERT;
    io.KeyMap[ImGuiKey_Delete]      = KEYCODE_DELETE;
    io.KeyMap[ImGuiKey_Backspace]   = KEYCODE_BACKSPACE;
    io.KeyMap[ImGuiKey_Space]       = KEYCODE_SPACE;
    io.KeyMap[ImGuiKey_Enter]       = KEYCODE_ENTER;
    io.KeyMap[ImGuiKey_Escape]      = KEYCODE_ENTER;
    io.KeyMap[ImGuiKey_A]           = KEYCODE_A;
    io.KeyMap[ImGuiKey_C]           = KEYCODE_C;
    io.KeyMap[ImGuiKey_V]           = KEYCODE_V;
    io.KeyMap[ImGuiKey_X]           = KEYCODE_X;
    io.KeyMap[ImGuiKey_Y]           = KEYCODE_Y;
    io.KeyMap[ImGuiKey_Z]           = KEYCODE_Z;

    SetupStyle(io);

    int iWidth, iHeight;
    unsigned char *pcPixels;

    AddFontFromExternalFile(io, "fonts/cousine_regular.ttf", 15.0f);

    io.Fonts->Build();
    io.Fonts->GetTexDataAsRGBA32(&pcPixels, &iWidth, &iHeight);

    GpuTexture2D* fontTexture = gGraphicsDevice.CreateTexture2D(eTextureFormat_RGBA8, iWidth, iHeight, pcPixels);
    debug_assert(fontTexture);

    io.Fonts->TexID = fontTexture;
    io.MouseDrawCursor = true;
    return true;
}

void ImGuiManager::Deinit()
{
    ImGuiIO& io = ImGui::GetIO();

    // destroy font texture
    GpuTexture2D* fontTexture = static_cast<GpuTexture2D*>(io.Fonts->TexID);
    if (fontTexture)
    {
        gGraphicsDevice.DestroyTexture2D(fontTexture);
        io.Fonts->TexID = nullptr;
    }

    ImGui::DestroyContext();

    if (mVertexBuffer)
    {
        gGraphicsDevice.DestroyBuffer(mVertexBuffer);
        mVertexBuffer = nullptr;
    }
    
    if (mIndexBuffer)
    {
        gGraphicsDevice.DestroyBuffer(mIndexBuffer);
        mIndexBuffer = nullptr;
    }
}
 
void ImGuiManager::RenderFrame()
{
    ImGui::EndFrame();
    ImGui::Render();

    ImDrawData* imGuiDrawData = ImGui::GetDrawData();

    // Avoid rendering when minimized, scale coordinates for retina displays (screen coordinates != framebuffer coordinates)
    int fb_width = (int)(imGuiDrawData->DisplaySize.x * imGuiDrawData->FramebufferScale.x);
    int fb_height = (int)(imGuiDrawData->DisplaySize.y * imGuiDrawData->FramebufferScale.y);
    if (fb_width <= 0 || fb_height <= 0)
        return;

    RenderStates imguiRenderStates = RenderStates()
        .Disable(RenderStateFlags_FaceCulling)
        .Disable(RenderStateFlags_DepthTest)
        .SetAlphaBlend(eBlendMode_Alpha);

    gGraphicsDevice.SetRenderStates(imguiRenderStates);

    Rect2D imguiViewportRect { 
        static_cast<int>(imGuiDrawData->DisplayPos.x), static_cast<int>(imGuiDrawData->DisplayPos.y),
        static_cast<int>(imGuiDrawData->DisplaySize.x), static_cast<int>(imGuiDrawData->DisplaySize.y) 
    };
    gGraphicsDevice.SetViewportRect(imguiViewportRect);

    // todo : enable scissors

    // compute ortho matrix
    glm::mat4 projmatrix = glm::ortho(imGuiDrawData->DisplayPos.x, imGuiDrawData->DisplayPos.x + imGuiDrawData->DisplaySize.x, 
        imGuiDrawData->DisplayPos.y + imGuiDrawData->DisplaySize.y, imGuiDrawData->DisplayPos.y);

    gRenderSystem.mGuiTexColorProgram.Activate();
    gRenderSystem.mGuiTexColorProgram.mGpuProgram->SetUniform(eRenderUniform_ViewProjectionMatrix, projmatrix);

    glEnable(GL_SCISSOR_TEST);

    ImVec2 clip_off = imGuiDrawData->DisplayPos;         // (0,0) unless using multi-viewports
    ImVec2 clip_scale = imGuiDrawData->FramebufferScale; // (1,1) unless using retina display which are often (2,2)

    // imgui primitives rendering
    for (int iCommandList = 0; iCommandList < imGuiDrawData->CmdListsCount; ++iCommandList)
    {
        const ImDrawList* cmd_list = imGuiDrawData->CmdLists[iCommandList];

         // vertex buffer generated by Dear ImGui
        mVertexBuffer->Setup(eBufferContent_Vertices, eBufferUsage_Dynamic, Sizeof_ImGuiVertex * cmd_list->VtxBuffer.Size, cmd_list->VtxBuffer.Data);
        gGraphicsDevice.BindVertexBuffer(mVertexBuffer, Vertex2D_Format::Get());

        // index buffer generated by Dear ImGui
        mIndexBuffer->Setup(eBufferContent_Indices, eBufferUsage_Dynamic, Sizeof_ImGuiIndex * cmd_list->IdxBuffer.Size, cmd_list->IdxBuffer.Data);
        gGraphicsDevice.BindIndexBuffer(mIndexBuffer);

        for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; ++cmd_i)
        {
            const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
            if (pcmd->UserCallback)
            {
                pcmd->UserCallback(cmd_list, pcmd);
                continue;
            }

            // Project scissor/clipping rectangles into framebuffer space
            ImVec4 clip_rect;
            clip_rect.x = (pcmd->ClipRect.x - clip_off.x) * clip_scale.x;
            clip_rect.y = (pcmd->ClipRect.y - clip_off.y) * clip_scale.y;
            clip_rect.z = (pcmd->ClipRect.z - clip_off.x) * clip_scale.x;
            clip_rect.w = (pcmd->ClipRect.w - clip_off.y) * clip_scale.y;

            bool should_draw = (clip_rect.x < fb_width && clip_rect.y < fb_height && clip_rect.z >= 0.0f && clip_rect.w >= 0.0f);

            if (!should_draw)
                continue;

            glScissor((int)clip_rect.x, (int)(fb_height - clip_rect.w), (int)(clip_rect.z - clip_rect.x), (int)(clip_rect.w - clip_rect.y));

            // The texture for the draw call is specified by pcmd->TextureId.
            // The vast majority of draw calls will use the Dear ImGui texture atlas, which value you have set yourself during initialization.
            GpuTexture2D* bindTexture = static_cast<GpuTexture2D*>(pcmd->TextureId);
            gGraphicsDevice.BindTexture2D(eTextureUnit_0, bindTexture);

            // We are using scissoring to clip some objects. All low-level graphics API should supports it.
            // - If your engine doesn't support scissoring yet, you may ignore this at first. You will get some small glitches
            //   (some elements visible outside their bounds) but you can fix that once everything else works!
            // - Clipping coordinates are provided in imgui coordinates space (from draw_data->DisplayPos to draw_data->DisplayPos + draw_data->DisplaySize)
            //   In a single viewport application, draw_data->DisplayPos will always be (0,0) and draw_data->DisplaySize will always be == io.DisplaySize.
            //   However, in the interest of supporting multi-viewport applications in the future (see 'viewport' branch on github),
            //   always subtract draw_data->DisplayPos from clipping bounds to convert them to your viewport space.
            // - Note that pcmd->ClipRect contains Min+Max bounds. Some graphics API may use Min+Max, other may use Min+Size (size being Max-Min)

            //ImVec2 pos = imGuiDrawData->DisplayPos;
            //MyEngineScissor((int)(pcmd->ClipRect.x - pos.x), (int)(pcmd->ClipRect.y - pos.y), (int)(pcmd->ClipRect.z - pos.x), (int)(pcmd->ClipRect.w - pos.y));

            // Render 'pcmd->ElemCount/3' indexed triangles.
            // By default the indices ImDrawIdx are 16-bits, you can change them to 32-bits in imconfig.h if your engine doesn't support 16-bits indices.

            eIndicesType indicesType = Sizeof_ImGuiIndex == 2 ? eIndicesType_i16 : eIndicesType_i32;
            gGraphicsDevice.RenderIndexedPrimitives(ePrimitiveType_Triangles, indicesType, Sizeof_ImGuiIndex * pcmd->IdxOffset, pcmd->ElemCount);
        }
    }
}

void ImGuiManager::UpdateFrame(Timespan deltaTime)
{
    ImGuiIO& io = ImGui::GetIO();

    io.DeltaTime = deltaTime.ToSeconds();   // set the time elapsed since the previous frame (in seconds)
    io.DisplaySize.x = gGraphicsDevice.mViewportRect.w * 1.0f;
    io.DisplaySize.y = gGraphicsDevice.mViewportRect.h * 1.0f;
    io.MousePos.x = gInputs.mCursorPositionX * 1.0f;
    io.MousePos.y = gInputs.mCursorPositionY * 1.0f;
    io.MouseDown[0] = gInputs.GetMouseButtonL();  // set the mouse button states
    io.MouseDown[1] = gInputs.GetMouseButtonR();
    io.MouseDown[3] = gInputs.GetMouseButtonM();

    ImGui::NewFrame();

    // todo: process all imgui windows

    ImGui::ShowDemoWindow();
    ImGui::ShowMetricsWindow();
}

void ImGuiManager::HandleEvent(MouseMovedInputEvent& inputEvent)
{
}

void ImGuiManager::HandleEvent(MouseScrollInputEvent& inputEvent)
{
    ImGuiIO& io = ImGui::GetIO();

    io.MouseWheelH += inputEvent.mScrollX * 1.0f;
    io.MouseWheel += inputEvent.mScrollY * 1.0f;
}

void ImGuiManager::HandleEvent(MouseButtonInputEvent& inputEvent)
{
}

void ImGuiManager::HandleEvent(KeyInputEvent& inputEvent)
{
    ImGuiIO& io = ImGui::GetIO();

    io.KeysDown[inputEvent.mKeycode] = inputEvent.mPressed;
    io.KeyCtrl = inputEvent.HasMods(KEYMOD_CTRL);
    io.KeyShift = inputEvent.HasMods(KEYMOD_SHIFT);
    io.KeyAlt = inputEvent.HasMods(KEYMOD_ALT);
}

void ImGuiManager::HandleEvent(KeyCharEvent& inputEvent)
{
    ImGuiIO& io = ImGui::GetIO();

    io.AddInputCharacter(inputEvent.mUnicodeChar);
}

bool ImGuiManager::AddFontFromExternalFile(ImGuiIO& imguiIO, const char* fontFile, float fontSize)
{
    std::string fullFontPath;
    if (!gFiles.GetFullPathToFile(fontFile, fullFontPath))
        return false;

    ImFont* imfont = imguiIO.Fonts->AddFontFromFileTTF(fullFontPath.c_str(), fontSize);
    debug_assert(imfont);

    return imfont != nullptr;
}

void ImGuiManager::SetupStyle(ImGuiIO& imguiIO)
{
	ImGuiStyle & style = ImGui::GetStyle();
	ImVec4 * colors = style.Colors;
	
	/// 0 = FLAT APPEARENCE
	/// 1 = MORE "3D" LOOK
	int is3D = 1;
		
	colors[ImGuiCol_Text]                   = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
	colors[ImGuiCol_TextDisabled]           = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);
	colors[ImGuiCol_ChildBg]                = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
	colors[ImGuiCol_WindowBg]               = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
	colors[ImGuiCol_PopupBg]                = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
	colors[ImGuiCol_Border]                 = ImVec4(0.12f, 0.12f, 0.12f, 0.71f);
	colors[ImGuiCol_BorderShadow]           = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);
	colors[ImGuiCol_FrameBg]                = ImVec4(0.42f, 0.42f, 0.42f, 0.54f);
	colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.42f, 0.42f, 0.42f, 0.40f);
	colors[ImGuiCol_FrameBgActive]          = ImVec4(0.56f, 0.56f, 0.56f, 0.67f);
	colors[ImGuiCol_TitleBg]                = ImVec4(0.19f, 0.19f, 0.19f, 1.00f);
	colors[ImGuiCol_TitleBgActive]          = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
	colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.17f, 0.17f, 0.17f, 0.90f);
	colors[ImGuiCol_MenuBarBg]              = ImVec4(0.335f, 0.335f, 0.335f, 1.000f);
	colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.24f, 0.24f, 0.24f, 0.53f);
	colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
	colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.52f, 0.52f, 0.52f, 1.00f);
	colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.76f, 0.76f, 0.76f, 1.00f);
	colors[ImGuiCol_CheckMark]              = ImVec4(0.65f, 0.65f, 0.65f, 1.00f);
	colors[ImGuiCol_SliderGrab]             = ImVec4(0.52f, 0.52f, 0.52f, 1.00f);
	colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.64f, 0.64f, 0.64f, 1.00f);
	colors[ImGuiCol_Button]                 = ImVec4(0.54f, 0.54f, 0.54f, 0.35f);
	colors[ImGuiCol_ButtonHovered]          = ImVec4(0.52f, 0.52f, 0.52f, 0.59f);
	colors[ImGuiCol_ButtonActive]           = ImVec4(0.76f, 0.76f, 0.76f, 1.00f);
	colors[ImGuiCol_Header]                 = ImVec4(0.38f, 0.38f, 0.38f, 1.00f);
	colors[ImGuiCol_HeaderHovered]          = ImVec4(0.47f, 0.47f, 0.47f, 1.00f);
	colors[ImGuiCol_HeaderActive]           = ImVec4(0.76f, 0.76f, 0.76f, 0.77f);
	colors[ImGuiCol_Separator]              = ImVec4(0.000f, 0.000f, 0.000f, 0.137f);
	colors[ImGuiCol_SeparatorHovered]       = ImVec4(0.700f, 0.671f, 0.600f, 0.290f);
	colors[ImGuiCol_SeparatorActive]        = ImVec4(0.702f, 0.671f, 0.600f, 0.674f);
	colors[ImGuiCol_ResizeGrip]             = ImVec4(0.26f, 0.59f, 0.98f, 0.25f);
	colors[ImGuiCol_ResizeGripHovered]      = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
	colors[ImGuiCol_ResizeGripActive]       = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
	colors[ImGuiCol_PlotLines]              = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
	colors[ImGuiCol_PlotLinesHovered]       = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
	colors[ImGuiCol_PlotHistogram]          = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
	colors[ImGuiCol_PlotHistogramHovered]   = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
	colors[ImGuiCol_TextSelectedBg]         = ImVec4(0.73f, 0.73f, 0.73f, 0.35f);
	colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);
	colors[ImGuiCol_DragDropTarget]         = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
	colors[ImGuiCol_NavHighlight]           = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	colors[ImGuiCol_NavWindowingHighlight]  = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
	colors[ImGuiCol_NavWindowingDimBg]      = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);

	style.PopupRounding = 3;

	style.WindowPadding = ImVec2(4, 4);
	style.FramePadding  = ImVec2(6, 4);
	style.ItemSpacing   = ImVec2(6, 2);

	style.ScrollbarSize = 18;

	style.WindowBorderSize = 1;
	style.ChildBorderSize  = 1;
	style.PopupBorderSize  = 1;
	style.FrameBorderSize  = is3D * 1.0f; 

	style.WindowRounding    = 3;
	style.ChildRounding     = 3;
	style.FrameRounding     = 3;
	style.ScrollbarRounding = 2;
	style.GrabRounding      = 3;

	#ifdef IMGUI_HAS_DOCK 
		style.TabBorderSize = is3D; 
		style.TabRounding   = 3;

		colors[ImGuiCol_DockingEmptyBg]     = ImVec4(0.38f, 0.38f, 0.38f, 1.00f);
		colors[ImGuiCol_Tab]                = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
		colors[ImGuiCol_TabHovered]         = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);
		colors[ImGuiCol_TabActive]          = ImVec4(0.33f, 0.33f, 0.33f, 1.00f);
		colors[ImGuiCol_TabUnfocused]       = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
		colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.33f, 0.33f, 0.33f, 1.00f);
		colors[ImGuiCol_DockingPreview]     = ImVec4(0.85f, 0.85f, 0.85f, 0.28f);

		if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			style.WindowRounding = 0.0f;
			style.Colors[ImGuiCol_WindowBg].w = 1.0f;
		}
	#endif
}