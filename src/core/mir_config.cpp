#include "mir_config.h"
#include "core/rendersys/base/platform.h"
#include "core/mir_config_macros.h"

namespace mir {

static_assert(PLATFORM_DIRECTX == kPlatformDirectx);
static_assert(PLATFORM_OPENGL == kPlatformOpengl);

Configure::Configure()
{
	_SHADOW_MODE = SHADOW_MODE;
	_REVERSE_Z = REVERSE_Z;
	_COLORSPACE = COLORSPACE;
	_DEBUG_CHANNEL = DEBUG_CHANNEL;
}

bool Configure::IsShadowVSM() const 
{ 
	return _SHADOW_MODE == SHADOW_VSM;
}

bool Configure::IsGammaSpace() const
{
	return _COLORSPACE == COLORSPACE_GAMMA;
}

void Configure::SetDebugChannel(int debugChannel)
{
	_DEBUG_CHANNEL = debugChannel;
}

}