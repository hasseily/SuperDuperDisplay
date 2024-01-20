#include "PostProcessor.h"
#include "OpenGLHelper.h"

// below because "The declaration of a static data member in its class definition is not a definition"
PostProcessor* PostProcessor::s_instance;

static OpenGLHelper* oglHelper;

//////////////////////////////////////////////////////////////////////////
// Basic singleton methods
//////////////////////////////////////////////////////////////////////////

void PostProcessor::Initialize()
{

}

PostProcessor::~PostProcessor()
{

}

//////////////////////////////////////////////////////////////////////////
// Main methods
//////////////////////////////////////////////////////////////////////////

void PostProcessor::Render()
{
	/*
		libretro shaders have the following uniforms that have to be set:
		BOTH
		uniform vec2 TextureSize;	// Size of the incoming framebuffer

		VECTOR
		uniform mat4 MVPMatrix;		// Transform matrix
		uniform vec2 InputSize;		// Same as TextureSize (??)
		uniform vec2 OutputSize;	// Outgoing framebuffer screen size

		FRAGMENT
		uniform sampler2D Texture;	// Incoming framebuffer
	*
	*/
	if (oglHelper == nullptr)
		oglHelper = OpenGLHelper::GetInstance();

}

