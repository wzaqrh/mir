#include "core/renderable/renderable_base.h"
#include "core/renderable/paint3d.h"
#include "core/base/debug.h"
#include "core/scene/transform.h"
#include "core/resource/resource_manager.h"

namespace mir {
#if defined MIR_MEMLEAK_DEBUG
Renderable::Renderable() { 
	DEBUG_MEM_ALLOC_TAG(rend);
}
Renderable::~Renderable() { 
	DEBUG_MEM_DEALLOC_TAG(rend);
}
#endif
namespace rend {

RenderableSingleRenderOp::RenderableSingleRenderOp(Launch launchMode, ResourceManager& resourceMng, const res::MaterialInstance& matInst)
	: mResMng(resourceMng)
	, mLaunchMode(launchMode)
	, mMaterial(matInst)
{
	mVao = mResMng.CreateVertexArray(mLaunchMode);
}

ITexturePtr RenderableSingleRenderOp::GetTexture() const 
{
	return mMaterial.GetTextures().AtOrNull(0);
}

Eigen::AlignedBox3f RenderableSingleRenderOp::GetWorldAABB() const
{
	Eigen::AlignedBox3f bounds = mAABB;
	if (!mAABB.isEmpty()) {
		if (auto transform = this->GetTransform()) {
			Transform3fAffine t(transform->GetWorldMatrix());
			bounds = mAABB.transformed(t);
		}
	}
	return bounds;
}

CoTask<void> RenderableSingleRenderOp::UpdateFrame(float dt)
{
#if MIR_MATERIAL_HOTLOAD
	DEBUG_LOG_CALLSTK("rendSROP.UpdateFrame");

	if ((mResMng.FrameCount % 30 == 0) && mMaterial && mMaterial->IsOutOfDate()) {
		auto mainTex = mMaterial.GetTextures().AtOrNull(0);
		CoAwait mMaterial.Reload(mLaunchMode, mResMng);
		mMaterial.GetTextures().AddOrSet(mainTex, 0);
		CoAwait mResMng.SwitchToLaunchService(__LaunchSync__);
	}
#endif
	CoReturn;
}

void RenderableSingleRenderOp::SetTexture(const ITexturePtr& texture)
{
	mMaterial.GetTextures().AddOrSet(texture, 0);
}

bool RenderableSingleRenderOp::IsLoaded() const
{
	BOOST_ASSERT(mMaterial);
	if (!mMaterial->IsLoaded()
		|| (mVertexBuffer && !mVertexBuffer->IsLoaded())
		|| (mIndexBuffer && !mIndexBuffer->IsLoaded()))
		return false;
	return true;
}

void RenderableSingleRenderOp::GenRenderOperation(RenderOperationQueue& ops)
{
	MakeRenderOperation(ops);
}

RenderOperation* RenderableSingleRenderOp::MakeRenderOperation(RenderOperationQueue& ops)
{
	if (!IsLoaded()) return nullptr;

#if MIR_GRAPHICS_DEBUG
	if (mDebugPaint) {
		mDebugPaint->Clear();
		mDebugPaint->DrawAABBEdge(GetWorldAABB());
		mDebugPaint->GenRenderOperation(ops);
	}
#endif

	RenderOperation op = {};
	op.Material = mMaterial;
	op.IndexBuffer = mIndexBuffer;
	op.AddVertexBuffer(mVertexBuffer);
	if (GetTransform()) op.WorldTransform = GetTransform()->GetWorldMatrix();
	ops.AddOP(op);
	
	return &ops.Back();
}

void RenderableSingleRenderOp::GetMaterials(std::vector<res::MaterialInstance>& mtls) const
{
	mtls.push_back(mMaterial);
}

}
}