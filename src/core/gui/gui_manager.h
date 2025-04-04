#pragma once
#include <boost/noncopyable.hpp>
#include "core/mir_export.h"
#include "core/gui/gui_canvas.h"

namespace mir {

class MIR_CORE_API GuiManager : boost::noncopyable
{
public:
	GuiManager(void* hwnd);
	~GuiManager();
	CoTask<bool> Initialize(Launch lchMode, ResourceManager& resMng) ThreadMaySwitch;
	void Dispose();

	void ClearCommands();
	void AddCommand(std::function<CoTask<void>()> cmd);
	CoTask<void> UpdateFrame(float dt);

	gui::GuiCanvasPtr GetCanvas() const { return mCanvas; }
	bool WantCaptureMouse() const;
	bool WantCaptureKeyboard() const;
private:
	gui::GuiCanvasPtr mCanvas;
	std::vector<std::function<CoTask<void>()>> mCmds;
};

MIR_CORE_API LRESULT GuiManager_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
}