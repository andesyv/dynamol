#pragma once
#include "Renderer.h"
#include <memory>

#include <glm/glm.hpp>
#include <glbinding/gl/gl.h>
#include <glbinding/gl/enum.h>
#include <glbinding/gl/functions.h>

#include <globjects/VertexArray.h>
#include <globjects/VertexAttributeBinding.h>
#include <globjects/Buffer.h>
#include <globjects/Program.h>
#include <globjects/Shader.h>
#include <globjects/Framebuffer.h>
#include <globjects/Renderbuffer.h>
#include <globjects/Texture.h>
#include <globjects/base/File.h>
#include <globjects/TextureHandle.h>
#include <globjects/NamedString.h>
#include <globjects/base/StaticStringSource.h>

namespace dynamol
{
	class Viewer;

	class ScalableRenderer : public Renderer
	{
	public:
		ScalableRenderer(Viewer *viewer);
		virtual void display();

	private:
		// Screen Spaced Vertex Array Object
		globjects::VertexArray m_ssvao{};
		globjects::Buffer m_ssvbo{};
		globjects::Buffer m_atompos{};

		globjects::Texture m_framebufferPositionTexture{};
		globjects::Texture m_framebufferCountTexture{};
		globjects::Framebuffer m_framebuffer{};
		globjects::Buffer m_staticpos{};
		globjects::VertexArray m_atomvao{};

		glm::ivec2 screenSize{64, 64};
	};

}