#pragma once
#include <windows.h>
#include <dinput.h>
#include "core/mir_export.h"
#include "core/base/stl.h"
#include "core/base/math.h"

namespace mir {
namespace input {

class MIR_CORE_API D3DInput {
public:
	D3DInput(HINSTANCE hinstance, HWND hwnd, int screenWidth, int screenHeight);
	~D3DInput();
	bool Init(HINSTANCE hinstance, HWND hwnd, int screenWidth, int screenHeight);
	void Frame();
	Eigen::Vector2f GetMouseLeftLocation() const { return mMouseL; }
	Eigen::Vector2f GetMouseRightLocation() const { return mMouseR; }
	Eigen::Vector2f GetMouseMiddleLocation() const { return mMouseM; }
	float GetMouseWheelChange() const { return mMouseWheel; }
	bool IsKeyPressed(int key);
private:
	bool ReadKeyboard();
	bool ReadMouse();
	void Process();
private:
	IDirectInput8* m_directInput = nullptr;
	IDirectInputDevice8* m_keyboard = nullptr;
	IDirectInputDevice8* m_mouse = nullptr;

	unsigned char m_keyboardState[256];
	DIMOUSESTATE m_mouseState;

	long m_screenWidth = 0, m_screenHeight = 0;
	Eigen::Vector2f mMouseL, mMouseR, mMouseM;
	float mMouseWheel;
	bool mMouseMiddleDown;
};

std::vector<char> ReadFile(const char* fileName, const char* mode);
bool WriteFile(const char* fileName, const char* mode, const char bytes[], size_t byteCount);

}
}