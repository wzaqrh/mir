#include <boost/assert.hpp>
#include <boost/asio.hpp>
#include <boost/format.hpp>
#include "core/base/debug.h"
#include "core/base/macros.h"
#include "core/base/thread.h"
#include "core/resource/resource_manager.h"
#include "core/resource/device_res_factory.h"
#include "core/resource/texture_factory.h"
#include "core/resource/program_factory.h"
#include "core/resource/material_factory.h"
#include "core/resource/assimp_factory.h"

namespace mir {

ResourceManager::ResourceManager(RenderSystem& renderSys, std::shared_ptr<cppcoro::io_service> ioService, const std::string& shaderDir)
: mRenderSys(renderSys)
{
	mMaterialFac = CreateInstance<res::MaterialFactory>(*this, shaderDir);
	mProgramFac = CreateInstance<res::ProgramFactory>(*this, shaderDir);
	mTextureFac = CreateInstance<res::TextureFactory>(*this);
	mAiResFac = CreateInstance<res::AiResourceFactory>(*this);
	mDeviceResFac = CreateInstance<res::DeviceResFactory>(mRenderSys);

	mMainThreadId = std::this_thread::get_id();
	TIME_PROFILE((boost::format("resMng.main_tid %1%") % mMainThreadId).str());

	constexpr int CThreadPoolNumber = 8;
	mThreadPool = CreateInstance<cppcoro::static_thread_pool>(CThreadPoolNumber);
	mIoService = ioService;
}
ResourceManager::~ResourceManager()
{
	DEBUG_LOG_MEMLEAK("resMng.destrcutor");
	Dispose();
}
void ResourceManager::Dispose() ThreadSafe
{
	if (mThreadPool) {
		DEBUG_LOG_MEMLEAK("resMng.Dispose");
		mDeviceResFac = nullptr;
		mTextureFac = nullptr;
		mProgramFac = nullptr;
		mMaterialFac = nullptr;
		mAiResFac = nullptr;

		mIoService = nullptr;
		mThreadPool = nullptr;
	}
}

bool ResourceManager::IsCurrentInAsyncService() const
{
	return std::this_thread::get_id() != mMainThreadId;
}
CoTask<void> ResourceManager::SwitchToLaunchService(Launch launchMode)
{
#if !defined MIR_CPPCORO_DISABLED
	COROUTINE_VARIABLES_1(launchMode);
	DEBUG_LOG_CALLSTK((boost::format("resMng.SwitchToLaunchService lchMode=%d curIsAsync=%d") %launchMode %IsCurrentInAsyncService()).str());

	if (launchMode == LaunchAsync) {
		if (! IsCurrentInAsyncService())
			CoAwait mThreadPool->schedule();
		BOOST_ASSERT(IsCurrentInAsyncService());
	}
	else {
		if (IsCurrentInAsyncService())
			CoAwait mIoService->schedule();
		BOOST_ASSERT(!IsCurrentInAsyncService());
	}
#endif
	CoReturnVoid;
}
CoTask<void> ResourceManager::WaitResComplete(IResourcePtr res, std::chrono::microseconds interval)
{
#if !defined MIR_CPPCORO_DISABLED
	DEBUG_LOG_CALLSTK(boost::format("resMng.WaitResComplete").str());

	bool asyncMode = IsCurrentInAsyncService();
	while (!res->IsLoadComplete()) {
		CoAwait mIoService->schedule_after(interval);
	}
	if (asyncMode) {
		CoAwait mThreadPool->schedule();
		BOOST_ASSERT(IsCurrentInAsyncService());
	}
#endif
	CoReturnVoid;
}

/********** Async Support **********/
CoTask<void> ResourceManager::UpdateFrame(float dt) ThreadSafe
{
#if MIR_MATERIAL_HOTLOAD
	DEBUG_LOG_CALLSTK("resMng.UpdateFrame");
	FrameCount++;
	if ((FrameCount % 30 == 0) && mMaterialFac->PurgeOutOfDates()) {
		mProgramFac->PurgeAll();
	}
#endif
	CoReturn;
}

}