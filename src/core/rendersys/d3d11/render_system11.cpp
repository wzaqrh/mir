#include <boost/assert.hpp>
#include <boost/format.hpp>
#include <boost/filesystem.hpp>
#include <boost/lambda/if.hpp>
#include <windows.h>
#include <dxerr.h>
#include <d3dcompiler.h>
#include "core/mir_config.h"
#include "core/rendersys/d3d11/d3d_utils.h"
#include "core/base/debug.h"
#include "core/base/input.h"
#include "core/base/macros.h"
#include "core/rendersys/d3d11/render_system11.h"
#include "core/rendersys/d3d11/blob11.h"
#include "core/rendersys/d3d11/program11.h"
#include "core/rendersys/d3d11/input_layout11.h"
#include "core/rendersys/d3d11/hardware_buffer11.h"
#include "core/rendersys/d3d11/texture11.h"
#include "core/rendersys/d3d11/framebuffer11.h"
#include "core/resource/material_factory.h"
#include "core/renderable/renderable.h"

namespace mir {

#define MakePtr CreateInstance

RenderSystem11::RenderSystem11()
{}
Platform RenderSystem11::GetPlatform() const 
{
	return Platform{ kPlatformDirectx, 11 };
}

RenderSystem11::~RenderSystem11()
{
	DEBUG_LOG_MEMLEAK("rendSys11.destrcutor");
	Dispose();
}
void RenderSystem11::Dispose()
{
	if (mDeviceContext) {
		DEBUG_LOG_MEMLEAK("rendSys11.dispose");
		mSwapChain = nullptr;
		mDxDSStates.clear();
		mDxBlendStates.clear();
		mDxRasterStates.clear();
		mBackFrameBuffer = nullptr;
		mCurFrameBuffer = nullptr;
	#if defined MIR_RENDERSYS_DEBUG
		mDeviceContext->Flush();
		mDeviceContext = nullptr;

		ID3D11Debug* pDebug;
		mDevice->QueryInterface(IID_ID3D11Debug, (VOID**)(&pDebug));
		if (pDebug) {
			DEBUG_LOG_WARN("RenderSystem11.Dispose ReportLiveDeviceObjects Begin\n");
			pDebug->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL);
			pDebug->Release();
			DEBUG_LOG_WARN("RenderSystem11.Dispose ReportLiveDeviceObjects End\n");
		}
		mDevice = nullptr;
	#else
		mDeviceContext = nullptr;
		mDevice = nullptr;
	#endif
	}
}

bool RenderSystem11::_CreateDeviceAndSwapChain(int width, int height)
{
	uint32_t createDeviceFlags = 0;
#if defined MIR_RENDERSYS_DEBUG
	createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	std::array<D3D_DRIVER_TYPE, 3> driverTypes = {
		D3D_DRIVER_TYPE_HARDWARE,
		D3D_DRIVER_TYPE_WARP,
		D3D_DRIVER_TYPE_REFERENCE,
	};

	std::array<D3D_FEATURE_LEVEL,3> featureLevels = {
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0,
	};

	DXGI_SWAP_CHAIN_DESC sd = {};
	sd.BufferCount = 2;
	sd.BufferDesc.Width = width;
	sd.BufferDesc.Height = height;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = mHWnd;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.Windowed = TRUE;
	sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

	D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
	for (size_t i = 0; i < driverTypes.size(); i++) {
		if (SUCCEEDED(D3D11CreateDeviceAndSwapChain(NULL, driverTypes[i], NULL, createDeviceFlags,
			featureLevels.data(), featureLevels.size(), D3D11_SDK_VERSION, &sd,
			&mSwapChain, &mDevice, &featureLevel, &mDeviceContext)))
			return true;
	}
	return false;
}
bool RenderSystem11::_FetchBackFrameBufferColor(int width, int height)
{
	ComPtr<ID3D11Texture2D> pTexture = nullptr;
	if (CheckHR(mSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pTexture))) return false;

	Texture11Ptr texture = CreateInstance<Texture11>();
	texture->Init(std::move(pTexture));
	if (!texture->InitRTV(mDevice)) return false;

	mBackFrameBuffer = CreateInstance<FrameBuffer11>();
	mBackFrameBuffer->SetSize(Eigen::Vector2i(width, height));
	mBackFrameBuffer->SetAttachColor(0, CreateInstance<FrameBuffer11Attach>(texture));
	mCurFrameBuffer = mBackFrameBuffer;
	return true;
}
bool RenderSystem11::_FetchBackBufferZStencil(int width, int height)
{
	constexpr size_t faceCount = 1;
	constexpr size_t mipCount = 1;

	Texture11Ptr texture = CreateInstance<Texture11>();
	texture->Init(kFormatD24UNormS8UInt, kHWUsageDefault, width, height, faceCount, mipCount, true);
	if (!texture->InitTex(mDevice)) return nullptr;
	if (!texture->InitDSV(mDevice)) return nullptr;

	mBackFrameBuffer->SetAttachZStencil(CreateInstance<FrameBuffer11Attach>(texture));
	
#if defined MIR_RESOURCE_DEBUG
	int idx = 0;
	for (auto rtv : mBackFrameBuffer->AsRTVs())
		DEBUG_RES_ADD_DEVICE(mBackFrameBuffer, rtv, "rtv" + boost::lexical_cast<std::string>(idx++));
	DEBUG_RES_ADD_DEVICE(mBackFrameBuffer, mBackFrameBuffer->AsDSV(), "-1");
	DEBUG_SET_PRIV_DATA(mBackFrameBuffer, "device.backbuffer");
#endif
	return true;
}
bool RenderSystem11::Initialize(HWND hWnd, Eigen::Vector4i viewport)
{
	TIME_PROFILE("renderSys11.Initialize");
	mMainThreadId = std::this_thread::get_id();
	mHWnd = hWnd;

	RECT rc;
	GetClientRect(mHWnd, &rc);
	UINT rcWidth = rc.right - rc.left;
	UINT rcHeight = rc.bottom - rc.top;

	if (!_CreateDeviceAndSwapChain(rcWidth, rcHeight)) return false;
	
	if (!_FetchBackFrameBufferColor(rcWidth, rcHeight)) return false; 
	if (!_FetchBackBufferZStencil(rcWidth, rcHeight)) return false;

	if (!viewport.any()) GetClientRect(mHWnd, (RECT*)&viewport);
	mScreenSize = viewport.tail<2>() - viewport.head<2>();
	SetViewPort(viewport[0], viewport[1], mScreenSize.x(), mScreenSize.y());

	SetFrameBuffer(nullptr);

	if (!_SetRasterizerState(mCurRasterState = RasterizerState::MakeDefault())) return false;
	if (!_SetDepthState(mCurDepthState = DepthState::Make(kCompareLessEqual, kDepthWriteMaskAll, true))) return false;
	if (!_SetBlendState(mCurBlendState = BlendState::MakeAlphaPremultiplied())) return false;
	return true;
}

bool RenderSystem11::IsCurrentInMainThread() const
{
	return mMainThreadId == std::this_thread::get_id();
}
void RenderSystem11::UpdateFrame(float dt)
{}
void RenderSystem11::SetViewPort(int x, int y, int width, int height)
{
	D3D11_VIEWPORT vp;
	vp.Width = (FLOAT)width;
	vp.Height = (FLOAT)height;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = x;
	vp.TopLeftY = y;
	mDeviceContext->RSSetViewports(1, &vp);
}

IResourcePtr RenderSystem11::CreateResource(DeviceResourceType deviceResType)
{
	switch (deviceResType) {
	case mir::kDeviceResourceInputLayout:
		return MakePtr<InputLayout11>();
	case mir::kDeviceResourceProgram:
		return MakePtr<Program11>();
	case mir::kDeviceResourceVertexArray:
		return MakePtr<VertexArray11>();
	case mir::kDeviceResourceVertexBuffer:
		return MakePtr<VertexBuffer11>();
	case mir::kDeviceResourceIndexBuffer:
		return MakePtr<IndexBuffer11>();
	case mir::kDeviceResourceContantBuffer:
		return MakePtr<ContantBuffer11>();
	case mir::kDeviceResourceTexture:
		return MakePtr<Texture11>();
	case mir::kDeviceResourceFrameBuffer:
		return MakePtr<FrameBuffer11>();
	case mir::kDeviceResourceSamplerState:
		return MakePtr<SamplerState11>();
	default:
		break;
	}
	return nullptr;
}

/********** LoadFrameBuffer **********/
IFrameBufferPtr RenderSystem11::GetBackFrameBuffer() 
{
	return mBackFrameBuffer;
}
IFrameBufferPtr RenderSystem11::LoadFrameBuffer(IResourcePtr res, const Eigen::Vector3i& size/*x,y,mipcount*/, const std::vector<ResourceFormat>& formats)
{
	//BOOST_ASSERT(IsCurrentInMainThread());
	BOOST_ASSERT(res);
	BOOST_ASSERT(formats.size() >= 1);

	FrameBuffer11Ptr framebuffer = std::static_pointer_cast<FrameBuffer11>(res);
	framebuffer->SetSize(size.head<2>());

	int colorCount = IF_AND_OR(IsDepthStencil(formats.back()) || formats.back() == kFormatUnknown, formats.size() - 1, formats.size());
	for (size_t i = 0; i < colorCount; ++i)
		framebuffer->SetAttachColor(i, FrameBuffer11AttachFactory::CreateColorAttachment(mDevice, size, formats[i]));
	if (colorCount != formats.size())
		framebuffer->SetAttachZStencil(FrameBuffer11AttachFactory::CreateZStencilAttachment(mDevice, size.head<2>(), formats.back()));

#if defined MIR_RESOURCE_DEBUG
	int idx = 0;
	for (auto rtv : framebuffer->AsRTVs())
		DEBUG_RES_ADD_DEVICE(framebuffer, rtv, "rtv" + boost::lexical_cast<std::string>(idx++));
	idx = 0;
	for (auto srv : framebuffer->AsSRVs())
		DEBUG_RES_ADD_DEVICE(framebuffer, srv, "srv" + boost::lexical_cast<std::string>(idx++));
	DEBUG_RES_ADD_DEVICE(framebuffer, framebuffer->AsDSV(), "-1");
#endif
	return framebuffer;
}
void RenderSystem11::ClearFrameBuffer(IFrameBufferPtr fb, const Eigen::Vector4f& color, float depth, uint8_t stencil)
{
	DEBUG_LOG_CALLSTK("renderSys11.ClearFrameBuffer");
	BOOST_ASSERT(IsCurrentInMainThread());

	FrameBuffer11Ptr fb11 = fb ? std::static_pointer_cast<FrameBuffer11>(fb) : mCurFrameBuffer;
	for (auto& rtv : fb11->AsRTVs()) {
		if (rtv) {
			mDeviceContext->ClearRenderTargetView(rtv, (const float*)&color);
		}
	}
	if (fb11->AsDSV()) mDeviceContext->ClearDepthStencilView(fb11->AsDSV(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, depth, stencil);
}
void RenderSystem11::CopyFrameBuffer(IFrameBufferPtr dst, int dstAttachment, IFrameBufferPtr src, int srcAttachment)
{
	DEBUG_LOG_CALLSTK("renderSys11.CopyFrameBuffer");
	BOOST_ASSERT(IsCurrentInMainThread());

	FrameBuffer11Ptr fbDst = IF_AND_OR(dst, std::static_pointer_cast<FrameBuffer11>(dst), mBackFrameBuffer);
	FrameBuffer11Ptr fbSrc = IF_AND_OR(src, std::static_pointer_cast<FrameBuffer11>(src), mBackFrameBuffer);

	Texture11Ptr texDst = std::static_pointer_cast<Texture11>(IF_AND_OR(dstAttachment >= 0, fbDst->GetAttachColorTexture(dstAttachment), fbDst->GetAttachZStencilTexture()));
	Texture11Ptr texSrc = std::static_pointer_cast<Texture11>(IF_AND_OR(srcAttachment >= 0, fbSrc->GetAttachColorTexture(srcAttachment), fbSrc->GetAttachZStencilTexture()));

	mDeviceContext->CopySubresourceRegion(texDst->AsTex2D().Get(), 0, 0, 0, 0, texSrc->AsTex2D().Get(), 0, nullptr);
}
void RenderSystem11::SetFrameBuffer(IFrameBufferPtr fb)
{
	DEBUG_LOG_CALLSTK("renderSys11.SetFrameBuffer");
	BOOST_ASSERT(IsCurrentInMainThread());

	auto prevFbSize = mCurFrameBuffer->GetSize();

	auto fb11 = std::static_pointer_cast<FrameBuffer11>(fb);
	mCurFrameBuffer = fb11 ? fb11 : mBackFrameBuffer;

	if (mCurFrameBuffer->GetSize() != prevFbSize) {
		auto fbsize = mCurFrameBuffer->GetSize();
		SetViewPort(0, 0, fbsize.x(), fbsize.y());
	}

	std::vector<ID3D11RenderTargetView*> vecRTV = mCurFrameBuffer->AsRTVs();
	mDeviceContext->OMSetRenderTargets(vecRTV.size(), 
		vecRTV.empty() ? nullptr : &vecRTV[0], 
		mCurFrameBuffer->AsDSV());
}

IInputLayoutPtr RenderSystem11::LoadLayout(IResourcePtr res, IProgramPtr pProgram, const std::vector<LayoutInputElement>& descArr)
{
	//BOOST_ASSERT(IsCurrentInMainThread());
	BOOST_ASSERT(res && pProgram->IsLoaded());

	InputLayout11Ptr ret = std::static_pointer_cast<InputLayout11>(res);
	ret->mInputDescs.resize(descArr.size());
	for (size_t i = 0; i < descArr.size(); ++i) {
		const LayoutInputElement& descI = descArr[i];
		ret->mInputDescs[i] = D3D11_INPUT_ELEMENT_DESC {
			descI.SemanticName.c_str(),
			descI.SemanticIndex,
			static_cast<DXGI_FORMAT>(descI.Format),
			descI.InputSlot,
			descI.AlignedByteOffset,
			static_cast<D3D11_INPUT_CLASSIFICATION>(descI.InputSlotClass),
			descI.InstanceDataStepRate
		};
	}

	Program11Ptr program11 = std::static_pointer_cast<Program11>(pProgram);
	auto programBlob = program11->mVertex->GetBlob();
	if (CheckHR(mDevice->CreateInputLayout(&ret->mInputDescs[0], ret->mInputDescs.size(), programBlob->GetBytes(), programBlob->GetSize(), &ret->mLayout))) 
		return nullptr;

	return ret;
}
void RenderSystem11::SetVertexLayout(IInputLayoutPtr layout) 
{
	DEBUG_LOG_CALLSTK("renderSys11.SetVertexLayout");
	BOOST_ASSERT(IsCurrentInMainThread());

	mDeviceContext->IASetInputLayout(std::static_pointer_cast<InputLayout11>(layout)->GetLayout11().Get());
}

IBlobDataPtr RenderSystem11::CompileShader(const ShaderCompileDesc& compile, const Data& data)
{
	//BOOST_ASSERT(IsCurrentInMainThread());

	BlobData11Ptr blob = MakePtr<BlobData11>(nullptr);

	std::vector<D3D_SHADER_MACRO> shaderMacros(compile.Macros.size());
	if (!compile.Macros.empty()) {
		for (size_t i = 0; i < compile.Macros.size(); ++i) {
			const auto& cdm = compile.Macros[i];
			shaderMacros[i] = D3D_SHADER_MACRO{ cdm.Name.c_str(), cdm.Definition.c_str() };
		}
		shaderMacros.push_back(D3D_SHADER_MACRO{ NULL, NULL });
	}

	DWORD shaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(MIR_RENDERSYS_DEBUG)
	shaderFlags |= D3DCOMPILE_DEBUG;
#else
	shaderFlags |= D3DCOMPILE_OPTIMIZATION_LEVEL2;
#endif

	ComPtr<ID3DBlob> blobError = nullptr;
	HRESULT hr = D3DCompile(data.Bytes, data.Size, 
		IF_AND_NULL(!compile.SourcePath.empty(), compile.SourcePath.c_str()),
		IF_AND_NULL(!shaderMacros.empty(), &shaderMacros[0]), 
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		compile.EntryPoint.c_str(), compile.ShaderModel.c_str(),
		shaderFlags, 0,
		&blob->mBlob, &blobError);
	if (d3d::CheckCompileFailed(hr, blobError.Get())) return nullptr;

	return blob;
}
IShaderPtr RenderSystem11::CreateShader(int type, IBlobDataPtr data)
{
	//BOOST_ASSERT(IsCurrentInMainThread());
#if 0
	ID3D11ShaderReflection* pReflector = NULL;
	D3DReflect(data->GetBytes(), data->GetSize(), IID_ID3D11ShaderReflection, (void**)&pReflector);

	D3D11_SHADER_INPUT_BIND_DESC bindDesc;
	pReflector->GetResourceBindingDescByName("cbPerLight", &bindDesc);
#endif
	switch (type) {
	case kShaderVertex: {
		VertexShader11Ptr ret = MakePtr<VertexShader11>(data);
		if (d3d::CheckCompileFailed(mDevice->CreateVertexShader(data->GetBytes(), data->GetSize(), NULL, &ret->mShader), data))
			return nullptr;		
		return ret;
	}break;
	case kShaderPixel: {
		PixelShader11Ptr ret = MakePtr<PixelShader11>(data);
		if (d3d::CheckCompileFailed(mDevice->CreatePixelShader(data->GetBytes(), data->GetSize(), NULL, &ret->mShader), data)) 
			return nullptr;
		return ret;
	}break;
	default:
		break;
	}
	return nullptr;
}
IProgramPtr RenderSystem11::LoadProgram(IResourcePtr res, const std::vector<IShaderPtr>& shaders)
{
	//BOOST_ASSERT(IsCurrentInMainThread());
	BOOST_ASSERT(res);

	Program11Ptr program = std::static_pointer_cast<Program11>(res);
	for (auto& iter : shaders) {
		switch (iter->GetType()) {
		case kShaderVertex:
			program->SetVertex(std::static_pointer_cast<VertexShader11>(iter));
			break;
		case kShaderPixel:
			program->SetPixel(std::static_pointer_cast<PixelShader11>(iter));
			break;
		default:
			break;
		}
	}
	
	DEBUG_RES_ADD_DEVICE(program, NULLABLE(program->mVertex, GetShader11().Get()), "vs");
	DEBUG_RES_ADD_DEVICE(program, NULLABLE(program->mPixel, GetShader11().Get()), "ps");
	return program;
}
void RenderSystem11::SetProgram(IProgramPtr program)
{
	DEBUG_LOG_CALLSTK("renderSys11.SetProgram");
	BOOST_ASSERT(IsCurrentInMainThread());

	mDeviceContext->VSSetShader(NULLABLE(std::static_pointer_cast<VertexShader11>(program->GetVertex()), GetShader11().Get()), NULL, 0);
	mDeviceContext->PSSetShader(NULLABLE(std::static_pointer_cast<PixelShader11>(program->GetPixel()), GetShader11().Get()), NULL, 0);
}

IVertexBufferPtr RenderSystem11::LoadVertexBuffer(IResourcePtr res, IVertexArrayPtr vao, int stride, int offset, const Data& data)
{
	//BOOST_ASSERT(IsCurrentInMainThread());
	BOOST_ASSERT(res && vao);

	HWMemoryUsage usage = data.Bytes ? kHWUsageImmutable : kHWUsageDynamic;

	D3D11_BUFFER_DESC bd = {};
	bd.ByteWidth = data.Size;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	if (usage == kHWUsageDefault) {
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.CPUAccessFlags = 0;
	}
	else {
		bd.Usage = D3D11_USAGE_DYNAMIC;
		bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	}

	D3D11_SUBRESOURCE_DATA InitData = {};
	InitData.pSysMem = data.Bytes;

	ComPtr<ID3D11Buffer> pVertexBuffer = nullptr;
	if (CheckHR(mDevice->CreateBuffer(&bd, data.Bytes ? &InitData : nullptr, &pVertexBuffer))) return nullptr;

	VertexBuffer11Ptr vbuffer = std::static_pointer_cast<VertexBuffer11>(res);
	vbuffer->Init(std::move(pVertexBuffer), vao, data.Size, usage, stride, offset);
	return vbuffer;
}
void RenderSystem11::SetVertexBuffers(size_t slot, const IVertexBufferPtr vertexBuffers[], size_t count)
{
	DEBUG_LOG_CALLSTK("renderSys11.SetVertexBuffers");
	BOOST_ASSERT(IsCurrentInMainThread());
	BOOST_ASSERT(count >= 1);

	if (count == 1) {
		auto vertexBuffer = vertexBuffers[0];
		uint32_t stride = vertexBuffer->GetStride();
		uint32_t offset = vertexBuffer->GetOffset();
		mDeviceContext->IASetVertexBuffers(slot, count, 
			std::static_pointer_cast<VertexBuffer11>(vertexBuffer)->GetBuffer11().GetAddressOf(), &stride, &offset);
	}
	else {
		std::vector<uint32_t> strides(count), offsets(count);
		std::vector<ID3D11Buffer*> buffer11s(count);
		for (size_t i = 0; i < count; ++i) {
			auto vertexBuffer = vertexBuffers[i];
			strides[i] = vertexBuffer->GetStride();
			offsets[i] = vertexBuffer->GetOffset();
			buffer11s[i] = std::static_pointer_cast<VertexBuffer11>(vertexBuffer)->GetBuffer11().Get();
		}
		mDeviceContext->IASetVertexBuffers(slot, count, &buffer11s[0], &strides[0], &offsets[0]);
	}
}

IIndexBufferPtr RenderSystem11::LoadIndexBuffer(IResourcePtr res, IVertexArrayPtr vao, ResourceFormat format, const Data& data)
{
	//BOOST_ASSERT(IsCurrentInMainThread());
	BOOST_ASSERT(res && vao);

	HWMemoryUsage usage = data.Bytes ? kHWUsageImmutable : kHWUsageDynamic;

	D3D11_BUFFER_DESC bd = {};
	bd.ByteWidth = data.Size;
	bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	if (usage == kHWUsageDynamic) {
		bd.Usage = D3D11_USAGE_DYNAMIC;
		bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	}
	else {
		bd.Usage = D3D11_USAGE_IMMUTABLE;
		bd.CPUAccessFlags = 0;
	}

	D3D11_SUBRESOURCE_DATA InitData = {};
	InitData.pSysMem = data.Bytes;
	
	ComPtr<ID3D11Buffer> pIndexBuffer = nullptr;
	if (CheckHR(mDevice->CreateBuffer(&bd, data.Bytes ? &InitData : nullptr, &pIndexBuffer))) return nullptr;

	IndexBuffer11Ptr ibuffer = std::static_pointer_cast<IndexBuffer11>(res);
	ibuffer->Init(std::move(pIndexBuffer), vao, data.Size, format, usage);
	return ibuffer;
}
void RenderSystem11::SetIndexBuffer(IIndexBufferPtr indexBuffer)
{
	DEBUG_LOG_CALLSTK("renderSys11.SetIndexBuffer");
	BOOST_ASSERT(IsCurrentInMainThread());

	if (indexBuffer) {
		mDeviceContext->IASetIndexBuffer(
			std::static_pointer_cast<IndexBuffer11>(indexBuffer)->GetBuffer11().Get(),
			static_cast<DXGI_FORMAT>(indexBuffer->GetFormat()),
			0);
	}
	else {
		mDeviceContext->IASetIndexBuffer(nullptr, DXGI_FORMAT_R32_UINT, 0);
	}
}

IContantBufferPtr RenderSystem11::LoadConstBuffer(IResourcePtr res, const ConstBufferDecl& cbDecl, HWMemoryUsage usage, const Data& data)
{
	//BOOST_ASSERT(IsCurrentInMainThread());
	BOOST_ASSERT(res);

	D3D11_BUFFER_DESC cbDesc = {};
	cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cbDesc.ByteWidth = (cbDecl.BufferSize + 15) / 16 * 16;
	cbDesc.Usage = static_cast<D3D11_USAGE>(usage);
	if (usage == kHWUsageDefault || usage == kHWUsageImmutable) {
		BOOST_ASSERT(data.Bytes);
		cbDesc.CPUAccessFlags = 0;
	}
	else if (usage == kHWUsageDynamic) {
		cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	}
	else {
		BOOST_ASSERT(false);
	}

	D3D11_SUBRESOURCE_DATA initData = {};
	initData.pSysMem = data.Bytes;

	ComPtr<ID3D11Buffer> pConstantBuffer = nullptr;
	if (CheckHR(mDevice->CreateBuffer(&cbDesc, data.Bytes ? &initData : nullptr, &pConstantBuffer))) return nullptr;
	
	ContantBuffer11Ptr cbuffer = std::static_pointer_cast<ContantBuffer11>(res);
	cbuffer->Init(std::move(pConstantBuffer), CreateInstance<ConstBufferDecl>(cbDecl), usage);
	return cbuffer;
}
void RenderSystem11::SetConstBuffers(size_t slot, const IContantBufferPtr buffers[], size_t count, IProgramPtr program)
{
	DEBUG_LOG_CALLSTK("renderSys11.SetConstBuffers");
	BOOST_ASSERT(IsCurrentInMainThread());

	std::vector<ID3D11Buffer*> passConstBuffers(count);
	for (size_t i = 0; i < count; ++i)
		passConstBuffers[i] = NULLABLE(std::static_pointer_cast<ContantBuffer11>(buffers[i]), GetBuffer11().Get());
	mDeviceContext->VSSetConstantBuffers(slot, count, &passConstBuffers[0]);
	mDeviceContext->PSSetConstantBuffers(slot, count, &passConstBuffers[0]);
}

bool RenderSystem11::UpdateBuffer(IHardwareBufferPtr buffer, const Data& data)
{
	DEBUG_LOG_CALLSTK("renderSys11.UpdateBuffer");
	BOOST_ASSERT(IsCurrentInMainThread());
	BOOST_ASSERT(buffer);

	D3D11_MAPPED_SUBRESOURCE mapData;
	switch (buffer->GetType()) {
	case kHWBufferConstant: {
		ContantBuffer11Ptr cbuffer11 = std::static_pointer_cast<ContantBuffer11>(buffer);
		if (cbuffer11->GetUsage() == kHWUsageDefault) {
			mDeviceContext->UpdateSubresource(cbuffer11->GetBuffer11().Get(), 0, NULL, data.Bytes, 0, 0);
		}
		else {
			if (CheckHR(mDeviceContext->Map(cbuffer11->GetBuffer11().Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapData))) return false;
			memcpy(mapData.pData, data.Bytes, data.Size);
			mDeviceContext->Unmap(cbuffer11->GetBuffer11().Get(), 0);
		}
	}break;
	case kHWBufferVertex: {
		VertexBuffer11Ptr cbuffer11 = std::static_pointer_cast<VertexBuffer11>(buffer);
		if (CheckHR(mDeviceContext->Map(cbuffer11->GetBuffer11().Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapData))) return false;
		memcpy(mapData.pData, data.Bytes, std::min<int>(buffer->GetBufferSize(), data.Size));
		mDeviceContext->Unmap(cbuffer11->GetBuffer11().Get(), 0);
	}break;
	case kHWBufferIndex: {
		IndexBuffer11Ptr cbuffer11 = std::static_pointer_cast<IndexBuffer11>(buffer);
		if (CheckHR(mDeviceContext->Map(cbuffer11->GetBuffer11().Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapData))) return false;
		memcpy(mapData.pData, data.Bytes, std::min<int>(buffer->GetBufferSize(), data.Size));
		mDeviceContext->Unmap(cbuffer11->GetBuffer11().Get(), 0);	
	}break;
	default:
		break;
	}
	return true;
}

ITexturePtr RenderSystem11::LoadTexture(IResourcePtr res, ResourceFormat format, const Eigen::Vector4i& size/*w_h_step_face*/, int mipCount, const Data2 datas[])
{
	DEBUG_LOG_CALLSTK("renderSys11.LoadTexture");
	BOOST_ASSERT(IsCurrentInMainThread());
	BOOST_ASSERT(res);

	Data2 defaultData = Data2{};
	datas = datas ? datas : &defaultData;
	const HWMemoryUsage usage = datas[0].Bytes ? kHWUsageDefault : kHWUsageDynamic;

	Texture11Ptr texture = std::static_pointer_cast<Texture11>(res);
	texture->Init(format, usage, size.x(), size.y(), size.w(), mipCount);

	mipCount = texture->GetMipmapCount();
	const bool autoGen = texture->IsAutoGenMipmap();
	const size_t faceCount = texture->GetFaceCount();
	constexpr int imageSize = 0;//only used for 3d textures
	BOOST_ASSERT_IF_THEN(faceCount > 1, datas[0].Bytes);
	BOOST_ASSERT_IF_THEN(autoGen, datas[0].Bytes && faceCount == 1);

	std::vector<D3D11_SUBRESOURCE_DATA> initDatas(mipCount * faceCount, D3D11_SUBRESOURCE_DATA{});
	for (size_t face = 0; face < faceCount; ++face) {
		for (size_t mip = 0; mip < mipCount; ++mip) {
			size_t index = face * mipCount + mip;
			const Data2& data = datas[index];
			D3D11_SUBRESOURCE_DATA& res_data = initDatas[index];
			res_data.pSysMem = data.Bytes;
			res_data.SysMemPitch = data.Stride ? data.Stride : BytePerPixel(format) * (size.x() >> mip);//Line width in bytes
			res_data.SysMemSlicePitch = imageSize; //only used for 3d textures
		}
	}
	if (!texture->InitTex(mDevice, (datas[0].Bytes && !autoGen) ? &initDatas[0] : nullptr)) 
		return nullptr; 

	if (!texture->InitSRV(mDevice)) 
		return nullptr; 

	if (texture->IsAutoGenMipmap()) {
		mDeviceContext->UpdateSubresource(texture->AsTex2D().Get(), 0, nullptr, datas[0].Bytes, datas[0].Stride, imageSize);
		mDeviceContext->GenerateMips(texture->AsSRV().Get());
	}

	DEBUG_RES_ADD_DEVICE(texture, texture->AsSRV().Get(), "");
	return texture;
}
void RenderSystem11::GenerateMips(ITexturePtr res)
{
	DEBUG_LOG_CALLSTK("renderSys11.GenerateMips");
	BOOST_ASSERT(IsCurrentInMainThread());
	BOOST_ASSERT(res);

	Texture11Ptr texture = std::static_pointer_cast<Texture11>(res);
	mDeviceContext->GenerateMips(texture->AsSRV().Get());
}
static inline std::vector<ID3D11ShaderResourceView*> GetTextureViews11(const ITexturePtr textures[], size_t count) {
	std::vector<ID3D11ShaderResourceView*> views(count);
	for (int i = 0; i < views.size(); ++i) {
		auto iTex = std::static_pointer_cast<Texture11>(textures[i]);
		if (iTex != nullptr) views[i] = iTex->AsSRV().Get();
	}
	return views;
}
void RenderSystem11::SetTextures(size_t slot, const ITexturePtr textures[], size_t count)
{
	DEBUG_LOG_CALLSTK("renderSys11.SetTextures");
	BOOST_ASSERT(IsCurrentInMainThread());

	std::vector<ID3D11ShaderResourceView*> texViews = GetTextureViews11(textures, count);
	mDeviceContext->PSSetShaderResources(slot, texViews.size(), !texViews.empty() ? &texViews[0] : nullptr);
}
bool RenderSystem11::LoadRawTextureData(ITexturePtr tex, char* data, int dataSize, int dataStep)
{
	return true;
}

ISamplerStatePtr RenderSystem11::LoadSampler(IResourcePtr res, const SamplerDesc& desc)
{
	//BOOST_ASSERT(IsCurrentInMainThread());
	BOOST_ASSERT(res);
	
	D3D11_SAMPLER_DESC sampDesc = {};
	sampDesc.Filter = static_cast<D3D11_FILTER>(desc.Filter);
	sampDesc.AddressU = static_cast<D3D11_TEXTURE_ADDRESS_MODE>(desc.AddressU);
	sampDesc.AddressV = static_cast<D3D11_TEXTURE_ADDRESS_MODE>(desc.AddressV);
	sampDesc.AddressW = static_cast<D3D11_TEXTURE_ADDRESS_MODE>(desc.AddressW);
	sampDesc.MipLODBias = 0.0f;
	sampDesc.MaxAnisotropy = (desc.Filter == D3D11_FILTER_ANISOTROPIC) ? D3D11_REQ_MAXANISOTROPY : 1;
	sampDesc.ComparisonFunc = static_cast<D3D11_COMPARISON_FUNC>(desc.CmpFunc);
	sampDesc.BorderColor[0] = 1.e30f;
	sampDesc.BorderColor[1] = sampDesc.BorderColor[2] = sampDesc.BorderColor[3] = 1.0;
	sampDesc.MinLOD = FLT_MIN;
	sampDesc.MaxLOD = FLT_MAX;

	ComPtr<ID3D11SamplerState> pSampler = nullptr;
	if (CheckHR(mDevice->CreateSamplerState(&sampDesc, &pSampler)))
		return nullptr;

	SamplerState11Ptr sampler = std::static_pointer_cast<SamplerState11>(res);
	sampler->Init(std::move(pSampler));
#if defined MIR_RESOURCE_DEBUG
	sampler->mDesc = desc;
#endif
	return sampler;
}
void RenderSystem11::SetSamplers(size_t slot, const ISamplerStatePtr samplers[], size_t count)
{
	DEBUG_LOG_CALLSTK("renderSys11.SetSamplers");
	BOOST_ASSERT(IsCurrentInMainThread());
	BOOST_ASSERT(count >= 0);

	std::vector<ID3D11SamplerState*> passSamplers(count);
	for (size_t i = 0; i < count; ++i)
		passSamplers[i] = NULLABLE(std::static_pointer_cast<SamplerState11>(samplers[i]), GetSampler11().Get());
	mDeviceContext->PSSetSamplers(0, passSamplers.size(), IF_AND_NULL(!passSamplers.empty(), &passSamplers[0]));
}

bool RenderSystem11::_SetBlendState(const BlendState& blendFunc)
{
	ComPtr<ID3D11BlendState> pBlendState;
	auto iter = mDxBlendStates.find(blendFunc);
	if (iter != mDxBlendStates.end()) {
		pBlendState = iter->second;
	}
	else {
		D3D11_BLEND_DESC blendDesc = { 0 };
		blendDesc.RenderTarget[0].BlendEnable = (blendFunc.Src != kBlendOne) || (blendFunc.Dst != kBlendZero);
		blendDesc.RenderTarget[0].SrcBlend = static_cast<D3D11_BLEND>(blendFunc.Src);
		blendDesc.RenderTarget[0].DestBlend = static_cast<D3D11_BLEND>(blendFunc.Dst);
		blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
		blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
		blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		if (CheckHR(mDevice->CreateBlendState(&blendDesc, &pBlendState)))
			return false;
		mDxBlendStates.insert(std::make_pair(blendFunc, pBlendState));
	}

	float blendFactor[] = { 0.0f, 0.0f, 0.0f, 0.0f };
	mDeviceContext->OMSetBlendState(pBlendState.Get(), blendFactor, 0xffffffff);
	return true;
}
void RenderSystem11::SetBlendState(const BlendState& blendFunc)
{
	DEBUG_LOG_CALLSTK("renderSys11.SetBlendState");
	BOOST_ASSERT(IsCurrentInMainThread());

	if (mCurBlendState != blendFunc) {
		mCurBlendState = blendFunc;
		_SetBlendState(mCurBlendState);
	}
}

bool RenderSystem11::_SetDepthState(const DepthState& depthState)
{
	ComPtr<ID3D11DepthStencilState> pDSState;
	auto iter = mDxDSStates.find(depthState);
	if (iter != mDxDSStates.end()) {
		pDSState = iter->second;
	}
	else {
		D3D11_DEPTH_STENCIL_DESC desc = {};
		desc.DepthEnable = depthState.DepthEnable;
		desc.DepthWriteMask = static_cast<D3D11_DEPTH_WRITE_MASK>(depthState.WriteMask);
		desc.DepthFunc = static_cast<D3D11_COMPARISON_FUNC>(depthState.CmpFunc);
		desc.StencilEnable = FALSE;
		if (CheckHR(mDevice->CreateDepthStencilState(&desc, &pDSState)))
			return false;
		mDxDSStates.insert(std::make_pair(depthState, pDSState));
	}

	mDeviceContext->OMSetDepthStencilState(pDSState.Get(), 1);
	return true;
}
void RenderSystem11::SetDepthState(const DepthState& depthState)
{
	DEBUG_LOG_CALLSTK("renderSys11.SetDepthState");
	BOOST_ASSERT(IsCurrentInMainThread());

	if (mCurDepthState != depthState) {
		mCurDepthState = depthState;
		_SetDepthState(mCurDepthState);
	}
}

bool RenderSystem11::_SetRasterizerState(const RasterizerState& rasterState)
{
	ComPtr<ID3D11RasterizerState> pRasterizerState;
	auto iter = mDxRasterStates.find(rasterState);
	if (iter != mDxRasterStates.end()) {
		pRasterizerState = iter->second;
	}
	else {
		D3D11_RASTERIZER_DESC desc = {};
		desc.FillMode = static_cast<D3D11_FILL_MODE>(rasterState.FillMode);
		desc.CullMode = static_cast<D3D11_CULL_MODE>(rasterState.CullMode);
		desc.SlopeScaledDepthBias = rasterState.DepthBias.Bias;
		desc.DepthBias = rasterState.DepthBias.SlopeScaledBias;
		desc.DepthClipEnable = FALSE;
		desc.ScissorEnable = rasterState.Scissor.ScissorEnable;
		if (CheckHR(mDevice->CreateRasterizerState(&desc, &pRasterizerState)))
			return false;
		mDxRasterStates.insert(std::make_pair(rasterState, pRasterizerState));
	}

	mDeviceContext->RSSetState(pRasterizerState.Get());

	D3D11_RECT rect;
	rect.left = rasterState.Scissor.Rect.x();
	rect.top = rasterState.Scissor.Rect.y();
	rect.right = rasterState.Scissor.Rect.z();
	rect.bottom = rasterState.Scissor.Rect.w();
	mDeviceContext->RSSetScissorRects(1, &rect);
	return true;
}
void RenderSystem11::SetCullMode(CullMode cullMode)
{
	DEBUG_LOG_CALLSTK("renderSys11.SetCullMode");
	BOOST_ASSERT(IsCurrentInMainThread());

	if (mCurRasterState.CullMode != cullMode) {
		mCurRasterState.CullMode = cullMode;
		_SetRasterizerState(mCurRasterState);
	}
}
void RenderSystem11::SetFillMode(FillMode fillMode)
{
	DEBUG_LOG_CALLSTK("renderSys11.SetFillMode");
	BOOST_ASSERT(IsCurrentInMainThread());

	if (mCurRasterState.FillMode != fillMode) {
		mCurRasterState.FillMode = fillMode;
		_SetRasterizerState(mCurRasterState);
	}
}
void RenderSystem11::SetDepthBias(const DepthBias& bias)
{
	DEBUG_LOG_CALLSTK("renderSys11.SetDepthBias");
	BOOST_ASSERT(IsCurrentInMainThread());

	if (mCurRasterState.DepthBias != bias) {
		mCurRasterState.DepthBias = bias;
		_SetRasterizerState(mCurRasterState);
	}
}
void RenderSystem11::SetScissorState(const ScissorState& scissor)
{
	DEBUG_LOG_CALLSTK("renderSys11.SetScissorState");
	BOOST_ASSERT(IsCurrentInMainThread());

	if (mCurRasterState.Scissor != scissor) {
		mCurRasterState.Scissor = scissor;
		_SetRasterizerState(mCurRasterState);
	}
}

void RenderSystem11::DrawPrimitive(const RenderOperation& op, PrimitiveTopology topo) {
	BOOST_ASSERT(IsCurrentInMainThread());

	mDeviceContext->IASetPrimitiveTopology(static_cast<D3D11_PRIMITIVE_TOPOLOGY>(topo));
	mDeviceContext->Draw(op.VertexBuffers[0]->GetCount(), 0);
}
void RenderSystem11::DrawIndexedPrimitive(const RenderOperation& op, PrimitiveTopology topo) {
	BOOST_ASSERT(IsCurrentInMainThread());

	mDeviceContext->IASetPrimitiveTopology(static_cast<D3D11_PRIMITIVE_TOPOLOGY>(topo));
	int indexCount = op.IndexCount != 0 ? op.IndexCount : op.IndexBuffer->GetBufferSize() / op.IndexBuffer->GetWidth();
	mDeviceContext->DrawIndexed(indexCount, op.IndexPos, op.IndexBase);
}

bool RenderSystem11::BeginScene()
{
	return true;
}
void RenderSystem11::EndScene(BOOL vsync)
{
	mSwapChain->Present(vsync, 0);
}

}