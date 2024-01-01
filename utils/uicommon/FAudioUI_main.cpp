/* FAudio - XAudio Reimplementation for FNA
 *
 * Copyright (c) 2011-2024 Ethan Lee, Luigi Auriemma, and the MonoGame Team
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * In no event will the authors be held liable for any damages arising from
 * the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 * claim that you wrote the original software. If you use this software in a
 * product, an acknowledgment in the product documentation would be
 * appreciated but is not required.
 *
 * 2. Altered source versions must be plainly marked as such, and must not be
 * misrepresented as being the original software.
 *
 * 3. This notice may not be removed or altered from any source distribution.
 *
 * Ethan "flibitijibibo" Lee <flibitijibibo@flibitijibibo.com>
 *
 */

/* Unless you're trying to do SDL/OpenGL work, you probably don't want this!
 * Go to the other folders to look at the actual tools.
 * -flibit
 */

#include <stdint.h>
#include <SDL.h>
#include "imgui.h"

/* Defined by the tools using this UI framework */

extern const char* TOOL_NAME;
extern int TOOL_WIDTH;
extern int TOOL_HEIGHT;
extern void FAudioTool_Update();
extern void FAudioTool_Quit();

/* ImGui Callbacks */

static void RenderDrawLists(ImDrawData *draw_data)
{
	ImGuiIO& io = ImGui::GetIO();
	SDL_Renderer *renderer = (SDL_Renderer*) io.BackendRendererUserData;
	SDL_Rect rect;

	/* Set up viewport/scissor rects (based on display size/scale */
	rect.x = 0;
	rect.y = 0;
	rect.w = (int) (io.DisplaySize.x * io.DisplayFramebufferScale.x);
	rect.h = (int) (io.DisplaySize.y * io.DisplayFramebufferScale.y);
	if (rect.w == 0 || rect.h == 0)
	{
		/* No point in rendering to nowhere... */
		return;
	}
	draw_data->ScaleClipRects(io.DisplayFramebufferScale);
	SDL_RenderSetViewport(renderer, &rect);

	/* Submit draw commands */
	#define OFFSETOF(TYPE, ELEMENT) ((size_t) &(((TYPE*) NULL)->ELEMENT))
	for (int cmd_l = 0; cmd_l < draw_data->CmdListsCount; cmd_l += 1)
	{
		const ImDrawList* cmd_list = draw_data->CmdLists[cmd_l];
		const ImDrawVert* vtx_buffer = cmd_list->VtxBuffer.Data;
		const ImDrawIdx* idx_buffer = cmd_list->IdxBuffer.Data;

		for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i += 1)
		{
			const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
			const char *vtx = (const char*) (vtx_buffer + pcmd->VtxOffset);
			const char *xy = vtx + OFFSETOF(ImDrawVert, pos);
			const char *uv = vtx + OFFSETOF(ImDrawVert, uv);
			const char *cl = vtx + OFFSETOF(ImDrawVert, col);

			rect.x = (int) pcmd->ClipRect.x;
			rect.y = (int) pcmd->ClipRect.y;
			rect.w = (int) (pcmd->ClipRect.z - pcmd->ClipRect.x);
			rect.h = (int) (pcmd->ClipRect.w - pcmd->ClipRect.y);
			SDL_RenderSetClipRect(renderer, &rect);

			SDL_RenderGeometryRaw(
				renderer,
				(SDL_Texture*) pcmd->TextureId,
				(const float*) xy,
				(int) sizeof(ImDrawVert),
				(const SDL_Color*) cl,
				(int) sizeof(ImDrawVert),
				(const float*) uv,
				(int) sizeof(ImDrawVert),
				cmd_list->VtxBuffer.Size - pcmd->VtxOffset,
				idx_buffer + pcmd->IdxOffset,
				pcmd->ElemCount,
				sizeof(ImDrawIdx)
			);
		}
	}
	#undef OFFSETOF
}

static const char* GetClipboardText(void* userdata)
{
	return SDL_GetClipboardText();
}

static void SetClipboardText(void* userdata, const char *text)
{
	SDL_SetClipboardText(text);
}

/* Entry Point */

int main(int argc, char **argv)
{
	/* Basic stuff */
	SDL_Window *window;
	SDL_Renderer *renderer;
	SDL_Event evt;
	uint8_t run = 1;

	/* ImGui interop */
	ImGuiContext *imContext;
	SDL_Keymod kmod;
	uint8_t mouseClicked[3];
	int8_t mouseWheel;
	int mx, my;
	uint32_t mouseState;
	int ww, wh, dw, dh;
	Uint32 tCur, tLast = 0;

	/* ImGui texture */
	unsigned char *pixels;
	int tw, th;
	SDL_Texture *fontTexture;

	/* Create window/context */
	SDL_Init(SDL_INIT_VIDEO);
	window = SDL_CreateWindow(
		TOOL_NAME,
		SDL_WINDOWPOS_CENTERED,
		SDL_WINDOWPOS_CENTERED,
		TOOL_WIDTH,
		TOOL_HEIGHT,
		SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI
	);
	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC);
	SDL_SetRenderDrawColor(renderer, 114, 144, 154, 255);

	/* ImGui setup */
	imContext = ImGui::CreateContext(NULL);
	ImGui::SetCurrentContext(imContext);
	ImGuiIO& io = ImGui::GetIO();
	io.BackendRendererUserData = (void*) renderer;

	/* Keyboard */
	io.KeyMap[ImGuiKey_Tab] = SDLK_TAB;
	io.KeyMap[ImGuiKey_LeftArrow] = SDL_SCANCODE_LEFT;
	io.KeyMap[ImGuiKey_RightArrow] = SDL_SCANCODE_RIGHT;
	io.KeyMap[ImGuiKey_UpArrow] = SDL_SCANCODE_UP;
	io.KeyMap[ImGuiKey_DownArrow] = SDL_SCANCODE_DOWN;
	io.KeyMap[ImGuiKey_PageUp] = SDL_SCANCODE_PAGEUP;
	io.KeyMap[ImGuiKey_PageDown] = SDL_SCANCODE_PAGEDOWN;
	io.KeyMap[ImGuiKey_Home] = SDL_SCANCODE_HOME;
	io.KeyMap[ImGuiKey_End] = SDL_SCANCODE_END;
	io.KeyMap[ImGuiKey_Delete] = SDLK_DELETE;
	io.KeyMap[ImGuiKey_Backspace] = SDLK_BACKSPACE;
	io.KeyMap[ImGuiKey_Enter] = SDLK_RETURN;
	io.KeyMap[ImGuiKey_Escape] = SDLK_ESCAPE;
	io.KeyMap[ImGuiKey_A] = SDLK_a;
	io.KeyMap[ImGuiKey_C] = SDLK_c;
	io.KeyMap[ImGuiKey_V] = SDLK_v;
	io.KeyMap[ImGuiKey_X] = SDLK_x;
	io.KeyMap[ImGuiKey_Y] = SDLK_y;
	io.KeyMap[ImGuiKey_Z] = SDLK_z;

	/* Callbacks */
	io.RenderDrawListsFn = RenderDrawLists;
	io.GetClipboardTextFn = GetClipboardText;
	io.SetClipboardTextFn = SetClipboardText;

	/* Create texture for text rendering */
	io.Fonts->GetTexDataAsRGBA32(&pixels, &tw, &th);
	fontTexture = SDL_CreateTexture(
		renderer,
		SDL_PIXELFORMAT_RGBA32, /* FIXME: GL_ALPHA? */
		SDL_TEXTUREACCESS_STATIC,
		tw,
		th
	);
	SDL_UpdateTexture(fontTexture, NULL, pixels, 4 * tw);
	SDL_SetTextureBlendMode(fontTexture, SDL_BLENDMODE_BLEND);
	io.Fonts->TexID = (void*) fontTexture;

	while (run)
	{
		while (SDL_PollEvent(&evt) == 1)
		{
			if (evt.type == SDL_QUIT)
			{
				run = 0;
			}
			else if (	evt.type == SDL_KEYDOWN ||
					evt.type == SDL_KEYUP	)
			{
				kmod = SDL_GetModState();
				io.KeysDown[
					evt.key.keysym.sym & ~SDLK_SCANCODE_MASK
				] = evt.type == SDL_KEYDOWN;
				io.KeyShift = (kmod & KMOD_SHIFT) != 0;
				io.KeyCtrl = (kmod & KMOD_CTRL) != 0;
				io.KeyAlt = (kmod & KMOD_ALT) != 0;
				io.KeySuper = (kmod & KMOD_GUI) != 0;
			}
			else if (evt.type == SDL_MOUSEBUTTONDOWN)
			{
				if (evt.button.button < 4)
				{
					mouseClicked[evt.button.button - 1] = 1;
				}
			}
			else if (evt.type == SDL_MOUSEWHEEL)
			{
				if (evt.wheel.y > 0) mouseWheel = 1;
				if (evt.wheel.y < 0) mouseWheel = -1;
			}
			else if (evt.type == SDL_TEXTINPUT)
			{
				io.AddInputCharactersUTF8(evt.text.text);
			}
		}

		/* SDL-related updates */
		SDL_GetWindowSize(window, &ww, &wh);
		SDL_GL_GetDrawableSize(window, &dw, &dh);
		mouseState = SDL_GetMouseState(&mx, &my); /* TODO: Focus */
		mouseClicked[0] |= (mouseState * SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;
		mouseClicked[1] |= (mouseState * SDL_BUTTON(SDL_BUTTON_MIDDLE)) != 0;
		mouseClicked[2] |= (mouseState * SDL_BUTTON(SDL_BUTTON_RIGHT)) != 0;
		tCur = SDL_GetTicks();

		/* Set these every frame, we have a resizable window! */
		io.DisplaySize = ImVec2((float) ww, (float) wh);
		io.DisplayFramebufferScale = ImVec2(
			ww > 0 ? ((float) dw / ww) : 0,
			wh > 0 ? ((float) dh / wh) : 0
		);

		/* Time update */
		io.DeltaTime = (tCur - tLast) / 1000.0f;
		if (io.DeltaTime == 0.0f)
		{
			io.DeltaTime = 0.01f;
		}

		/* Input updates not done via UI_Submit*() */
		io.MousePos = ImVec2((float) mx, (float) my);
		io.MouseDown[0] = mouseClicked[0];
		io.MouseDown[1] = mouseClicked[1];
		io.MouseDown[2] = mouseClicked[2];
		io.MouseWheel = mouseWheel;

		/* BEGIN */
		ImGui::NewFrame();

		/* Reset some things now that input's updated */
		SDL_ShowCursor(io.MouseDrawCursor ? 0 : 1);
		tLast = tCur;
		mouseClicked[0] = 0;
		mouseClicked[1] = 0;
		mouseClicked[2] = 0;
		mouseWheel = 0;

		/* The actual meat of the audition tool */
		FAudioTool_Update();

		/* Draw, draw, draw! */
		SDL_RenderClear(renderer);
		ImGui::Render();
		SDL_RenderPresent(renderer);
	}

	/* Clean up if we need to */
	FAudioTool_Quit();

	/* Clean up. We out. */
	ImGui::DestroyContext(imContext);
	SDL_DestroyTexture(fontTexture);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();
	return 0;
}
