#include "test/framework/test_case.h"
#include "core/resource/texture_factory.h"
#include "core/resource/resource_manager.h"
#include "core/renderable/sprite.h"
#include "core/renderable/post_process.h"

using namespace mir;
using namespace mir::rend;

class TestImGui : public App
{
protected:
	CoTask<bool> OnInitScene() override;
	void OnInitLight() override {}
	void OnInitCamera() override {}
private:
	bool mShowDemoWindow = true;
	bool mShowAnotherWindow = false;
	Eigen::Vector4f mClearColor = Eigen::Vector4f(0.45f, 0.55f, 0.60f, 1.00f);
};

CoTask<bool> TestImGui::OnInitScene()
{
	auto canvas = mScneMng->CreateGuiCanvasNode();
	auto camera = mScneMng->CreateCameraNode(kCameraPerspective);

	auto cmd = [&]()->CoTask<void> 
	{
		// 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
		if (mShowDemoWindow)
			ImGui::ShowDemoWindow(&mShowDemoWindow);

		// 2. Show a simple window that we create ourselves. We use a Begin/End pair to created a named window.
		{
			static float f = 0.0f;
			static int counter = 0;

			ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.

			ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
			ImGui::Checkbox("Demo Window", &mShowDemoWindow);      // Edit bools storing our window open/close state
			ImGui::Checkbox("Another Window", &mShowAnotherWindow);

			ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
			ImGui::ColorEdit3("clear color", (float*)&mClearColor); // Edit 3 floats representing a color

			if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
				counter++;
			ImGui::SameLine();
			ImGui::Text("counter = %d", counter);

			ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
			ImGui::End();
		}

		// 3. Show another simple window.
		if (mShowAnotherWindow)
		{
			ImGui::Begin("Another Window", &mShowAnotherWindow);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
			ImGui::Text("Hello from another window!");
			if (ImGui::Button("Close Me"))
				mShowAnotherWindow = false;
			ImGui::End();
		}

		camera->SetBackgroundColor(mClearColor);
		CoReturn;
	};
	mGuiMng->AddCommand(cmd);

	CoReturn true;
}

auto reg = AppRegister<TestImGui>("test_imgui");