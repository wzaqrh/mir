#include <boost/assert.hpp>
#include "core/scene/light.h"
#include "core/scene/camera.h"

namespace mir {
namespace scene {

/********** Light **********/
Light::Light()
{}

void Light::SetColor(const Eigen::Vector3f& color)
{
	mCbLight.LightColor.head<3>() = color;
}

/********** LightCamera **********/
void LightCamera::Update(const Eigen::AlignedBox3f& sceneAABB)
{
	const Eigen::Vector3f& forward = Direction;
	const Eigen::Vector3f& eye = Position;
	
	Eigen::Quaternionf viewRot = Eigen::Quaternionf::FromTwoVectors(Eigen::Vector3f(0, 0, 1), forward);
	Eigen::Vector3f up = viewRot * Eigen::Vector3f(0, 1, 0);

	View = math::cam::MakeLookForwardLH(eye, forward, up);

	Transform3fAffine t(View);
	Eigen::AlignedBox3f frustum = sceneAABB.transformed(t);

	FrustumWidth = (std::max(fabs(frustum.min().x()), fabs(frustum.max().x()))) * 2;
	FrustumHeight = (std::max(fabs(frustum.min().y()), fabs(frustum.max().y()))) * 2;
	BOOST_ASSERT(FrustumWidth >= 0 && FrustumHeight >= 0);

	FrustumZNear = frustum.min().z();
	//FrustumZFar = __max(frustum.max().z(), 32);
	FrustumZFar = floor(frustum.max().z());

	if (IsSpotLight) ShadowCasterProj = math::cam::MakePerspectiveLH(FrustumWidth, FrustumHeight, FrustumZNear, FrustumZFar);
	else ShadowCasterProj = math::cam::MakeOrthographicOffCenterLH(-FrustumWidth * 0.5f, FrustumWidth * 0.5f, -FrustumHeight * 0.5f, FrustumHeight * 0.5f, FrustumZNear, FrustumZFar);
#if 0
	Eigen::Vector4f pos[2] = {
		Eigen::Vector4f(0,0,0,1),
		Eigen::Vector4f(0,0.5,0,1)
	};
	for (size_t i = 0; i < 2; ++i) {
		Eigen::Vector4f view = Transform3Projective(mView) * pos[i];
		Eigen::Vector4f perspective1 = Transform3Projective(mShadowCasterProj) * view;
		Eigen::Vector4f perspective = Transform3Projective(mShadowCasterProj * mView) * pos[i];
	}
#endif

#if 1
	ShadowRecvProj = ShadowCasterProj;
#else
	ShadowRecvProj = Transform3Projective(ShadowCasterProj)
		.prescale(Eigen::Vector3f(1, -1, 1))
		.matrix();
	ShadowRecvProj = Transform3Projective(ShadowRecvProj)
		.prescale(Eigen::Vector3f(0.5, 0.5, 1))
		.pretranslate(Eigen::Vector3f(0.5, 0.5, 0))
		.matrix();
#endif
}

/********** DirectLight **********/
DirectLight::DirectLight()
{}

void DirectLight::SetDirection(const Eigen::Vector3f& dir)
{
	mCamera.Direction = dir.normalized();
}

void DirectLight::SetPosition(const Eigen::Vector3f& pos)
{
	mCamera.Position = pos;
}

void DirectLight::SetLookAt(const Eigen::Vector3f& eye, const Eigen::Vector3f& at)
{
	SetPosition(eye);
	SetDirection(at - eye);
}

void DirectLight::SetLightRadius(float radius)
{
	mLightRadius = radius;
}

void DirectLight::SetMinPCFRadius(float minPcfRadius)
{
	mMinPcfRadius = minPcfRadius;
}

void DirectLight::UpdateLightCamera(const Eigen::AlignedBox3f& sceneAABB)
{
	mCbLight.LightPosition.head<3>() = -mCamera.Direction.normalized();
	mCbLight.LightPosition.w() = 0.0f;

	mCamera.Update(sceneAABB);
	mCbLight.LightRadiusUVNearFar = Eigen::Vector4f(mLightRadius / mCamera.FrustumWidth, mLightRadius / mCamera.FrustumHeight, mCamera.FrustumZNear, mCamera.FrustumZFar);
	mCbLight.LightDepthParam = Eigen::Vector4f(1 / mCamera.FrustumZNear, (mCamera.FrustumZNear - mCamera.FrustumZFar) / (mCamera.FrustumZFar * mCamera.FrustumZNear), mMinPcfRadius, 0);
}

/********** SpotLight **********/
SpotLight::SpotLight()
{
	mCamera.IsSpotLight = true;
	mCbLight.IsSpotLight = true;
	SetDirection(Eigen::Vector3f(0, 0, 1));
	SetAngle(30.0f / 180 * 3.14);
}

void SpotLight::SetCutOff(float cutoff)
{
	mCbLight.unity_LightAtten.x() = cutoff;
	mCbLight.unity_LightAtten.y() = 1.0f / (1.0f - cutoff);
}

void SpotLight::SetAngle(float radian)
{
	SetCutOff(cos(radian));
}

void SpotLight::UpdateLightCamera(const Eigen::AlignedBox3f& sceneAABB)
{
	mCbLight.LightPosition.head<3>() = mCamera.Position;
	mCbLight.LightPosition.w() = 1.0f;

	mCbLight.unity_SpotDirection.head<3>() = mCamera.Direction.normalized();

	mCamera.Update(sceneAABB);
	mCbLight.LightRadiusUVNearFar = Eigen::Vector4f(mLightRadius / mCamera.FrustumWidth, mLightRadius / mCamera.FrustumHeight, mCamera.FrustumZNear, mCamera.FrustumZFar);
	mCbLight.LightDepthParam = Eigen::Vector4f(1 / mCamera.FrustumZNear, (mCamera.FrustumZNear - mCamera.FrustumZFar) / (mCamera.FrustumZFar * mCamera.FrustumZNear), mMinPcfRadius, 0);
}

/********** PointLight **********/
PointLight::PointLight()
{
	SetPosition(Eigen::Vector3f(0, 0, -10));
	SetAttenuation(0);
}

void PointLight::SetPosition(const Eigen::Vector3f& pos)
{
	mCbLight.LightPosition.head<3>() = pos;
	mCbLight.LightPosition.w() = 1.0f;
}

void PointLight::SetAttenuation(float c)
{
	mCbLight.unity_LightAtten.z() = c;
}

void PointLight::UpdateLightCamera(const Eigen::AlignedBox3f& sceneAABB)
{

}

/********** LightFactory **********/
DirectLightPtr LightFactory::CreateDirectLight()
{
	DirectLightPtr light = CreateInstance<DirectLight>();
	DoAddLight(light);
	return light;
}

PointLightPtr LightFactory::CreatePointLight()
{
	PointLightPtr light = CreateInstance<PointLight>();
	DoAddLight(light);
	return light;
}

SpotLightPtr LightFactory::CreateSpotLight()
{
	SpotLightPtr light = CreateInstance<SpotLight>();
	DoAddLight(light);
	return light;
}

void LightFactory::DoAddLight(scene::LightPtr light)
{
	light->SetCameraMask(mDefCameraMask);
#if LIGHT_FAC_CACHE
	mLights.push_back(light);
	mLightsDirty = true;
#endif
}

#if LIGHT_FAC_CACHE
void LightFactory::ResortLights()
{
	if (mLightsDirty) {
		mLightsDirty = false;

		struct CompLightByType {
			bool operator()(const LightPtr& l, const LightPtr& r) const {
				return l->GetType() < r->GetType();
			}
		};
		std::sort(mLights.begin(), mLights.end(), CompLightByType());
	}
}
#endif

}
}