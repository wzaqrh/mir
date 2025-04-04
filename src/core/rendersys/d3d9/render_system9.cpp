#include <boost/assert.hpp>
#include <boost/filesystem.hpp>
#include "core/rendersys/d3d9/render_system9.h"
#include "core/rendersys/d3d9/blob9.h"
#include "core/rendersys/d3d9/program9.h"
#include "core/rendersys/d3d9/input_layout9.h"
#include "core/rendersys/d3d9/hardware_buffer9.h"
#include "core/rendersys/d3d9/texture9.h"
#include "core/rendersys/d3d9/framebuffer9.h"
#include "core/resource/material_factory.h"
#include "core/renderable/renderable.h"
#include "core/base/debug.h"
#include "core/base/input.h"
#include "core/base/d3d.h"

namespace mir {

#define MakePtr CreateInstance
#define PtrRaw(T) T.get()

#define FILE_EXT_CSO ".cso"
#define FILE_EXT_FX ".fx"

RenderSystem9::RenderSystem9()
{}

RenderSystem9::~RenderSystem9()
{}

bool RenderSystem9::Initialize(HWND hWnd, RECT vp)
{
	mHWnd = hWnd;
	
	RECT rc;
	GetClientRect(mHWnd, &rc);
	UINT width = rc.right - rc.left;
	UINT height = rc.bottom - rc.top;

	if (NULL == (mD3D9 = Direct3DCreate9(D3D_SDK_VERSION))) {
		CheckHR(E_FAIL);
		return false;
	}
	if (!_GetDeviceCaps()) return false;
	if (!_CreateDeviceAndSwapChain()) return false;
	_SetRasterizerState();

	SetDepthState(DepthState::MakeFor3D());
	SetBlendState(BlendState::MakeAlphaPremultiplied());

	mDevice9->GetRenderTarget(0, &mBackColorBuffer); mCurColorBuffer = mBackColorBuffer;
	mDevice9->GetDepthStencilSurface(&mBackDepthStencilBuffer); mCurDepthStencilBuffer = mBackDepthStencilBuffer;

	mScreenSize.x() = width;
	mScreenSize.y() = height;

	D3DXMACRO Shader_Macros[] = { "SHADER_MODEL", "30000", NULL, NULL };
	mShaderMacros.assign(Shader_Macros, Shader_Macros + ARRAYSIZE(Shader_Macros));
	return true;
}

void RenderSystem9::SetViewPort(int x, int y, int w, int h)
{

}

bool RenderSystem9::_CreateDeviceAndSwapChain()
{	
	// Get the desktop display mode.
	D3DDISPLAYMODE displayMode;
	if (CheckHR(mD3D9->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &displayMode)))
		return false;

	int sampleType = D3DMULTISAMPLE_16_SAMPLES;
	DWORD sampleQuality = 0;
	for (; sampleType >= D3DMULTISAMPLE_NONE; --sampleType) {
		if (SUCCEEDED(mD3D9->CheckDeviceMultiSampleType(mD3DCaps.AdapterOrdinal, mD3DCaps.DeviceType, 
			displayMode.Format, FALSE, (D3DMULTISAMPLE_TYPE)sampleType, &sampleQuality))) {
			break;
		}
	}

	D3DPRESENT_PARAMETERS d3dpp;
	ZeroMemory(&d3dpp, sizeof(d3dpp));
	d3dpp.Windowed = TRUE;
	d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
	d3dpp.BackBufferFormat = displayMode.Format;// D3DFMT_A8B8G8R8;
	d3dpp.EnableAutoDepthStencil = TRUE;
	d3dpp.AutoDepthStencilFormat = D3DFMT_D24X8;
	d3dpp.MultiSampleType = (D3DMULTISAMPLE_TYPE)sampleType;
	//d3dpp.MultiSampleQuality = sampleQuality;

	bool success = false;
	int BehaviorFlags[] = { D3DCREATE_HARDWARE_VERTEXPROCESSING, D3DCREATE_HARDWARE_VERTEXPROCESSING, D3DCREATE_MIXED_VERTEXPROCESSING, D3DCREATE_SOFTWARE_VERTEXPROCESSING };
	for (size_t i = 0; i < ARRAYSIZE(BehaviorFlags); ++i) {
		if (! FAILED(mD3D9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, mHWnd, BehaviorFlags[i], &d3dpp, &mDevice9))) {
			success = true;
			break;
		}
	}
	assert(success);
	return success;
}

bool RenderSystem9::_GetDeviceCaps()
{
	if (CheckHR(mD3D9->GetDeviceCaps(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &mD3DCaps))) return false;
	debug::Log(mD3DCaps);
	return true;
}

void RenderSystem9::_SetRasterizerState()
{
	mDevice9->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
	mDevice9->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
}

void RenderSystem9::UpdateFrame(float dt)
{
}

void RenderSystem9::Dispose()
{
}

static inline D3DCOLOR XMFLOAT2D3DCOLOR(const Eigen::Vector4f& color) {
	D3DCOLOR dc = D3DCOLOR_RGBA(int(color.x() * 255), int(color.y() * 255), int(color.z() * 255), int(color.w() * 255));
	return dc;
}
void RenderSystem9::ClearFrameBuffer(IFrameBufferPtr rendTarget, const Eigen::Vector4f& color, float depth, unsigned char stencil)
{
	if (mCurColorBuffer != mBackColorBuffer) {
		//mDevice9->ColorFill(mCurColorBuffer, NULL, XMFLOAT2D3DCOLOR(color));
	}
	mDevice9->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, XMFLOAT2D3DCOLOR(color), depth, stencil);
}

IResourcePtr RenderSystem9::CreateResource(DeviceResourceType deviceResType)
{
	switch (deviceResType) {
	case mir::kDeviceResourceInputLayout:
		return MakePtr<InputLayout9>();
	case mir::kDeviceResourceProgram:
		return MakePtr<Program9>();
	case mir::kDeviceResourceVertexBuffer:
		return MakePtr<VertexBuffer9>();
	case mir::kDeviceResourceIndexBuffer:
		return MakePtr<IndexBuffer9>();
	case mir::kDeviceResourceContantBuffer:
		return MakePtr<ContantBuffer9>();
	case mir::kDeviceResourceTexture:
		return MakePtr<Texture9>(nullptr);
	case mir::kDeviceResourceFrameBuffer:
		return MakePtr<FrameBuffer9>();
	case mir::kDeviceResourceSamplerState:
		return MakePtr<SamplerState9>();
	default:
		break;
	}
	return nullptr;
}

IFrameBufferPtr RenderSystem9::LoadFrameBuffer(IResourcePtr res, const Eigen::Vector3i& size, const std::vector<ResourceFormat>& formats)
{
	BOOST_ASSERT(formats.size() >= 2);

	Texture9Ptr pTextureRV = MakePtr<Texture9>(nullptr);
	D3DFORMAT Format = d3d::convert11To9(static_cast<DXGI_FORMAT>(formats[0]));
	if (CheckHR(mDevice9->CreateTexture(size.x(), size.y(), 1, 
		D3DUSAGE_RENDERTARGET, Format, D3DPOOL_DEFAULT, &pTextureRV->GetSRV9(), NULL))) return nullptr;

	IDirect3DSurface9 *pSurfaceDepthStencil = nullptr;
	if (CheckHR(mDevice9->CreateDepthStencilSurface(size.x(), size.y(), 
		d3d::convert11To9(static_cast<DXGI_FORMAT>(formats.back())), D3DMULTISAMPLE_NONE, 0, TRUE, &pSurfaceDepthStencil, NULL))) return false;

	FrameBuffer9Ptr ret = MakePtr<FrameBuffer9>(pTextureRV, pSurfaceDepthStencil);
	return ret;
}

void RenderSystem9::SetFrameBuffer(IFrameBufferPtr rendTarget)
{
	if (rendTarget) {
		auto target9 = std::static_pointer_cast<FrameBuffer9>(rendTarget);
		mCurColorBuffer = std::static_pointer_cast<FrameBufferAttachColor9>(target9->GetAttachColor(0))
			->GetColorBuffer9();
		mCurDepthStencilBuffer = std::static_pointer_cast<FrameBufferAttachZStencil9>(target9->GetAttachZStencil())
			->GetDepthStencilBuffer9();
	}
	else {
		mCurColorBuffer = mBackColorBuffer;
		mCurDepthStencilBuffer = mBackDepthStencilBuffer;
	}
	if (CheckHR(mDevice9->SetRenderTarget(0, mCurColorBuffer))) return;
	if (CheckHR(mDevice9->SetDepthStencilSurface(mCurDepthStencilBuffer))) return;
}

IContantBufferPtr RenderSystem9::LoadConstBuffer(IResourcePtr res, const ConstBufferDecl& cbDecl, HWMemoryUsage usage, const Data& data)
{
	if (res == nullptr) res = CreateResource(kDeviceResourceContantBuffer);

	ContantBuffer9Ptr ret = MakePtr<ContantBuffer9>(CreateInstance<ConstBufferDecl>(cbDecl));
	if (data.NotNull()) {
		BOOST_ASSERT(data.Size == ret->GetBufferSize());
		UpdateBuffer(ret, data);
	}
	return ret;
}

IIndexBufferPtr RenderSystem9::LoadIndexBuffer(IResourcePtr res, ResourceFormat format, const Data& data)
{
	BOOST_ASSERT(res);
	IndexBuffer9Ptr ret;
	IDirect3DIndexBuffer9* pIndexBuffer = nullptr;
	D3DFORMAT Format = d3d::convert11To9(static_cast<DXGI_FORMAT>(format));
	if (! CheckHR(mDevice9->CreateIndexBuffer(data.Size, D3DUSAGE_WRITEONLY, Format, D3DPOOL_MANAGED, &pIndexBuffer, NULL))) {
		ret = std::static_pointer_cast<IndexBuffer9>(res);
		ret->Init(pIndexBuffer, data.Size, format);
		if (data.NotNull()) UpdateBuffer(ret, data);
	}
	return ret;
}

void RenderSystem9::SetIndexBuffer(IIndexBufferPtr indexBuffer)
{
	mDevice9->SetIndices(indexBuffer ? std::static_pointer_cast<IndexBuffer9>(indexBuffer)->GetBuffer9() : nullptr);
}

IVertexBufferPtr RenderSystem9::LoadVertexBuffer(IResourcePtr res, int stride, int offset, const Data& data)
{
	if (res == nullptr) res = CreateResource(kDeviceResourceVertexBuffer);

	VertexBuffer9Ptr ret;
	IDirect3DVertexBuffer9* pVertexBuffer = nullptr;
	if (! CheckHR(mDevice9->CreateVertexBuffer(data.Size, D3DUSAGE_WRITEONLY, 
		0/*non-FVF*/, D3DPOOL_MANAGED, &pVertexBuffer, NULL))) {
		ret = MakePtr<VertexBuffer9>(pVertexBuffer, data.Size, stride, offset);
	}
	if (data.NotNull()) UpdateBuffer(ret, data);
	return ret;
}

void RenderSystem9::SetVertexBuffers(size_t slot, const IVertexBufferPtr vertexBuffers[], size_t count)
{
	for (size_t i = 0; i < count; ++i) {
		auto vertexBuffer = vertexBuffers[i];
		uint32_t offset = vertexBuffer->GetOffset();
		uint32_t stride = vertexBuffer->GetStride();
		IDirect3DVertexBuffer9* buffer = std::static_pointer_cast<VertexBuffer9>(vertexBuffer)->GetBuffer9();
		mDevice9->SetStreamSource(slot + i, buffer, offset, stride);
	}
}

bool RenderSystem9::UpdateBuffer(IHardwareBufferPtr buffer, const Data& data)
{
	BOOST_ASSERT(buffer);
	HardwareBufferType bufferType = buffer->GetType();
	switch (bufferType)
	{
	case kHWBufferConstant: {
		IContantBufferPtr cbuffer = std::static_pointer_cast<IContantBuffer>(buffer);
		std::static_pointer_cast<ContantBuffer9>(cbuffer)->SetBuffer9(data.Bytes, data.Size);
	}break;
	case kHWBufferVertex: {
		IVertexBufferPtr vbuffer = std::static_pointer_cast<IVertexBuffer>(buffer);
		IDirect3DVertexBuffer9* buffer9 = std::static_pointer_cast<VertexBuffer9>(vbuffer)->GetBuffer9();
		void* pByteDest = nullptr;
		if (CheckHR(buffer9->Lock(0, data.Size, &pByteDest, 0))) return false;
		memcpy(pByteDest, data.Bytes, data.Size);
		if (CheckHR(buffer9->Unlock())) return false;
	}break;
	case kHWBufferIndex: {
		IIndexBufferPtr ibuffer = std::static_pointer_cast<IIndexBuffer>(buffer);
		IDirect3DIndexBuffer9* buffer9 = std::static_pointer_cast<IndexBuffer9>(ibuffer)->GetBuffer9();
		void* pByteDest = nullptr;
		if (CheckHR(buffer9->Lock(0, data.Size, &pByteDest, 0))) return false;
		memcpy(pByteDest, data.Bytes, data.Size);
		if (CheckHR(buffer9->Unlock())) return false;
	}break;
	default:
		break;
	}
	return true;
}

#if 0
static VertexShader9Ptr _CreateVSByBlob(IDirect3DDevice9* pDevice9, IBlobDataPtr pBlob) {
	VertexShader9Ptr ret = MakePtr< VertexShader9>();

	if (CheckHR(pDevice9->CreateVertexShader((DWORD*)pBlob->GetBufferPointer(), &ret->mShader))) return nullptr;

	ID3DXConstantTable* constTable = nullptr;
	if (CheckHR(D3DXGetShaderConstantTableEx((DWORD*)pBlob->GetBufferPointer(), D3DXCONSTTABLE_LARGEADDRESSAWARE, &constTable))) return nullptr;
	ret->SetConstTable(constTable);

	ret->mBlob = pBlob;
	AsRes(ret)->SetLoaded();
	return ret;
}
VertexShader9Ptr RenderSystem9::_CreateVS(const std::string& filename, const std::string& entry)
{
	DWORD Flag = 0;
#ifdef _DEBUG
	Flag = D3DXSHADER_DEBUG;
#endif
	std::string vsEntry = !entry.empty() ? entry : "VS";
	BlobData9Ptr blob = MakePtr<BlobData9>(nullptr);
	BlobData9Ptr errBlob = MakePtr<BlobData9>(nullptr);
	if (FAILED(D3DXCompileShaderFromFileA(filename.c_str(), 
		&mShaderMacros[0], nullptr, 
		vsEntry.c_str(), "vs_3_0", Flag, 
		&blob->mBlob, &errBlob->mBlob, nullptr))) {
		OutputDebugStringA((LPCSTR)errBlob->mBlob->GetBufferPointer());
		assert(FALSE);
		return nullptr;
	}

	VertexShader9Ptr ret = _CreateVSByBlob(mDevice9, blob);
	ret->mErrBlob = errBlob;
	return ret;
}
VertexShader9Ptr RenderSystem9::_CreateVSByFXC(const std::string& filename)
{
	VertexShader9Ptr ret;
	std::vector<char> buffer = input::ReadFile(filename.c_str(), "rb");
	if (!buffer.empty()) 
		ret = _CreateVSByBlob(mDevice9, MakePtr<BlobDataBytes>(buffer));
	return ret;
}

static PixelShader9Ptr _CreatePSByBlob(IDirect3DDevice9* pDevice9, IBlobDataPtr pBlob) {
	PixelShader9Ptr ret = MakePtr<PixelShader9>();
	ret->mBlob = pBlob;
	
	if (CheckHR(pDevice9->CreatePixelShader((DWORD*)pBlob->GetBytes(), &ret->mShader))) return nullptr;

	ID3DXConstantTable* constTable = nullptr;
	if (CheckHR(D3DXGetShaderConstantTableEx((DWORD*)pBlob->GetBytes(), D3DXCONSTTABLE_LARGEADDRESSAWARE, &constTable))) return nullptr;
	ret->SetConstTable(constTable);

	AsRes(ret)->SetLoaded();
	return ret;
}
PixelShader9Ptr RenderSystem9::_CreatePS(const std::string& filename, const std::string& entry)
{
	DWORD Flag = 0;
#ifdef _DEBUG
	Flag = D3DXSHADER_DEBUG;
#endif
	std::string psEntry = !entry.empty() ? entry : "PS";
	BlobData9Ptr blob = MakePtr<BlobData9>(nullptr);
	BlobData9Ptr errBlob = MakePtr<BlobData9>(nullptr);
	if (FAILED(D3DXCompileShaderFromFileA(filename.c_str(), 
		&mShaderMacros[0], nullptr, 
		psEntry.c_str(), "ps_3_0", Flag,
		&blob->mBlob, &errBlob->mBlob, nullptr))) {
		OutputDebugStringA((LPCSTR)errBlob->mBlob->GetBufferPointer());
		assert(FALSE);
		return nullptr;
	}

	PixelShader9Ptr ret = _CreatePSByBlob(mDevice9, blob);
	ret->mErrBlob = errBlob;
	return ret;
}
PixelShader9Ptr RenderSystem9::_CreatePSByFXC(const std::string& filename)
{
	PixelShader9Ptr ret;
	std::vector<char> buffer = input::ReadFile(filename.c_str(), "rb");
	if (!buffer.empty())
		ret = _CreatePSByBlob(mDevice9, MakePtr<BlobDataBytes>(buffer));
	return ret;
}

IProgramPtr RenderSystem9::CreateProgramByCompile(IResourcePtr res, const std::string& vsPath, 
	const std::string& psPath,
	const std::string& vsEntry,
	const std::string& psEntry)
{
	TIME_PROFILE(CreateProgramByCompile, vsPath);

	Program9Ptr program = MakePtr<Program9>();
	program->SetVertex(_CreateVS(vsPath, vsEntry));
	program->SetPixel(_CreatePS(!psPath.empty() ? psPath : vsPath, psEntry));
	AsRes(program)->CheckAndSetLoaded();
	return program;
}

IProgramPtr RenderSystem9::CreateProgramByFXC(IResourcePtr res, const std::string& name, 
	const std::string& vsEntry,
	const std::string& psEntry)
{
	TIME_PROFILE(CreateProgramByFXC, (name));
	Program9Ptr program = MakePtr<Program9>();

	std::string vsEntryOrVS = !vsEntry.empty() ? vsEntry : "VS";
	std::string vsName = name + "_" + vsEntryOrVS + FILE_EXT_CSO;
	program->SetVertex(_CreateVSByFXC(vsName.c_str()));

	std::string psEntryOrPS = !psEntry.empty() ? psEntry : "PS";
	std::string psName = name + "_" + psEntryOrPS + FILE_EXT_CSO;
	program->SetPixel(_CreatePSByFXC(psName.c_str()));

	AsRes(program)->CheckAndSetLoaded();
	return program;
}
#else
IBlobDataPtr RenderSystem9::CompileShader(const ShaderCompileDesc& compileDesc, const Data& data)
{
	return nullptr;
}

IShaderPtr RenderSystem9::CreateShader(int type, IBlobDataPtr data)
{
	return nullptr;
}

IProgramPtr RenderSystem9::LoadProgram(IResourcePtr res, const std::vector<IShaderPtr>& shaders)
{
	return nullptr;
}
#endif

ISamplerStatePtr RenderSystem9::LoadSampler(IResourcePtr res, const SamplerDesc& samplerDesc)
{
	SamplerState9Ptr ret = MakePtr<SamplerState9>();
	
	ret->mStates[D3DSAMP_ADDRESSU] = d3d::convert11To9(D3D11_TEXTURE_ADDRESS_WRAP);
	ret->mStates[D3DSAMP_ADDRESSV] = d3d::convert11To9(D3D11_TEXTURE_ADDRESS_WRAP);
	ret->mStates[D3DSAMP_ADDRESSW] = d3d::convert11To9(D3D11_TEXTURE_ADDRESS_WRAP);
	ret->mStates[D3DSAMP_BORDERCOLOR] = D3DCOLOR_ARGB(0,0,0,0);

	std::map<D3DSAMPLERSTATETYPE, D3DTEXTUREFILTERTYPE> fts = d3d::convert11To9(static_cast<D3D11_FILTER>(samplerDesc.Filter));
	for (auto& iter : fts)
		ret->mStates[iter.first] = iter.second;

	ret->mStates[D3DSAMP_MIPMAPLODBIAS] = 0.0f;
	ret->mStates[D3DSAMP_MAXANISOTROPY] = (samplerDesc.Filter == D3D11_FILTER_ANISOTROPIC) ? D3D11_REQ_MAXANISOTROPY : 1;
	ret->mStates[D3DSAMP_MAXMIPLEVEL] = D3D11_FLOAT32_MAX;

	return ret;
}

IDirect3DVertexDeclaration9* RenderSystem9::_CreateInputLayout(Program9* _, const std::vector<D3DVERTEXELEMENT9>& descArr)
{
	IDirect3DVertexDeclaration9* ret = nullptr;
	if (CheckHR(mDevice9->CreateVertexDeclaration(&descArr[0], &ret))) return nullptr;
	return ret;
}

IInputLayoutPtr RenderSystem9::LoadLayout(IResourcePtr res, IProgramPtr pProgram, const std::vector<LayoutInputElement>& descArr)
{
	InputLayout9Ptr ret = MakePtr<InputLayout9>();
	for (size_t i = 0; i < descArr.size(); ++i)
		ret->mInputDescs.push_back(d3d::convert11To9(descArr[i]));
	ret->mInputDescs.push_back(D3DDECL_END());

	auto resource = AsRes(pProgram);
	//if (resource->IsLoaded()) {
		ret->mLayout = _CreateInputLayout(std::static_pointer_cast<Program9>(pProgram).get(), ret->mInputDescs);
		resource->SetLoaded();
		/*}
		else {
			resource->AddOnLoadedListener([=](IResource* res) {
				ret->mLayout = _CreateInputLayout(std::static_pointer_cast<Program9>(pProgram).get(), ret->mInputDescs);
				res->SetLoaded();
			});
		}*/
	return ret;
}
void RenderSystem9::SetVertexLayout(IInputLayoutPtr layout)
{
	mDevice9->SetVertexDeclaration(std::static_pointer_cast<InputLayout9>(layout)->GetLayout9());
}

ITexturePtr RenderSystem9::LoadTexture(IResourcePtr res, ResourceFormat format, 
	const Eigen::Vector4i& size, int mipmap, const Data datas[])
{
	BOOST_ASSERT(mipmap == 1);
	Texture9Ptr texture = MakePtr<Texture9>(size.x(), size.y(), format, mipmap);
	IDirect3DTexture9* pTexture = nullptr;
	if (FAILED(mDevice9->CreateTexture(size.x(), size.y(), 0, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &pTexture, NULL)))
		return nullptr;
	texture->SetSRV9(pTexture);
	return texture;
}

bool RenderSystem9::LoadRawTextureData(ITexturePtr texture, char* data, int dataSize, int dataStep)
{
	BOOST_ASSERT(dataStep * texture->GetHeight() <= dataSize);
	Texture9Ptr tex9 = std::static_pointer_cast<Texture9>(texture);

	int width = texture->GetWidth(), height = texture->GetHeight();
	IDirect3DTexture9* pTexture = tex9->GetSRV9();
	if (pTexture == nullptr) return false;

	D3DLOCKED_RECT outRect;
	if (FAILED(pTexture->LockRect(0, &outRect, nullptr, D3DLOCK_DISCARD)))
		return false;
	
	char *src = data, *dst = (char*)outRect.pBits;
	int pitch = min(outRect.Pitch, dataStep);
	for (int y = 0; y < height; ++y)
	{
		memcpy(dst, src, pitch);
		src += dataStep;
		dst += outRect.Pitch;
	}
	if (FAILED(pTexture->UnlockRect(0)))
		return false;

	return true;
}

void RenderSystem9::SetBlendState(const BlendState& blendFunc)
{
	mCurBlendState = blendFunc;
	mDevice9->SetRenderState(D3DRS_ALPHABLENDENABLE, (blendFunc.Src == kBlendOne && blendFunc.Dst == kBlendZero) ? FALSE : TRUE);
	mDevice9->SetRenderState(D3DRS_SRCBLEND, d3d::convert11To9(static_cast<D3D11_BLEND>(blendFunc.Src)));
	mDevice9->SetRenderState(D3DRS_DESTBLEND, d3d::convert11To9(static_cast<D3D11_BLEND>(blendFunc.Dst)));
}

void RenderSystem9::SetDepthState(const DepthState& depthState)
{
	mCurDepthState = depthState;
	mDevice9->SetRenderState(D3DRS_ZENABLE, depthState.DepthEnable);
	mDevice9->SetRenderState(D3DRS_ZFUNC, d3d::convert11To9(static_cast<D3D11_COMPARISON_FUNC>(depthState.CmpFunc)));
	mDevice9->SetRenderState(D3DRS_ZWRITEENABLE, depthState.WriteMask == D3D11_DEPTH_WRITE_MASK_ALL ? TRUE : FALSE);
}

void RenderSystem9::SetProgram(IProgramPtr program) 
{
	Program9Ptr program9 = std::static_pointer_cast<Program9>(program);
	PixelShader9* ps = PtrRaw(program9->mPixel);
	VertexShader9* vs = PtrRaw(program9->mVertex);

	mDevice9->SetVertexShader(vs->GetShader9());
	mDevice9->SetPixelShader(ps->GetShader9());
}

void RenderSystem9::SetConstBuffers(size_t slot, const IContantBufferPtr buffers[], size_t count, IProgramPtr program)
{
	Program9Ptr program9 = std::static_pointer_cast<Program9>(program);
	PixelShader9* ps = PtrRaw(program9->mPixel);
	VertexShader9* vs = PtrRaw(program9->mVertex);

	for (size_t i = 0; i < count; ++i) {
		IContantBufferPtr buffer = buffers[i];
		char* buffer9 = (char*)std::static_pointer_cast<ContantBuffer9>(buffer)->GetBuffer9();
		ConstBufferDeclPtr decl = buffer->GetDecl();
		vs->mConstTable.SetValue(mDevice9, buffer9, *decl);
		ps->mConstTable.SetValue(mDevice9, buffer9, *decl);
	}
}

void RenderSystem9::SetSamplers(size_t slot, const ISamplerStatePtr samplers[], size_t count)
{
	BOOST_ASSERT(count > 0);
	for (size_t i = 0; i < count; ++i) {
		ISamplerStatePtr sampler = samplers[i];
		std::map<D3DSAMPLERSTATETYPE, DWORD>& states = std::static_pointer_cast<SamplerState9>(sampler)->GetSampler9();
		for (auto& pair : states)
			mDevice9->SetSamplerState(i, pair.first, pair.second);
	}
}

inline int CalPrimCount(int indexCount, D3DPRIMITIVETYPE topo) {
	switch (topo)
	{
	case D3DPT_POINTLIST: return indexCount;
	case D3DPT_LINELIST: return indexCount / 2;
	case D3DPT_LINESTRIP: return indexCount - 1;
	case D3DPT_TRIANGLELIST: return indexCount / 3;
	case D3DPT_TRIANGLESTRIP: return indexCount - 2;
	case D3DPT_TRIANGLEFAN: return indexCount - 2;
	default:break;
	}
	assert(FALSE);
	return 0;
}
void RenderSystem9::DrawPrimitive(const RenderOperation& op, PrimitiveTopology topo) {
	D3DPRIMITIVETYPE topo9 = d3d::convert11To9(static_cast<D3D11_PRIMITIVE_TOPOLOGY>(topo));
	//if (_CanDraw())
	mDevice9->DrawPrimitive(topo9, 0, CalPrimCount(op.VertexBuffers[0]->GetBufferSize() / op.VertexBuffers[0]->GetStride(), topo9));
}
void RenderSystem9::DrawIndexedPrimitive(const RenderOperation& op, PrimitiveTopology topo) {
	D3DPRIMITIVETYPE topo9 = d3d::convert11To9(static_cast<D3D11_PRIMITIVE_TOPOLOGY>(topo));
	//if (_CanDraw())
	mDevice9->DrawIndexedPrimitive(topo9, 
		0, 0, op.VertexBuffers[0]->GetBufferSize() / op.VertexBuffers[0]->GetStride(), 0, 
		CalPrimCount(op.IndexBuffer->GetCount(), topo9));
}

void RenderSystem9::SetTextures(size_t slot, const ITexturePtr textures[], size_t count) {
	for (size_t i = 0; i < count; ++i) {
		auto iTex = std::static_pointer_cast<Texture9>(textures[i]);
		if (iTex) {
			if (iTex->IsCube()) mDevice9->SetTexture(i, iTex->GetSRVCube9());
			else mDevice9->SetTexture(i, iTex->GetSRV9());
		}
		else {
			mDevice9->SetTexture(i, nullptr);
		}
	}
}

bool RenderSystem9::BeginScene()
{
	if (FAILED(mDevice9->BeginScene()))
		return false;
	return true;
}

void RenderSystem9::EndScene()
{
	mDevice9->EndScene();
	mDevice9->Present(NULL, NULL, NULL, NULL);
}

}