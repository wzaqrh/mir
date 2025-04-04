#include <boost/algorithm/clamp.hpp>
#include <boost/filesystem.hpp>
#include "test/framework/app.h"
#include "test/framework/test_case.h"
#include "core/base/input.h"
#include "core/scene/transform.h"
#include "core/scene/camera.h"
#include "core/scene/light.h"
#include "core/scene/scene_manager.h"

#ifdef _DEBUG
#pragma comment(lib, "mird.lib")
#pragma comment(lib, "cppcorod.lib")
#else
#pragma comment(lib, "mir.lib")
#pragma comment(lib, "cppcoro.lib")
#endif
#pragma comment(lib, "winmm.lib")

using namespace mir;

/************************************************************************/
/* App */
/************************************************************************/

int ReadAppSetting(const std::string& key0, const std::string& key1, int defValue) {
	boost::filesystem::path curPath = boost::filesystem::current_path();
	curPath /= "setting.ini";
	std::string iniName = curPath.string();

	return GetPrivateProfileIntA(key0.c_str(), key1.c_str(), defValue, iniName.c_str());
}
void WriteAppSetting(const std::string& key0, const std::string& key1, int value) {
	boost::filesystem::path curPath = boost::filesystem::current_path();
	curPath /= "setting.ini";
	std::string iniName = curPath.string();

	std::stringstream ss;
	ss << value;
	WritePrivateProfileStringA(key0.c_str(), key1.c_str(), ss.str().c_str(), iniName.c_str());
}

//#define AppLaunchMode __LaunchSync__
#define AppLaunchMode __LaunchAsync__

App::App()
{
}
App::~App()
{
	CleanUp();
}
void App::CleanUp()
{
	if (mContext) {
		mContext->Dispose();
		mContext = nullptr;
		ResetMir();
	}
}
void App::Create()
{
	mCameraInitInvLengthForward = Eigen::Vector3f::Zero();
	mContext = new mir::Mir(AppLaunchMode);
}
CoTask<bool> App::InitContext(HINSTANCE hInstance, HWND hWnd)
{
	BOOL IsOpenglBackend = ReadAppSetting("Rendering Backend", "IsOpengl", TRUE);

	boost::filesystem::path workPath = boost::filesystem::system_complete("../work/");
	if (!boost::filesystem::is_directory(workPath)) CoReturn false;

	test1::res::SetMediaDirectory(workPath.string() + "media/");

	mHnd = hWnd;
	if (! CoAwait mContext->Initialize(mHnd, workPath.string(), IsOpenglBackend ? kPlatformOpengl : kPlatformDirectx)) {
		mContext->Dispose();
		CoReturn false;
	}
	SetMir(mContext);

	mInput = new mir::input::D3DInput(hInstance, hWnd, mContext->WinSize().x(), mContext->WinSize().y());
	mTimer = new mir::debug::Timer;
	CoReturn true;
}
CoTask<bool> App::InitScene()
{
	OnInitCamera();
	OnInitLight();
	return OnInitScene();
}
void App::OnInitCamera()
{
	mContext->SceneMng()->CreateCameraNode(kCameraPerspective);
}
void App::OnInitLight()
{
	mContext->SceneMng()->CreateLightNode(kLightPoint);
}

CoTask<void> App::Render()
{
	auto mScneMng = mContext->SceneMng();

	mTimer->Update();
	mInput->Frame();

	//rotate camera
	auto camera0 = mScneMng->GetDefCamera();
	if (camera0 && mControlCamera && !mGuiMng->WantCaptureMouse())
	{
		TransformPtr camera0Tranform = camera0->GetTransform();
		if (!mCameraInitInvLengthForward.any()) {
			mCameraInitInvLengthForward = -camera0Tranform->GetForward() * camera0Tranform->GetForwardLength();
			mCameraInitLookAt = camera0Tranform->GetLookAt();
		}
		
		float forwardLength = camera0Tranform->GetForwardLength();
		if (mInput->GetMouseWheelChange() != 0)
			forwardLength -= mInput->GetMouseWheelChange() * 0.8 * forwardLength;

		Eigen::Vector3f curInvForward = -camera0Tranform->GetForward();
		{
			Eigen::Vector2f m = mInput->GetMouseRightLocation();
			float mx = -3.14 * m.x();
			float my = -3.14 * 0.5 * m.y();
			(fabs(mx) > fabs(my)) ? my = 0 : mx = 0;

			Eigen::Quaternionf quat = Eigen::AngleAxisf(0, Eigen::Vector3f::UnitZ())
				* Eigen::AngleAxisf(my, Eigen::Vector3f::UnitX())
				* Eigen::AngleAxisf(mx, Eigen::Vector3f::UnitY());
			curInvForward = Transform3fAffine(quat) * curInvForward;
		}

		camera0->SetLookAt(mCameraInitLookAt + curInvForward * forwardLength, mCameraInitLookAt);
	}

	if (mTransform && !mGuiMng->WantCaptureMouse())
	{
		Eigen::Vector2f m = mInput->GetMouseMiddleLocation();
		float mx = m.x();
		float my = -m.y();
		if (mx != 0 || my != 0) {
			auto p = mTransform->GetPosition();
			auto cp = camera0->GetTransform()->GetPosition();
			float offset = fabs(p.z() - cp.z());
			p.x() += mx * offset;
			p.y() += my * offset;
			mTransform->SetPosition(p);
		}
	}

	//rotate target
	if (mTransform)
	{
		if (!mGuiMng->WantCaptureMouse()) 
		{
			Eigen::Vector2f m = mInput->GetMouseLeftLocation();
			float mx = 3.14 * -m.x();
			float my = 3.14 * -m.y();
			(fabs(mx) > fabs(my)) ? my = 0 : mx = 0;

			auto quat = Eigen::AngleAxisf(0, Eigen::Vector3f::UnitZ())
				* Eigen::AngleAxisf(my, Eigen::Vector3f::UnitX())
				* Eigen::AngleAxisf(mx, Eigen::Vector3f::UnitY())
				* mTransform->GetRotation();
			mTransform->SetRotation(quat);
		}

		if (!mGuiMng->WantCaptureKeyboard()) 
		{
			if (mInput->IsKeyPressed(DIK_UPARROW)) {
				mTransform->SetPosition(mTransform->GetPosition() + Eigen::Vector3f(0, 0, 5));
			}
			else if (mInput->IsKeyPressed(DIK_DOWNARROW)) {
				mTransform->SetPosition(mTransform->GetPosition() - Eigen::Vector3f(0, 0, 5));
			}
		}
	}
	CoAwait mContext->Update(mTimer->mDeltaTime);
	CoAwait mContext->Render();
}

int App::MainLoop(HINSTANCE hInstance, HWND hWnd)
{
	auto mainLoop = [&]()->CoTask<bool> {
	#if defined MIR_TIME_DEBUG
		auto time = std::make_shared<mir::debug::TimeProfile>("App.InitScene");
	#else
		int time = 0;
	#endif

		if (! CoAwait this->InitContext(hInstance, hWnd))
			CoReturn false;

		bool quitFlag = false;
		auto initScene = this->InitScene();
		auto processMsg = [this, &quitFlag]()->CoTask<bool> 
		{
			while (!quitFlag) 
			{
				MSG msg;
				while (::PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
				{
					::TranslateMessage(&msg);
					::DispatchMessage(&msg);
					if (msg.message == WM_QUIT)
						quitFlag = true;
				}
				if (quitFlag)
					break;

				CoAwait mContext->ScheduleTaskAfter(std::chrono::microseconds(1));
			}
			CoReturn true;
		};
		auto renderScene = [this,&initScene,&time,&quitFlag]()->CoTask<bool>
		{
			while (!quitFlag) 
			{
				if (initScene.is_ready()) {
				#if defined MIR_TIME_DEBUG
					time = nullptr;
				#endif
					CoAwait this->Render();
				}
				else {
					mContext->ProcessPendingEvent();
				}

				CoAwait mContext->ScheduleTaskAfter(std::chrono::microseconds(1));
			}
		};
		CoAwait WhenAllReady(initScene, renderScene(), processMsg());

		this->CleanUp();
		CoReturn true;
	};
	mContext->ExecuteTaskSync(mainLoop());
	
	delete this;
	return 0;
}

std::string App::GetName()
{
	return mName;
}
void App::SetCaseIndex(int caseIndex)
{
	mCaseIndex = caseIndex;
}
void App::SetCaseSecondIndex(int secondIndex)
{
	mCaseSecondIndex = secondIndex;
}

/************************************************************************/
/* MirManager */
/************************************************************************/
void MirManager::SetMir(mir::Mir* ctx)
{
	mGuiMng = ctx->GuiMng();
	mScneMng = ctx->SceneMng();
	mRendFac = ctx->RenderableFac();
	mResMng = ctx->ResourceMng();
	auto size = ctx->WinSize();
	mHalfSize = Eigen::Vector3f(size.x() / 2, size.y() / 2, 0);
	mWinCenter = Eigen::Vector3f(0, 0, 0);

	float aspect = 1.0 * size.x() / size.y();
	mCamWinHSize = Eigen::Vector3f(aspect * 5, 5, 0);
}

void MirManager::ResetMir()
{
	mGuiMng = nullptr;
	mScneMng = nullptr;
	mRendFac = nullptr;
	mResMng = nullptr;
}

void MirManager::SetPPU(float ppu)
{
	mScneMng->SetPixelPerUnit(ppu);

	auto size = mScneMng->GetDefCamera()->GetOthoWinSize();
	mCamWinHSize = Eigen::Vector3f(size.x() / 2, size.y() / 2, 0);
}