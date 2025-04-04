#pragma once
#include <windows.h>
#include <d3d11.h>
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;
#include "core/mir_config.h"
#include "core/rendersys/sampler.h"
#include "core/rendersys/texture.h"

namespace mir {

class SamplerState11 : public ImplementResource<ISamplerState> 
{
public:
	MIR_MAKE_ALIGNED_OPERATOR_NEW;
	void Init(ComPtr<ID3D11SamplerState>&& sampler) { mSampler = std::move(sampler); }
	ComPtr<ID3D11SamplerState>& GetSampler11() { return mSampler; }
public:
	ComPtr<ID3D11SamplerState> mSampler = nullptr;
#if defined MIR_RESOURCE_DEBUG
	SamplerDesc mDesc;
#endif
};

class Texture11 : public ImplementResource<ITexture>
{
public:
	MIR_MAKE_ALIGNED_OPERATOR_NEW;
	Texture11();
	~Texture11();
	void Init(ResourceFormat format, HWMemoryUsage usage, int width, int height, int faceCount, int mipmap, bool bindRTOrDS = false);
	void Init(ComPtr<ID3D11Texture2D>&& tex2d);
	const ComPtr<ID3D11Texture2D>& InitTex(const ComPtr<ID3D11Device>& device, const D3D11_SUBRESOURCE_DATA datas[] = nullptr);
	const ComPtr<ID3D11ShaderResourceView>& InitSRV(const ComPtr<ID3D11Device>& device);
	const ComPtr<ID3D11RenderTargetView>& InitRTV(const ComPtr<ID3D11Device>& device);
	const ComPtr<ID3D11DepthStencilView>& InitDSV(const ComPtr<ID3D11Device>& device);
public:
	const ComPtr<ID3D11Texture2D>& AsTex2D() const { return mTex2D; }
	const ComPtr<ID3D11ShaderResourceView>& AsSRV() const { return mSRV; }
	const ComPtr<ID3D11RenderTargetView>& AsRTV() const { return mRTV; }
	const ComPtr<ID3D11DepthStencilView>& AsDSV() const { return mDSV; }
public:
	ResourceFormat GetFormat() const override { return mFormat; }
	HWMemoryUsage GetUsage() const override { return mUsage; }
	Eigen::Vector2i GetSize() const override { return mSize; }
	Eigen::Vector2i GetRealSize() const override { return mRealSize; }
	int GetMipmapCount() const override { return mMipCount; }
	int GetFaceCount() const override { return mFaceCount; }
	bool IsAutoGenMipmap() const override { return mAutoGenMipmap; }

	void OnLoaded() override;
private:
	bool mAutoGenMipmap, mBindRTOrDS;
	int mFaceCount, mMipCount;
	Eigen::Vector2i mSize, mRealSize;
	ResourceFormat mFormat;
	HWMemoryUsage mUsage;
	ComPtr<ID3D11Texture2D> mTex2D;
	ComPtr<ID3D11ShaderResourceView> mSRV;
	ComPtr<ID3D11RenderTargetView> mRTV;
	ComPtr<ID3D11DepthStencilView> mDSV;
};

}