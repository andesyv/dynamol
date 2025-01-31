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

	class ImageDepthScaleRenderer : public Renderer
	{
	public:
		ImageDepthScaleRenderer(Viewer *viewer);
		virtual void display();

	private:
		
		std::vector< std::unique_ptr<globjects::Buffer> > m_vertices;
		std::unique_ptr<globjects::VertexArray> m_vao = std::make_unique<globjects::VertexArray>();
		std::unique_ptr<globjects::Buffer> m_elementColorsRadii = std::make_unique<globjects::Buffer>();
		std::unique_ptr<globjects::Buffer> m_residueColors = std::make_unique<globjects::Buffer>();
		std::unique_ptr<globjects::Buffer> m_chainColors = std::make_unique<globjects::Buffer>();
		
		std::unique_ptr<globjects::VertexArray> m_vaoQuad = std::make_unique<globjects::VertexArray>();
		std::unique_ptr<globjects::Buffer> m_verticesQuad = std::make_unique<globjects::Buffer>();
		
		std::unique_ptr<globjects::StaticStringSource> m_shaderSourceDefines = nullptr;
		std::unique_ptr<globjects::NamedString> m_shaderDefines = nullptr;

		std::unique_ptr<globjects::Buffer> m_intersectionBuffer = std::make_unique<globjects::Buffer>();
		std::unique_ptr<globjects::Buffer> m_statisticsBuffer = std::make_unique<globjects::Buffer>();
		std::unique_ptr<globjects::Texture> m_offsetTexture = nullptr;
		std::array<std::unique_ptr<globjects::Texture>, 2> m_depthTexture;
		std::unique_ptr<globjects::Texture> m_spherePositionTexture = nullptr;
		std::unique_ptr<globjects::Texture> m_sphereNormalTexture = nullptr;
		std::array<std::unique_ptr<globjects::Texture>, 2> m_surfacePositionTexture;
		std::array<std::unique_ptr<globjects::Texture>, 2> m_surfaceNormalTexture;
		std::unique_ptr<globjects::Texture> m_sphereDiffuseTexture{};
		std::array<std::unique_ptr<globjects::Texture>, 2> m_surfaceDiffuseTexture;
		std::unique_ptr<globjects::Texture> m_ambientTexture = nullptr;
		std::unique_ptr<globjects::Texture> m_blurTexture = nullptr;
		std::unique_ptr<globjects::Texture> m_colorTexture = nullptr;
		std::unique_ptr<globjects::Texture> m_shadowColorTexture = nullptr;
		std::unique_ptr<globjects::Texture> m_shadowDepthTexture = nullptr;

		std::unique_ptr<globjects::Framebuffer> m_sphereFramebuffer = nullptr;
		std::array<std::unique_ptr<globjects::Framebuffer>, 2> m_surfaceFramebuffer;
		std::unique_ptr<globjects::Framebuffer> m_ambientFramebuffer = nullptr;
		std::unique_ptr<globjects::Framebuffer> m_shadeFramebuffer = nullptr;
		std::unique_ptr<globjects::Framebuffer> m_aoFramebuffer = nullptr;
		std::unique_ptr<globjects::Framebuffer> m_aoBlurFramebuffer = nullptr;
		std::unique_ptr<globjects::Framebuffer> m_dofFramebuffer = nullptr;
		std::unique_ptr<globjects::Framebuffer> m_dofBlurFramebuffer = nullptr;
		std::unique_ptr<globjects::Framebuffer> m_shadowFramebuffer = nullptr;

		std::vector< std::unique_ptr<globjects::Texture> > m_environmentTextures;
		std::vector< std::unique_ptr<globjects::Texture> > m_materialTextures;
		std::vector< std::unique_ptr<globjects::Texture> > m_bumpTextures;

		glm::ivec2 m_shadowMapSize = glm::ivec2(512, 512);
		glm::ivec2 m_framebufferSize;

		std::array<std::unique_ptr<globjects::Texture>, 2> m_LODDepthFramebuffer_Color;
		std::array<std::unique_ptr<globjects::Texture>, 2> m_LODDepthFramebuffer_Depth;
		std::array<std::unique_ptr<globjects::Framebuffer>, 2> m_LODDepthFramebuffer;

		std::unique_ptr<globjects::Buffer> m_sparseAtomVertices;
		gl::GLsizei m_sparseVertexCount;
	};

}