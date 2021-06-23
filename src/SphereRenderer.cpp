#include "SphereRenderer.h"
#include <globjects/base/File.h>
#include <globjects/State.h>
#include <iostream>
#include <filesystem>
#include <imgui.h>
#include "Viewer.h"
#include "Scene.h"
#include "Protein.h"
#include <sstream>
#include <array>
#include <tuple>
#include <utility>
#include <functional>
#include "enumerate.h"

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

using namespace dynamol;
using namespace gl;
using namespace glm;
using namespace globjects;

// https://stackoverflow.com/questions/1505675/power-of-an-integer-in-c
int pow(int a, unsigned int p) {
	if (p == 0) return 1;
	if (p == 1) return a;

	int tmp = pow(a, p/2);
	if (p%2 == 0) return tmp * tmp;
	else return a * tmp * tmp;
}

std::unique_ptr<Texture> SphereRenderer::loadTexture(const std::string& filename)
{
	int width, height, channels;

	stbi_set_flip_vertically_on_load(true);
	unsigned char* data = stbi_load(filename.c_str(), &width, &height, &channels, 0);

	if (data)
	{
		std::cout << "Loaded " << filename << std::endl;

		auto texture = Texture::create(GL_TEXTURE_2D);
		texture->setParameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		texture->setParameter(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		texture->setParameter(GL_TEXTURE_WRAP_S, GL_REPEAT);
		texture->setParameter(GL_TEXTURE_WRAP_T, GL_REPEAT);

		GLenum format = GL_RGBA;

		switch (channels)
		{
		case 1:
			format = GL_RED;
			break;

		case 2:
			format = GL_RG;
			break;

		case 3:
			format = GL_RGB;
			break;

		case 4:
			format = GL_RGBA;
			break;
		}

		texture->image2D(0, format, ivec2(width, height), 0, format, GL_UNSIGNED_BYTE, data);
		texture->generateMipmap();

		stbi_image_free(data);

		return texture;
	}

	return std::unique_ptr<Texture>();
}

template <typename T, std::size_t N>
auto bezier(std::array<T, N>&& controlPoints, float t) {
	for (unsigned int k{N-1}; 0 < k; --k)
		for (unsigned int i{0}; i < k; ++i)
			controlPoints.at(i) = std::lerp(controlPoints.at(i), controlPoints.at(i+1), t);
	return controlPoints.at(0);
}

template <typename T, std::size_t N>
auto bezier(T (&&controlPoints)[N], float t) {
	return bezier(std::to_array<T, N>(controlPoints), t);
}

SphereRenderer::SphereRenderer(Viewer* viewer) : Renderer(viewer), gridSize{2}, gridDepth{8}
{
	Shader::hintIncludeImplementation(Shader::IncludeImplementation::Fallback);

	for (auto i : viewer->scene()->protein()->atoms())
	{
		m_vertices.push_back(Buffer::create());
		m_vertices.back()->setStorage(i, gl::GL_NONE_BIT);
	}

	m_elementColorsRadii->setStorage(viewer->scene()->protein()->activeElementColorsRadiiPacked(), gl::GL_NONE_BIT);
	m_residueColors->setStorage(viewer->scene()->protein()->activeResidueColorsPacked(), gl::GL_NONE_BIT);
	m_chainColors->setStorage(viewer->scene()->protein()->activeChainColorsPacked(), gl::GL_NONE_BIT);

	// Note: 1024 * 1024 * 128 seems like a very arbitrary number
	for (auto& ptr : m_intersectionBuffer) ptr->setStorage(sizeof(vec3) * 1024 * 1024 * 128, nullptr, gl::GL_NONE_BIT);
	for (auto& ptr : m_intersectionCount) ptr->setStorage(sizeof(uint), gl::GL_NONE_BIT);

	m_verticesQuad->setStorage(std::array<vec3, 1>({ vec3(0.0f, 0.0f, 0.0f) }), gl::GL_NONE_BIT);
	auto vertexBindingQuad = m_vaoQuad->binding(0);
	vertexBindingQuad->setBuffer(m_verticesQuad.get(), 0, sizeof(vec3));
	vertexBindingQuad->setFormat(3, GL_FLOAT);
	m_vaoQuad->enable(0);
	m_vaoQuad->unbind();

	m_shaderSourceDefines = StaticStringSource::create("");
	m_shaderDefines = NamedString::create("/defines", m_shaderSourceDefines.get());

	createShaderProgram("sphere", {
			{ GL_VERTEX_SHADER,"./res/sphere/sphere-vs.glsl" },
			{ GL_GEOMETRY_SHADER,"./res/sphere/sphere-gs.glsl" },
			{ GL_FRAGMENT_SHADER,"./res/sphere/sphere-fs.glsl" },
		},
		{ "./res/model/globals.glsl" });
	
	createShaderProgram("layer-sphere", {
			{ GL_VERTEX_SHADER,"./res/sphere/sphere-vs.glsl" },
			{ GL_GEOMETRY_SHADER,"./res/sphere/sphere-gs.glsl" },
			{ GL_FRAGMENT_SHADER,"./res/sphere/layer-sphere-fs.glsl" },
		},
		{ "./res/model/globals.glsl" });

	createShaderProgram("spawn", {
			{ GL_VERTEX_SHADER,"./res/sphere/sphere-vs.glsl" },
			{ GL_GEOMETRY_SHADER,"./res/sphere/sphere-gs.glsl" },
			{ GL_FRAGMENT_SHADER,"./res/sphere/spawn-fs.glsl" },
		},
		{ "./res/sphere/globals.glsl" });

	createShaderProgram("spawn-iteration", {
			{ GL_VERTEX_SHADER,"./res/scaling/normalize-grid-vs.glsl" },
			{ GL_GEOMETRY_SHADER,"./res/sphere/sphere-gs.glsl" },
			{ GL_FRAGMENT_SHADER,"./res/sphere/spawn-fs.glsl" },
		},
		{ "./res/sphere/globals.glsl" });

	createShaderProgram("surface", {
			{ GL_VERTEX_SHADER,"./res/sphere/image-vs.glsl" },
			{ GL_GEOMETRY_SHADER,"./res/sphere/image-gs.glsl" },
			{ GL_FRAGMENT_SHADER,"./res/sphere/surface-fs.glsl" },
		},
		{ "./res/sphere/globals.glsl" });

	createShaderProgram("aosample", {
			{ GL_VERTEX_SHADER,"./res/sphere/image-vs.glsl" },
			{ GL_GEOMETRY_SHADER,"./res/sphere/image-gs.glsl" },
			{ GL_FRAGMENT_SHADER,"./res/sphere/aosample-fs.glsl" },
		},
		{ "./res/sphere/globals.glsl" });

	createShaderProgram("aoblur", {
			{ GL_VERTEX_SHADER,"./res/sphere/image-vs.glsl" },
			{ GL_GEOMETRY_SHADER,"./res/sphere/image-gs.glsl" },
			{ GL_FRAGMENT_SHADER,"./res/sphere/aoblur-fs.glsl" },
		},
		{ "./res/sphere/globals.glsl" });

	createShaderProgram("shade", {
			{ GL_VERTEX_SHADER,"./res/sphere/image-vs.glsl" },
			{ GL_GEOMETRY_SHADER,"./res/sphere/image-gs.glsl" },
			{ GL_FRAGMENT_SHADER,"./res/sphere/shade-fs.glsl" },
		},
		{ "./res/sphere/globals.glsl" });

	createShaderProgram("dofblur", {
			{ GL_VERTEX_SHADER,"./res/sphere/image-vs.glsl" },
			{ GL_GEOMETRY_SHADER,"./res/sphere/image-gs.glsl" },
			{ GL_FRAGMENT_SHADER,"./res/sphere/dofblur-fs.glsl" },
		},
		{ "./res/sphere/globals.glsl" });

	createShaderProgram("dofblend", {
			{ GL_VERTEX_SHADER,"./res/sphere/image-vs.glsl" },
			{ GL_GEOMETRY_SHADER,"./res/sphere/image-gs.glsl" },
			{ GL_FRAGMENT_SHADER,"./res/sphere/dofblend-fs.glsl" },
		},
		{ "./res/sphere/globals.glsl" });

	createShaderProgram("display", {
			{ GL_VERTEX_SHADER,"./res/sphere/image-vs.glsl" },
			{ GL_GEOMETRY_SHADER,"./res/sphere/image-gs.glsl" },
			{ GL_FRAGMENT_SHADER,"./res/sphere/display-fs.glsl" },
		},
		{ "./res/sphere/globals.glsl" });

	createShaderProgram("shadow", {
			{ GL_VERTEX_SHADER,"./res/sphere/sphere-vs.glsl" },
			{ GL_GEOMETRY_SHADER,"./res/sphere/sphere-gs.glsl" },
			{ GL_FRAGMENT_SHADER,"./res/sphere/shadow-fs.glsl" },
		},
		{ "./res/model/globals.glsl" });

	createShaderProgram("grid", {
			{ GL_VERTEX_SHADER,"./res/scaling/grid-vs.glsl" },
			{ GL_FRAGMENT_SHADER,"./res/scaling/grid-fs.glsl" },
		});

	createShaderProgram("test", {
		{ GL_VERTEX_SHADER, "./res/sphere/image-vs.glsl"},
		// { GL_TESS_CONTROL_SHADER, "./res/scaling/test-tcs.glsl"},
		// { GL_TESS_EVALUATION_SHADER, "./res/scaling/test-es.glsl"},
		{ GL_GEOMETRY_SHADER, "./res/sphere/image-gs.glsl"},
		{ GL_FRAGMENT_SHADER,"./res/scaling/test-fs.glsl" },
	});

	createShaderProgram("grid-to-points", {
		{ GL_VERTEX_SHADER, "./res/scaling/grid-generate-vs.glsl"},
		{ GL_FRAGMENT_SHADER, "./res/scaling/grid-fs.glsl"},
	});

	createShaderProgram("fb-split", {
		{ GL_VERTEX_SHADER, "./res/sphere/image-vs.glsl"},
		// { GL_TESS_CONTROL_SHADER, "./res/scaling/test-tcs.glsl"},
		// { GL_TESS_EVALUATION_SHADER, "./res/scaling/test-es.glsl"},
		{ GL_GEOMETRY_SHADER, "./res/sphere/image-gs.glsl"},
		{ GL_FRAGMENT_SHADER,"./res/scaling/fb-split-fs.glsl" },
	});

	createShaderProgram("fb-split-blit", {
		{ GL_VERTEX_SHADER, "./res/sphere/image-vs.glsl"},
		{ GL_GEOMETRY_SHADER, "./res/sphere/image-gs.glsl"},
		{ GL_FRAGMENT_SHADER,"./res/scaling/fb-blit-fs.glsl" },
	});

	m_framebufferSize = viewer->viewportSize();

	m_depthTexture = Texture::create(GL_TEXTURE_2D);
	m_depthTexture->setParameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	m_depthTexture->setParameter(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	m_depthTexture->setParameter(GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	m_depthTexture->setParameter(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	m_depthTexture->image2D(0, GL_DEPTH_COMPONENT, m_framebufferSize, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, nullptr);

	m_LOD0depthTexture = Texture::create(GL_TEXTURE_2D);
	m_LOD0depthTexture->setParameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	m_LOD0depthTexture->setParameter(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	m_LOD0depthTexture->setParameter(GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	m_LOD0depthTexture->setParameter(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	m_LOD0depthTexture->image2D(0, GL_DEPTH_COMPONENT, m_framebufferSize, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, nullptr);

	m_LOD1depthTexture = Texture::create(GL_TEXTURE_2D);
	m_LOD1depthTexture->setParameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	m_LOD1depthTexture->setParameter(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	m_LOD1depthTexture->setParameter(GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	m_LOD1depthTexture->setParameter(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	m_LOD1depthTexture->image2D(0, GL_DEPTH_COMPONENT, m_framebufferSize, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, nullptr);

	m_depthStencilTexture = Texture::create(GL_TEXTURE_2D);
	m_depthStencilTexture->setParameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	m_depthStencilTexture->setParameter(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	m_depthStencilTexture->setParameter(GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	m_depthStencilTexture->setParameter(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	m_depthStencilTexture->image2D(0, GL_DEPTH24_STENCIL8, m_framebufferSize, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, nullptr);

	m_spherePositionTexture = Texture::create(GL_TEXTURE_2D);
	m_spherePositionTexture->setParameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	m_spherePositionTexture->setParameter(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	m_spherePositionTexture->setParameter(GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	m_spherePositionTexture->setParameter(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	m_spherePositionTexture->image2D(0, GL_RGBA32F, m_framebufferSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

	m_sphereLOD0PositionTexture = Texture::create(GL_TEXTURE_2D);
	m_sphereLOD0PositionTexture->setParameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	m_sphereLOD0PositionTexture->setParameter(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	m_sphereLOD0PositionTexture->setParameter(GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	m_sphereLOD0PositionTexture->setParameter(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	m_sphereLOD0PositionTexture->image2D(0, GL_RGBA32F, m_framebufferSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

	m_sphereLOD1PositionTexture = Texture::create(GL_TEXTURE_2D);
	m_sphereLOD1PositionTexture->setParameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	m_sphereLOD1PositionTexture->setParameter(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	m_sphereLOD1PositionTexture->setParameter(GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	m_sphereLOD1PositionTexture->setParameter(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	m_sphereLOD1PositionTexture->image2D(0, GL_RGBA32F, m_framebufferSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

	m_sphereNormalTexture = Texture::create(GL_TEXTURE_2D);
	m_sphereNormalTexture->setParameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	m_sphereNormalTexture->setParameter(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	m_sphereNormalTexture->setParameter(GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	m_sphereNormalTexture->setParameter(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	m_sphereNormalTexture->image2D(0, GL_RGBA32F, m_framebufferSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

	m_sphereLOD0NormalTexture = Texture::create(GL_TEXTURE_2D);
	m_sphereLOD0NormalTexture->setParameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	m_sphereLOD0NormalTexture->setParameter(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	m_sphereLOD0NormalTexture->setParameter(GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	m_sphereLOD0NormalTexture->setParameter(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	m_sphereLOD0NormalTexture->image2D(0, GL_RGBA32F, m_framebufferSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	
	m_sphereLOD1NormalTexture = Texture::create(GL_TEXTURE_2D);
	m_sphereLOD1NormalTexture->setParameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	m_sphereLOD1NormalTexture->setParameter(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	m_sphereLOD1NormalTexture->setParameter(GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	m_sphereLOD1NormalTexture->setParameter(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	m_sphereLOD1NormalTexture->image2D(0, GL_RGBA32F, m_framebufferSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

	m_surfacePositionTexture = Texture::create(GL_TEXTURE_2D);
	m_surfacePositionTexture->setParameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	m_surfacePositionTexture->setParameter(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	m_surfacePositionTexture->setParameter(GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	m_surfacePositionTexture->setParameter(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	m_surfacePositionTexture->image2D(0, GL_RGBA32F, m_framebufferSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

	m_surfaceNormalTexture = Texture::create(GL_TEXTURE_2D);
	m_surfaceNormalTexture->setParameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	m_surfaceNormalTexture->setParameter(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	m_surfaceNormalTexture->setParameter(GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	m_surfaceNormalTexture->setParameter(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	m_surfaceNormalTexture->image2D(0, GL_RGBA32F, m_framebufferSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

	m_sphereDiffuseTexture = Texture::create(GL_TEXTURE_2D);
	m_sphereDiffuseTexture->setParameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	m_sphereDiffuseTexture->setParameter(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	m_sphereDiffuseTexture->setParameter(GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	m_sphereDiffuseTexture->setParameter(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	m_sphereDiffuseTexture->image2D(0, GL_RGBA32F, m_framebufferSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

	m_surfaceDiffuseTexture = Texture::create(GL_TEXTURE_2D);
	m_surfaceDiffuseTexture->setParameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	m_surfaceDiffuseTexture->setParameter(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	m_surfaceDiffuseTexture->setParameter(GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	m_surfaceDiffuseTexture->setParameter(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	m_surfaceDiffuseTexture->image2D(0, GL_RGBA32F, m_framebufferSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

	m_ambientTexture = Texture::create(GL_TEXTURE_2D);
	m_ambientTexture->setParameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	m_ambientTexture->setParameter(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	m_ambientTexture->setParameter(GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	m_ambientTexture->setParameter(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	m_ambientTexture->image2D(0, GL_RGBA32F, m_framebufferSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

	m_blurTexture = Texture::create(GL_TEXTURE_2D);
	m_blurTexture->setParameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	m_blurTexture->setParameter(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	m_blurTexture->setParameter(GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	m_blurTexture->setParameter(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	m_blurTexture->image2D(0, GL_RGBA32F, m_framebufferSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

	m_colorTexture = Texture::create(GL_TEXTURE_2D);
	m_colorTexture->setParameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	m_colorTexture->setParameter(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	m_colorTexture->setParameter(GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	m_colorTexture->setParameter(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	m_colorTexture->image2D(0, GL_RGBA32F, m_framebufferSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

	for (auto& tex : m_offsetTexture) {
		tex = Texture::create(GL_TEXTURE_2D);
		tex->setParameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		tex->setParameter(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		tex->setParameter(GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		tex->setParameter(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		tex->image2D(0, GL_R32UI, m_framebufferSize, 0, GL_RED_INTEGER, GL_UNSIGNED_BYTE, nullptr);
	}

	m_shadowColorTexture = Texture::create(GL_TEXTURE_2D);
	m_shadowColorTexture->setParameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	m_shadowColorTexture->setParameter(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	m_shadowColorTexture->setParameter(GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	m_shadowColorTexture->setParameter(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	m_shadowColorTexture->setParameter(GL_TEXTURE_BORDER_COLOR, vec4(0.0, 0.0, 0.0, 65535.0f));
	m_shadowColorTexture->image2D(0, GL_RGBA32F, m_shadowMapSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

	m_shadowDepthTexture = Texture::create(GL_TEXTURE_2D);
	m_shadowDepthTexture->setParameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	m_shadowDepthTexture->setParameter(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	m_shadowDepthTexture->setParameter(GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	m_shadowDepthTexture->setParameter(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	m_shadowDepthTexture->setParameter(GL_TEXTURE_BORDER_COLOR, vec4(0.0, 0.0, 0.0, 0.0));
	m_shadowDepthTexture->image2D(0, GL_DEPTH_COMPONENT, m_shadowMapSize, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, nullptr);


	stbi_set_flip_vertically_on_load(true);

	for (auto& d : std::filesystem::directory_iterator("./dat/environments"))
	{
		std::filesystem::path environmentPath(d);

		std::unique_ptr<Texture> texture = loadTexture(environmentPath.string());

		if (texture)
			m_environmentTextures.push_back(std::move(texture));

	}

	for (auto& d : std::filesystem::directory_iterator("./dat/materials"))
	{
		std::filesystem::path materialPath(d);

		std::unique_ptr<Texture> texture = loadTexture(materialPath.string());

		if (texture)
		{
			texture->setParameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			texture->setParameter(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			texture->setParameter(GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			texture->setParameter(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

			m_materialTextures.push_back(std::move(texture));
		}
	}

	for (auto& d : std::filesystem::directory_iterator("./dat/bumps"))
	{
		std::filesystem::path bumpPath(d);

		std::unique_ptr<Texture> texture = loadTexture(bumpPath.string());

		if (texture)
			m_bumpTextures.push_back(std::move(texture));
	}

	m_sphereFramebuffer = Framebuffer::create();
	m_sphereFramebuffer->attachTexture(GL_COLOR_ATTACHMENT0, m_spherePositionTexture.get());
	m_sphereFramebuffer->attachTexture(GL_COLOR_ATTACHMENT1, m_sphereNormalTexture.get());
	m_sphereFramebuffer->attachTexture(GL_DEPTH_ATTACHMENT, m_depthTexture.get());
	m_sphereFramebuffer->setDrawBuffers({ GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 });

	m_sphereLOD0Framebuffer = Framebuffer::create();
	m_sphereLOD0Framebuffer->attachTexture(GL_COLOR_ATTACHMENT0, m_sphereLOD0PositionTexture.get());
	m_sphereLOD0Framebuffer->attachTexture(GL_COLOR_ATTACHMENT1, m_sphereLOD0NormalTexture.get());
	m_sphereLOD0Framebuffer->attachTexture(GL_DEPTH_ATTACHMENT, m_LOD0depthTexture.get());
	m_sphereLOD0Framebuffer->setDrawBuffers({ GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 });
	
	m_sphereLOD1Framebuffer = Framebuffer::create();
	m_sphereLOD1Framebuffer->attachTexture(GL_COLOR_ATTACHMENT0, m_sphereLOD1PositionTexture.get());
	m_sphereLOD1Framebuffer->attachTexture(GL_COLOR_ATTACHMENT1, m_sphereLOD1NormalTexture.get());
	m_sphereLOD1Framebuffer->attachTexture(GL_DEPTH_ATTACHMENT, m_LOD1depthTexture.get());
	m_sphereLOD1Framebuffer->setDrawBuffers({ GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 });

	m_surfaceFramebuffer = Framebuffer::create();
	m_surfaceFramebuffer->attachTexture(GL_COLOR_ATTACHMENT0, m_surfacePositionTexture.get());
	m_surfaceFramebuffer->attachTexture(GL_COLOR_ATTACHMENT1, m_surfaceNormalTexture.get());
	m_surfaceFramebuffer->attachTexture(GL_COLOR_ATTACHMENT2, m_surfaceDiffuseTexture.get());
	m_surfaceFramebuffer->attachTexture(GL_COLOR_ATTACHMENT3, m_sphereDiffuseTexture.get());
	m_surfaceFramebuffer->attachTexture(GL_DEPTH_ATTACHMENT, m_depthTexture.get());
	m_surfaceFramebuffer->setDrawBuffers({ GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3 });

	m_shadeFramebuffer = Framebuffer::create();
	m_shadeFramebuffer->attachTexture(GL_COLOR_ATTACHMENT0, m_colorTexture.get());
	m_shadeFramebuffer->setDrawBuffers({ GL_COLOR_ATTACHMENT0 });
	m_shadeFramebuffer->attachTexture(GL_DEPTH_ATTACHMENT, m_depthTexture.get());

	m_aoBlurFramebuffer = Framebuffer::create();
	m_aoBlurFramebuffer->attachTexture(GL_COLOR_ATTACHMENT0, m_blurTexture.get());
	m_aoBlurFramebuffer->setDrawBuffers({ GL_COLOR_ATTACHMENT0 });

	m_aoFramebuffer = Framebuffer::create();
	m_aoFramebuffer->attachTexture(GL_COLOR_ATTACHMENT0, m_ambientTexture.get());
	m_aoFramebuffer->setDrawBuffers({ GL_COLOR_ATTACHMENT0 });

	m_dofBlurFramebuffer = Framebuffer::create();
	m_dofBlurFramebuffer->attachTexture(GL_COLOR_ATTACHMENT0, m_sphereNormalTexture.get());
	m_dofBlurFramebuffer->attachTexture(GL_COLOR_ATTACHMENT1, m_surfaceNormalTexture.get());
	m_dofBlurFramebuffer->setDrawBuffers({ GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 });

	m_dofFramebuffer = Framebuffer::create();
	m_dofFramebuffer->attachTexture(GL_COLOR_ATTACHMENT0, m_sphereDiffuseTexture.get());
	m_dofFramebuffer->attachTexture(GL_COLOR_ATTACHMENT1, m_surfaceDiffuseTexture.get());
	m_dofFramebuffer->setDrawBuffers({ GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 });
	
	m_shadowFramebuffer = Framebuffer::create();
	m_shadowFramebuffer->attachTexture(GL_COLOR_ATTACHMENT0, m_shadowColorTexture.get());
	m_shadowFramebuffer->attachTexture(GL_DEPTH_ATTACHMENT, m_shadowDepthTexture.get());
	m_shadowFramebuffer->setDrawBuffers({ GL_COLOR_ATTACHMENT0 });
	
	// Set scene graph buffer:
	m_sceneGraphBuffer = Buffer::create();
	// Use sum of geometric series (a = 8, k = 8) to calculate grid size.
	const unsigned long long gridCount = (pow(8, gridDepth + 1) - 8) / 7;
	std::cout << "Buffer size: " << gridCount * (sizeof(glm::uvec4) + sizeof(glm::uvec4)) << std::endl;
	m_sceneGraphBuffer->setData(
		2 * sizeof(glm::uvec4) * gridCount,
		nullptr, gl::GL_DYNAMIC_COPY);
	m_sceneGraphBuffer->bindBase(GL_SHADER_STORAGE_BUFFER, 7);

	// Vertex binding setup
	m_hiarchyVertices = Buffer::create();
	m_hiarchyVertices->setStorage(viewer->scene()->protein()->m_hierarchyPoints, gl::GL_NONE_BIT);
	
	auto vertexBinding = m_vao->binding(0);
	vertexBinding->setAttribute(0);
	vertexBinding->setBuffer(m_hiarchyVertices.get(), 0, sizeof(Protein::HierchicalPoints));
	vertexBinding->setFormat(4, GL_FLOAT);
	m_vao->enable(0);
	vertexBinding = m_vao->binding(1);
	vertexBinding->setAttribute(1);
	vertexBinding->setBuffer(m_hiarchyVertices.get(), sizeof(glm::vec4), sizeof(Protein::HierchicalPoints));
	vertexBinding->setFormat(4, GL_FLOAT);
	m_vao->enable(1);
	vertexBinding = m_vao->binding(2);
	vertexBinding->setAttribute(2);
	vertexBinding->setBuffer(m_hiarchyVertices.get(), 2 * sizeof(glm::vec4), sizeof(Protein::HierchicalPoints));
	vertexBinding->setFormat(1, GL_FLOAT);
	m_vao->enable(2);

	m_denseAtomVertices = Buffer::create();
	m_denseAtomVertices->setStorage(viewer->scene()->protein()->m_genAtomsDense, gl::GL_NONE_BIT);
	m_denseVertexCount = static_cast<gl::GLsizei>(viewer->scene()->protein()->m_genAtomsDense.size());
	
	vertexBinding = m_denseVAO->binding(0);
	vertexBinding->setAttribute(0);
	vertexBinding->setBuffer(m_denseAtomVertices.get(), 0, sizeof(Protein::HierchicalPoints));
	vertexBinding->setFormat(4, GL_FLOAT);
	m_denseVAO->enable(0);
	vertexBinding = m_denseVAO->binding(1);
	vertexBinding->setAttribute(1);
	vertexBinding->setBuffer(m_denseAtomVertices.get(), sizeof(glm::vec4), sizeof(Protein::HierchicalPoints));
	vertexBinding->setFormat(4, GL_FLOAT);
	m_denseVAO->enable(1);
	vertexBinding = m_denseVAO->binding(2);
	vertexBinding->setAttribute(2);
	vertexBinding->setBuffer(m_denseAtomVertices.get(), 2 * sizeof(glm::vec4), sizeof(Protein::HierchicalPoints));
	vertexBinding->setFormat(1, GL_FLOAT);
	m_denseVAO->enable(2);

	// Sparse points:
	m_sparseAtomVertices = Buffer::create();
	m_sparseAtomVertices->setStorage(viewer->scene()->protein()->m_genAtomsSparse, gl::GL_NONE_BIT);
	m_sparseVertexCount = static_cast<gl::GLsizei>(viewer->scene()->protein()->m_genAtomsSparse.size());

	m_sparseVAO = std::make_unique<globjects::VertexArray>();
	vertexBinding = m_sparseVAO->binding(0);
	vertexBinding->setAttribute(0);
	vertexBinding->setBuffer(m_sparseAtomVertices.get(), 0, sizeof(Protein::HierchicalPoints));
	vertexBinding->setFormat(4, GL_FLOAT);
	m_sparseVAO->enable(0);
	vertexBinding = m_sparseVAO->binding(1);
	vertexBinding->setAttribute(1);
	vertexBinding->setBuffer(m_sparseAtomVertices.get(), sizeof(glm::vec4), sizeof(Protein::HierchicalPoints));
	vertexBinding->setFormat(4, GL_FLOAT);
	m_sparseVAO->enable(1);
	vertexBinding = m_sparseVAO->binding(2);
	vertexBinding->setAttribute(2);
	vertexBinding->setBuffer(m_sparseAtomVertices.get(), 2 * sizeof(glm::vec4), sizeof(Protein::HierchicalPoints));
	vertexBinding->setFormat(1, GL_FLOAT);
	m_sparseVAO->enable(2);

	// Triangle (xyz, rgb, uv):
	const auto verts = std::to_array<GLfloat>({
		-0.5f, -0.5f, 0.f,		1.f, 0.f, 0.f,		0.f, 0.f,
		0.f, 0.5f, 0.f,			0.f, 1.f, 0.f,		0.5f, 1.f,
		0.5f, -0.5f, 0.f,		0.f, 0.f, 1.f,		1.f, 0.f,
	});
	m_triangleVertices = Buffer::create();
	m_triangleVertices->setStorage(verts, gl::GL_NONE_BIT);
	
	m_triangleVAO = std::make_unique<globjects::VertexArray>();
	vertexBinding = m_triangleVAO->binding(0);
	vertexBinding->setAttribute(0);
	vertexBinding->setBuffer(m_triangleVertices.get(), 0, sizeof(GLfloat) * verts.size() / 3);
	vertexBinding->setFormat(3, GL_FLOAT);
	m_triangleVAO->enable(0);

	vertexBinding = m_triangleVAO->binding(1);
	vertexBinding->setAttribute(1);
	vertexBinding->setBuffer(m_triangleVertices.get(), 3 * sizeof(GLfloat), sizeof(GLfloat) * verts.size() / 3);
	vertexBinding->setFormat(3, GL_FLOAT);
	m_triangleVAO->enable(1);
	
	vertexBinding = m_triangleVAO->binding(2);
	vertexBinding->setAttribute(2);
	vertexBinding->setBuffer(m_triangleVertices.get(), 6 * sizeof(GLfloat), sizeof(GLfloat) * verts.size() / 3);
	vertexBinding->setFormat(2, GL_FLOAT);
	m_triangleVAO->enable(2);

	// LOD redrawing buffer and counter:
	m_redrawCounter = std::make_unique<globjects::Buffer>();
	m_redrawCounter->setStorage(GLuint{0}, gl::GL_MAP_READ_BIT);

	m_redrawIndices = {std::make_unique<globjects::Buffer>(), std::make_unique<globjects::Buffer>()};
	for (auto& indices : m_redrawIndices)
		indices->setStorage(sizeof(GLuint) * m_sparseVertexCount * 9, nullptr, gl::GL_DYNAMIC_STORAGE_BIT | gl::GL_MAP_READ_BIT);

	
	m_redrawingVAO = std::make_unique<globjects::VertexArray>();
	vertexBinding = m_redrawingVAO->binding(0);
	vertexBinding->setAttribute(0);
	vertexBinding->setBuffer(m_redrawIndices[1].get(), 0, sizeof(glm::vec4));
	vertexBinding->setFormat(4, GL_FLOAT);
	m_redrawingVAO->enable(0);
	
	// gridToPoint VAO uses the same buffer as the grid buffer:
	m_gridToPointVAO = std::make_unique<globjects::VertexArray>();
	vertexBinding = m_gridToPointVAO->binding(0);
	vertexBinding->setAttribute(0);
	vertexBinding->setBuffer(m_sceneGraphBuffer.get(), 0, 2 * sizeof(glm::uvec4));
	vertexBinding->setFormat(4, GL_UNSIGNED_INT);
	m_gridToPointVAO->enable(0);
	vertexBinding = m_gridToPointVAO->binding(1);
	vertexBinding->setAttribute(1);
	vertexBinding->setBuffer(m_sceneGraphBuffer.get(), sizeof(glm::uvec4), 2 * sizeof(glm::uvec4));
	vertexBinding->setFormat(1, GL_UNSIGNED_INT);
	m_gridToPointVAO->enable(1);

	m_initialGridPoints = Buffer::create();
}

void SphereRenderer::display()
{
	if (viewer()->scene()->protein()->atoms().size() == 0)
		return;

	// SaveOpenGL state
	auto currentState = State::currentState();

	static float resolutionScale = 1.0f;

	const ivec2 viewportSize = ivec2(vec2(viewer()->viewportSize()) * resolutionScale);

	// Resize all FBOs if the viewport size has changed
	if (viewportSize != m_framebufferSize)
	{
		m_framebufferSize = viewportSize;
		for (auto& tex : m_offsetTexture)
			tex->image2D(0, GL_R32UI, m_framebufferSize, 0, GL_RED_INTEGER, GL_UNSIGNED_BYTE, nullptr);
		m_depthTexture->image2D(0, GL_DEPTH_COMPONENT, m_framebufferSize, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, nullptr);
		m_spherePositionTexture->image2D(0, GL_RGBA32F, m_framebufferSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
		m_sphereNormalTexture->image2D(0, GL_RGBA32F, m_framebufferSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
		m_surfacePositionTexture->image2D(0, GL_RGBA32F, m_framebufferSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
		m_surfaceNormalTexture->image2D(0, GL_RGBA32F, m_framebufferSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
		m_surfaceDiffuseTexture->image2D(0, GL_RGBA32F, m_framebufferSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
		m_sphereDiffuseTexture->image2D(0, GL_RGBA32F, m_framebufferSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
		m_ambientTexture->image2D(0, GL_RGBA32F, m_framebufferSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
		m_blurTexture->image2D(0, GL_RGBA32F, m_framebufferSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
		m_colorTexture->image2D(0, GL_RGBA32F, m_framebufferSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	}

	// our shader programs

	auto programSphere = shaderProgram("sphere");
	auto programSpawn = shaderProgram("spawn");
	auto programSpawnIteration = shaderProgram("spawn-iteration");
	auto programSurface = shaderProgram("surface");
	auto programAOSample = shaderProgram("aosample");
	auto programAOBlur = shaderProgram("aoblur");
	auto programShade = shaderProgram("shade");
	auto programDOFBlur = shaderProgram("dofblur");
	auto programDOFBlend = shaderProgram("dofblend");
	auto programDisplay = shaderProgram("display");
	auto programShadow = shaderProgram("shadow");
	auto programGrid = shaderProgram("grid");
	auto programTest = shaderProgram("test");
	auto programGridGenerate = shaderProgram("grid-to-points");
	auto programLayerSphere = shaderProgram("sphere");
	auto programFramebufferSplitting = shaderProgram("fb-split");
	auto programFramebufferSplittingBlitting = shaderProgram("fb-split-blit");

	
	// get cursor position for magic lens
	double mouseX, mouseY;
	glfwGetCursorPos(viewer()->window(), &mouseX, &mouseY);
	const vec2 focusPosition = vec2(2.0f*float(mouseX) / float(viewportSize.x) - 1.0f, -2.0f*float(mouseY) / float(viewportSize.y) + 1.0f);

	// retrieve/compute all necessary matrices and related properties
	const mat4 viewMatrix = viewer()->viewTransform();
	const mat4 inverseViewMatrix = inverse(viewMatrix);
	const mat4 modelViewMatrix = viewer()->modelViewTransform();
	const mat4 inverseModelViewMatrix = inverse(modelViewMatrix);
	const mat4 modelLightMatrix = viewer()->modelLightTransform();
	const mat4 inverseModelLightMatrix = inverse(modelLightMatrix);
	const mat4 modelViewProjectionMatrix = viewer()->modelViewProjectionTransform();
	const mat4 inverseModelViewProjectionMatrix = inverse(modelViewProjectionMatrix);
	const mat4 modelLightProjectionMatrix = viewer()->modelLightProjectionTransform();
	const mat4 inverseModelLightProjectionMatrix = inverse(modelLightProjectionMatrix);
	const mat4 projectionMatrix = viewer()->projectionTransform();
	const mat4 inverseProjectionMatrix = inverse(projectionMatrix);
	const mat3 normalMatrix = mat3(transpose(inverseModelViewMatrix));
	const mat3 inverseNormalMatrix = inverse(normalMatrix);

	const vec3 objectCenter = 0.5f * (viewer()->scene()->protein()->maximumBounds() + viewer()->scene()->protein()->minimumBounds());
	const float objectRadius = 0.5f * length(viewer()->scene()->protein()->maximumBounds() - viewer()->scene()->protein()->minimumBounds());

	const vec4 projectionInfo(float(-2.0 / (viewportSize.x * projectionMatrix[0][0])),
		float(-2.0 / (viewportSize.y * projectionMatrix[1][1])),
		float((1.0 - (double)projectionMatrix[0][2]) / projectionMatrix[0][0]),
		float((1.0 + (double)projectionMatrix[1][2]) / projectionMatrix[1][1]));

	const float projectionScale = float(viewportSize.y) / fabs(2.0f / projectionMatrix[1][1]);
	const float fieldOfView = 2.0f * atan(1.0f / projectionMatrix[1][1]);

	vec4 nearPlane = inverseProjectionMatrix * vec4(0.0, 0.0, -1.0, 1.0);
	nearPlane /= nearPlane.w;

	vec4 worldLightPosition = inverseModelLightMatrix * vec4(0.0f, 0.0f, 0.0f, 1.0f);
	vec4 viewLightPosition = modelViewMatrix * worldLightPosition;

	// all input parameters and their default values
	static vec3 ambientMaterial = viewer()->backgroundColor();// (0.3f, 0.3f, 0.3f);
	static vec3 diffuseMaterial(0.6f, 0.6f, 0.6f);
	static vec3 specularMaterial(0.3f, 0.3f, 0.3f);
	static float shininess = 20.0f;
	static float sharpness = 1.0f;

	static float distanceBlending = 0.0f;
	static float distanceScale = 1.0;

	static bool ambientOcclusion = false;
	static bool environmentMapping = false;
	static bool environmentLighting = false;
	static bool normalMapping = false;
	static bool materialMapping = false;
	static bool depthOfField = false;

	static int coloring = 0;
	static bool animate = false;
	static float animationAmplitude = 1.0f;
	static float animationFrequency = 1.0f;
	static bool lens = false;

	static float focalDistance = 2.0f * sqrt(3.0f);
	static float maximumCoCRadius = 9.0f;
	static float farRadiusRescale = 1.0f;
	static float focalLength = 1.0f;
	static float aparture = 1.0f;
	static float fStop = 1.0;
	static int fStop_current = 12;
	const char* fStops[] = { "0.7", "0.8", "1.0", "1.2", "1.4", "1.7", "2.0", "2.4", "2.8", "3.3", "4.0", "4.8", "5.6", "6.7", "8.0", "9.5", "11.0", "16.0", "22.0", "32.0" };

	static uint environmentTextureIndex = 0;
	static uint materialTextureIndex = 0;
	static uint bumpTextureIndex = 0;

	static float replaceScaleParam{0.01f}, interpolation{0.f}, clustering{0.f};
	static int startLODParam{1};
	static bool bVisualizeOverlaps{false};

	static float rLOD0{1.7f}, rLOD1{1.f};
	static float sharpnessOffset{1.f};

	// user interface for manipulating rendering parameters
	if (ImGui::BeginMenu("Renderer"))
	{
		ImGui::SliderFloat("Resolution Scale", &resolutionScale, 0.25f, 8.0f);

		if (ImGui::CollapsingHeader("Lighting"))
		{
			ImGui::ColorEdit3("Ambient", (float*)&ambientMaterial);
			ImGui::ColorEdit3("Diffuse", (float*)&diffuseMaterial);
			ImGui::ColorEdit3("Specular", (float*)&specularMaterial);
			ImGui::SliderFloat("Shininess", &shininess, 1.0f, 256.0f);
			ImGui::Checkbox("Ambient Occlusion Enabled", &ambientOcclusion);
			ImGui::Checkbox("Material Mapping Enabled", &materialMapping);
			ImGui::Checkbox("Normal Mapping Enabled", &normalMapping);
			ImGui::Checkbox("Environment Mapping Enabled", &environmentMapping);
			ImGui::Checkbox("Depth of Field Enabled", &depthOfField);
		}

		if (ImGui::CollapsingHeader("Surface"))
		{
			ImGui::SliderFloat("Sharpness", &sharpness, 0.5f, 16.0f);
			ImGui::SliderFloat("Dist. Blending", &distanceBlending, 0.0f, 1.0f);
			ImGui::SliderFloat("Dist. Scale", &distanceScale, 0.0f, 16.0f);
			ImGui::Combo("Coloring", &coloring, "None\0Element\0Residue\0Chain\0");
			ImGui::Checkbox("Magic Lens", &lens);
		}


		if (environmentMapping)
		{
			if (ImGui::CollapsingHeader("Environment Mapping"))
			{
				if (ImGui::ListBoxHeader("Environment Map"))
				{
					for (uint i = 0; i < m_environmentTextures.size(); i++)
					{
						auto& texture = m_environmentTextures[i];
						bool selected = (i == environmentTextureIndex);
						ImGui::BeginGroup();
						ImGui::PushID(i);

						if (ImGui::Selectable("", &selected, 0, ImVec2(0.0f, 32.0f)))
							environmentTextureIndex = i;

						ImGui::SameLine();
						ImGui::Image(reinterpret_cast<ImTextureID>(static_cast<std::size_t>(texture->id())), ImVec2(32.0f, 32.0f));
						ImGui::PopID();
						ImGui::EndGroup();
					}

					ImGui::ListBoxFooter();
				}

				ImGui::Checkbox("Use for Illumination", &environmentLighting);
			}
		}


		if (materialMapping)
		{
			if (ImGui::CollapsingHeader("Material Mapping"))
			{
				if (ImGui::ListBoxHeader("Material Map"))
				{
					for (uint i = 0; i < m_materialTextures.size(); i++)
					{
						auto& texture = m_materialTextures[i];
						bool selected = (i == materialTextureIndex);
						ImGui::BeginGroup();
						ImGui::PushID(i);

						if (ImGui::Selectable("", &selected, 0, ImVec2(0.0f, 32.0f)))
							materialTextureIndex = i;

						ImGui::SameLine();
						ImGui::Image((ImTextureID)texture->id(), ImVec2(32.0f, 32.0f));
						ImGui::PopID();
						ImGui::EndGroup();
					}

					ImGui::ListBoxFooter();
				}
			}
		}


		if (normalMapping)
		{
			if (ImGui::CollapsingHeader("Normal Mapping"))
			{
				if (ImGui::ListBoxHeader("Normal Map"))
				{
					for (uint i = 0; i < m_bumpTextures.size(); i++)
					{
						auto& texture = m_bumpTextures[i];
						bool selected = (i == bumpTextureIndex);
						ImGui::BeginGroup();
						ImGui::PushID(i);

						if (ImGui::Selectable("", &selected, 0, ImVec2(0.0f, 32.0f)))
							bumpTextureIndex = i;

						ImGui::SameLine();
						ImGui::Image((ImTextureID)texture->id(), ImVec2(32.0f, 32.0f));
						ImGui::PopID();
						ImGui::EndGroup();
					}

					ImGui::ListBoxFooter();
				}
			}
		}

		if (depthOfField)
		{
			if (ImGui::CollapsingHeader("Depth of Field"))
			{
				ImGui::SliderFloat("Focal Distance", &focalDistance, 0.1f, 35.0f);
				ImGui::Combo("F-stop", &fStop_current, fStops, IM_ARRAYSIZE(fStops));

				ImGui::SliderFloat("Max. CoC Radius", &maximumCoCRadius, 1.0f, 20.0f);
				ImGui::SliderFloat("Far Radius Scale", &farRadiusRescale, 0.1f, 5.0f);

			}
		}

		fStop = std::stof(fStops[fStop_current]);
		focalLength = 1.0f / (tan(fieldOfView * 0.5f) * 2.0f);
		aparture = focalLength / fStop;

		if (ImGui::CollapsingHeader("Animation"))
		{
			ImGui::Checkbox("Prodecural Animation", &animate);
			ImGui::SliderFloat("Frequency", &animationFrequency, 1.0f, 256.0f);
			ImGui::SliderFloat("Amplitude", &animationAmplitude, 1.0f, 32.0f);
		}

		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu("Other Shit")) {
		ImGui::SliderFloat("Interpolation", &interpolation, 0.f, 1.f);
		ImGui::SliderFloat("Clustering", &clustering, 0.f, 1.f);
		ImGui::SliderFloat("LOD0 radius", &rLOD0, 0.f, 10.f);
		ImGui::SliderFloat("LOD1 radius", &rLOD1, 0.f, 10.f);
		ImGui::SliderFloat("Interpolation sharpness", &sharpnessOffset, 0.1f, 32.0f);
		ImGui::SliderInt("Start LOD", &startLODParam, 1, 4);
		ImGui::Checkbox("Visualize overlaps", &bVisualizeOverlaps);
		ImGui::EndMenu();
	}

	// Scaling for sphere of influence radius based on estimated density
	const float contributingAtoms = 32.0f;
	const float radiusScale = sqrtf(log(contributingAtoms * exp(sharpness)) / sharpness);

	// Properties for animation
	const uint timestepCount = (uint)viewer()->scene()->protein()->atoms().size();
	const float animationTime = animate ? float(glfwGetTime()) : -1.0f;
	const float currentTime = static_cast<float>(glfwGetTime()) * animationFrequency;
	const uint currentTimestep = uint(currentTime) % timestepCount;
	const uint nextTimestep = (currentTimestep + 1) % timestepCount;
	const float animationDelta = currentTime - floor(currentTime);
	const int vertexCount = int(viewer()->scene()->protein()->atoms()[currentTimestep].size());

	// Defines for enabling/disabling shader feature based on parameter setting
	std::string defines = "";

	if (animate)
		defines += "#define ANIMATION\n";

	if (lens)
		defines += "#define LENSING\n";

	if (coloring > 0)
		defines += "#define COLORING\n";

	if (ambientOcclusion)
		defines += "#define AMBIENT\n";

	if (environmentMapping)
		defines += "#define ENVIRONMENT\n";

	if (environmentMapping && environmentLighting)
		defines += "#define ENVIRONMENTLIGHTING\n";

	if (normalMapping)
		defines += "#define NORMAL\n";

	if (materialMapping)
		defines += "#define MATERIAL\n";

	if (depthOfField)
		defines += "#define DEPTHOFFIELD\n";

	if (bVisualizeOverlaps)
		defines += "#define VISUALIZE_OVERLAPS\n";

	// Reload shaders if settings have changed
	if (defines != m_shaderSourceDefines->string())
	{
		m_shaderSourceDefines->setString(defines);
		reloadShaders();
	}

	// // Vertex binding setup
	// auto vertexBinding = m_vao->binding(0);
	// vertexBinding->setAttribute(0);
	// vertexBinding->setBuffer(m_vertices[currentTimestep].get(), 0, sizeof(vec4));
	// vertexBinding->setFormat(4, GL_FLOAT);
	// m_vao->enable(0);
	
	const auto l = [=](float t){
		return std::lerp(rLOD0, rLOD1, t);
	};

	struct LOD {
		std::unique_ptr<globjects::VertexArray>& vao;
		const int vCount;
		float radius;
		std::function<float(float)> clustering;
		std::function<float(float)> sharpness;
	};

	// using LODT = std::tuple<std::unique_ptr<globjects::VertexArray>&, int&, decltype(f0)&>;

	const auto LODs = std::to_array<LOD>({
		// LOD-1
		{
			m_sparseVAO, m_sparseVertexCount, 5.f,
			[](float t){ return 0.f; },
			[](float t){ return sharpness; }
		},
		// LOD0
		{
			m_vao, vertexCount, 1.7f,
			[](float t){ return 0.f; },
			[](float t){ return sharpness; }
		},
		// LOD1
		{
			m_denseVAO, m_denseVertexCount, 1.f,
			[](float t){ return 0.f/*t < 0.5 ? 0.f : 2.f - t * 2.f*/; },
			[](float t){ return sharpness; }
		},
	});

	constexpr float PAIR_EPSILON = 0.002f;

	// const auto cmp = [](float a, float b, float eps){
	// 	return 
	// };

	const auto isSinglePair = [&LODs = std::as_const(LODs), PAIR_EPSILON](float t){
		const auto n = LODs.size() - 1;
		const std::size_t i = static_cast<std::size_t>(std::round(n * t));
		return std::abs(n * t - i) < PAIR_EPSILON;
	};

	const auto clampedInterpolation = [&LODs = std::as_const(LODs), isSinglePair](float t){
		const auto n = LODs.size() - 1;
		if (isSinglePair(t))
			return 0.f;
		
		return static_cast<float>(n * t - std::floor(n * t));
	};

	const auto getPairwiseLODs = [&LODs, clampedInterpolation, PAIR_EPSILON, isSinglePair](float t){
		const auto n = LODs.size() - 1;
		std::size_t i = n * t;
		const bool bSinglePair = isSinglePair(t);
		if (bSinglePair)
			i = static_cast<std::size_t>(std::round(n * t));

		return std::to_array({LODs.data() + i, bSinglePair ? nullptr : LODs.data() + i+1});		
	};

	// if (timestepCount > 0)
	// {
	// 	auto nextVertexBinding = m_vao->binding(1);
	// 	nextVertexBinding->setAttribute(1);
	// 	nextVertexBinding->setBuffer(m_vertices[nextTimestep].get(), 0, sizeof(vec4));
	// 	nextVertexBinding->setFormat(4, GL_FLOAT);
	// 	m_vao->enable(1);
	// }

	constexpr float ATOM_SIZE = 1.7f;
	const std::pair bounds{viewer()->scene()->protein()->minimumBounds(), viewer()->scene()->protein()->maximumBounds()};
	/*

	//////////////////////////////////////////////////////////////////////////
	// Grid calculation passs
	//////////////////////////////////////////////////////////////////////////
	// Clear SSBO
	// Use sum of geometric series (a = 8, k = 8) to calculate grid size.
	const unsigned long long gridCount = (pow(8, gridDepth + 1) - 8) / 7;
	// std::vector<std::pair<glm::uvec4, glm::uvec4>> emptyBuffer{};
	// emptyBuffer.resize(gridCount, {});
	m_sceneGraphBuffer->clearSubData(GL_RGBA32UI, 0, 2 * sizeof(glm::uvec4) * gridCount, GL_RGBA, GL_UNSIGNED_INT, nullptr);
	m_sceneGraphBuffer->bindBase(GL_SHADER_STORAGE_BUFFER, 7);


	programGrid->setUniform("modelViewProjectionMatrix", modelViewProjectionMatrix);
	programGrid->setUniform("inverseModelViewProjectionMatrix", inverseModelViewProjectionMatrix);
	programGrid->setUniform("gridScale", gridSize);
	programGrid->setUniform("gridDepth", gridDepth);
	programGrid->setUniform("minb", bounds.first);
	programGrid->setUniform("maxb", bounds.second);

	auto [gridVAO, gridVCount] = std::tie(LODs[0].vao, LODs[0].vCount);

	programGrid->use();
	glPointSize(10.f);
	gridVAO->drawArrays(GL_POINTS, 0, gridVCount);
	programGrid->release();

	m_sceneGraphBuffer->unbind(GL_SHADER_STORAGE_BUFFER);
	*/

	/*
	//////////////////////////////////////////////////////////////////////////
	// Grid start points generation
	//////////////////////////////////////////////////////////////////////////
	glMemoryBarrier(GL_ALL_BARRIER_BITS);
	glClearDepth(1.0f);
	glClearColor(0.2, 0.2, 0.2, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glDisable(GL_DEPTH_TEST);
	{
		const unsigned int LOD = startLODParam;

		glDisable(GL_DEPTH_TEST);
		glPointSize(10.f);

		m_initialGridPoints->setData(sizeof(glm::vec4) * pow(8, LOD), nullptr, GL_DYNAMIC_COPY);
		m_initialGridPoints->bindBase(GL_SHADER_STORAGE_BUFFER, 5);
		
		vertexBinding = m_gridToPointVAO->binding(0);
		vertexBinding->setAttribute(0);
		vertexBinding->setBuffer(m_sceneGraphBuffer.get(), 0, 2 * sizeof(glm::vec4));
		vertexBinding->setFormat(4, GL_UNSIGNED_INT);
		m_gridToPointVAO->enable(0);
		vertexBinding = m_gridToPointVAO->binding(1);
		vertexBinding->setAttribute(1);
		vertexBinding->setBuffer(m_sceneGraphBuffer.get(), sizeof(glm::vec4), 2 * sizeof(glm::vec4));
		vertexBinding->setFormat(4, GL_UNSIGNED_INT);
		m_gridToPointVAO->enable(1);
		// glMemoryBarrier(GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT);

		m_gridToPointVAO->bind();

		programGridGenerate->use();
		programGridGenerate->setUniform("modelViewProjectionMatrix", modelViewProjectionMatrix);
		programGridGenerate->setUniform("visualize", true);
		// Draw points in the LOD range from the grid buffer:
		const auto start = (pow(8, LOD) - 8) / 7;
		const auto end = (pow(8, LOD+1) - 8) / 7;
		const auto count = end - start;
		m_gridToPointVAO->drawArrays(GL_POINTS, start, count);
		programGridGenerate->release();
		m_gridToPointVAO->unbind();

		m_initialGridPoints->unbind(GL_SHADER_STORAGE_BUFFER);
	}

	*/

	//////////////////////////////////////////////////////////////////////////
	// Sphere rendering pass
	//////////////////////////////////////////////////////////////////////////
	/** Renders sphere's inner radius onto a position lookup texture
	 */ 
	// m_sphereFramebuffer->bind();
	glClearDepth(1.0f);
	glClearColor(0.0, 0.0, 0.0, 65535.0f);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
		
	programSphere->use();

	programSphere->setUniform("modelViewMatrix", modelViewMatrix);
	programSphere->setUniform("projectionMatrix", projectionMatrix);
	programSphere->setUniform("modelViewProjectionMatrix", modelViewProjectionMatrix);
	programSphere->setUniform("inverseModelViewProjectionMatrix", inverseModelViewProjectionMatrix);
	programSphere->setUniform("nearPlaneZ", nearPlane.z);
	programSphere->setUniform("animationDelta", animationDelta);
	programSphere->setUniform("animationTime", animationTime);
	programSphere->setUniform("animationAmplitude", animationAmplitude);
	programSphere->setUniform("animationFrequency", animationFrequency);
	programSphere->setUniform("gridScale", gridSize);
	programSphere->setUniform("minb", bounds.first);
	programSphere->setUniform("maxb", bounds.second);

	m_sceneGraphBuffer->bindBase(GL_SHADER_STORAGE_BUFFER, 7);

	for (auto [index, lod] : enumerate(getPairwiseLODs(interpolation))) {
		auto framebuffer = (index == 0 ? m_sphereLOD0Framebuffer : m_sphereLOD1Framebuffer).get();
		framebuffer->bind();

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		
		if (lod == nullptr)
			continue;
		auto& [vao, vCount, scale, cluster, sharp] = *lod;
		const auto interp = clampedInterpolation(interpolation);
		const auto weight = index == 0 ? 1.f - interp : interp;

		programSphere->setUniform("radiusScale", scale); // Inner radius
		// programSpawn->setUniform("outerRadius", ATOM_SIZE * scale);
		programSphere->setUniform("clipRadiusScale", radiusScale * scale); // Outer radius
		programSphere->setUniform("clustering", cluster(interp));
		programSphere->setUniform("individualSharpness", sharp(interp));
		programSphere->setUniform("weight", weight);

		vao->drawArrays(GL_POINTS, 0, vCount);
		framebuffer->unbind();
	}

	programSphere->release();

	m_sceneGraphBuffer->unbind(GL_SHADER_STORAGE_BUFFER);

	//////////////////////////////////////////////////////////////////////////
	// Framebuffer splitting:
	//////////////////////////////////////////////////////////////////////////
	// glMemoryBarrier(GL_ALL_BARRIER_BITS);

	// const bool bSingleLOD = isSinglePair(interpolation);
	// m_sphereSplitFramebuffer->bind();
	// glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	// glDepthFunc(GL_ALWAYS);

	// const auto programSplitting = bSingleLOD ? programFramebufferSplittingBlitting : programFramebufferSplitting;

	// programSplitting->use();

	// m_sphereLOD0PositionTexture->bindActive(0);
	// m_sphereLOD0NormalTexture->bindActive(1);
	
	// if (!bSingleLOD) {
	// 	programSplitting->setUniform("modelViewProjectionMatrix", modelViewProjectionMatrix);
	// 	programSplitting->setUniform("interpolation", clampedInterpolation(interpolation));

	// 	m_sphereLOD1PositionTexture->bindActive(2);
	// 	m_sphereLOD1NormalTexture->bindActive(3);
	// }

	// m_vaoQuad->drawArrays(GL_POINTS, 0, 1);

	// if (!bSingleLOD) {
	// 	m_sphereLOD1NormalTexture->unbindActive(3);
	// 	m_sphereLOD1PositionTexture->unbindActive(2);
	// }

	// m_sphereLOD0NormalTexture->unbindActive(1);
	// m_sphereLOD0PositionTexture->unbindActive(0);
		
	// programSplitting->release();

	// m_sphereSplitFramebuffer->unbind();

	// //////////////////////////////////////////////////////////////////////////
	// // Layer sphere rendering pass (for back position texture)
	// //////////////////////////////////////////////////////////////////////////
	// m_sphereLevelFramebuffer->bind();
	// glClearDepth(1.0f);
	// glClearColor(0.0, 0.0, 0.0, 65535.0f);
	// glClearStencil(0);
	// glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
	// glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

	// glEnable(GL_DEPTH_TEST);
	// glDepthFunc(GL_LESS);
		
	// programLayerSphere->use();

	// programLayerSphere->setUniform("modelViewMatrix", modelViewMatrix);
	// programLayerSphere->setUniform("projectionMatrix", projectionMatrix);
	// programLayerSphere->setUniform("modelViewProjectionMatrix", modelViewProjectionMatrix);
	// programLayerSphere->setUniform("inverseModelViewProjectionMatrix", inverseModelViewProjectionMatrix);
	// programLayerSphere->setUniform("nearPlaneZ", nearPlane.z);
	// programLayerSphere->setUniform("animationDelta", animationDelta);
	// programLayerSphere->setUniform("animationTime", animationTime);
	// programLayerSphere->setUniform("animationAmplitude", animationAmplitude);
	// programLayerSphere->setUniform("animationFrequency", animationFrequency);
	// programLayerSphere->setUniform("gridScale", gridSize);
	// programLayerSphere->setUniform("minb", bounds.first);
	// programLayerSphere->setUniform("maxb", bounds.second);

	// m_sceneGraphBuffer->bindBase(GL_SHADER_STORAGE_BUFFER, 7);

	// for (uint i{0}; i < 2; ++i) {
	// 	auto& [vao, vCount, scale, cluster, sharp] = LODs[i];

	// 	glClear(GL_DEPTH_BUFFER_BIT);
	// 	uint8_t mask = 2u << i;
	// 	glStencilFunc(GL_EQUAL, mask, 0b11111111 ^ mask);

	// 	programLayerSphere->setUniform("radiusScale", scale);
	// 	// programSpawn->setUniform("outerRadius", ATOM_SIZE * scale);
	// 	programLayerSphere->setUniform("clipRadiusScale", radiusScale * scale);
	// 	programLayerSphere->setUniform("clustering", cluster(interpolation));
	// 	programLayerSphere->setUniform("individualSharpness", sharp(interpolation));
	// 	programLayerSphere->setUniform("weight", i == 0 ? 1.f - interpolation : interpolation);


	// 	vao->drawArrays(GL_POINTS, 0, vCount);
	// }
		
	// programLayerSphere->release();

	// m_sceneGraphBuffer->unbind(GL_SHADER_STORAGE_BUFFER);
	
	// m_sphereLevelFramebuffer->unbind();
		
	//////////////////////////////////////////////////////////////////////////
	// List generation pass
	//////////////////////////////////////////////////////////////////////////
	/** Generates an intersection list of the sphere's outer radius per pixel
	 */
	m_sphereFramebuffer->bind();
	constexpr uint intersectionClearValue = 1;
	constexpr uint offsetClearValue = 0;
	// clear only the first intersection?
	// m_intersectionBuffer->clearSubData(GL_R32UI, 0, sizeof(uint), GL_RED_INTEGER, GL_UNSIGNED_INT, &intersectionClearValue);


	glMemoryBarrier(GL_ALL_BARRIER_BITS);

	glDepthFunc(GL_ALWAYS);
	glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
	glDepthMask(GL_FALSE);

	// Positions of fragments of spheres (only closest to camera)
	// m_spherePositionTexture->bindActive(0);
	
	programSpawn->use();
	
	programSpawn->setUniform("modelViewMatrix", modelViewMatrix);
	programSpawn->setUniform("projectionMatrix", projectionMatrix);
	programSpawn->setUniform("modelViewProjectionMatrix", modelViewProjectionMatrix);
	programSpawn->setUniform("inverseModelViewProjectionMatrix", inverseModelViewProjectionMatrix);
	programSpawn->setUniform("nearPlaneZ", nearPlane.z);
	programSpawn->setUniform("animationDelta", animationDelta);
	programSpawn->setUniform("animationTime", animationTime);
	programSpawn->setUniform("animationAmplitude", animationAmplitude);
	programSpawn->setUniform("animationFrequency", animationFrequency);
	programSpawn->setUniform("gridScale", gridSize);
	programSpawn->setUniform("minb", bounds.first);
	programSpawn->setUniform("maxb", bounds.second);

	for (auto [index, lod] : enumerate(getPairwiseLODs(interpolation))) {
		if (lod == nullptr)
			continue;
		auto& [vao, vCount, scale, cluster, sharp] = *lod;
		const auto interp = clampedInterpolation(interpolation);
		const auto weight = index == 0 ? 1.f - interp : interp;

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	
		m_intersectionCount[index]->clearSubData(GL_R32UI, 0, sizeof(uint), GL_RED_INTEGER, GL_UNSIGNED_INT, &intersectionClearValue);
		m_offsetTexture[index]->clearImage(0, GL_RED_INTEGER, GL_UNSIGNED_INT, &offsetClearValue);

		(index == 0 ? m_sphereLOD0PositionTexture : m_sphereLOD1PositionTexture)->bindActive(0);
		m_intersectionBuffer[index]->bindBase(GL_SHADER_STORAGE_BUFFER, 1);
		m_intersectionCount[index]->bindBase(GL_ATOMIC_COUNTER_BUFFER, 1);
		m_offsetTexture[index]->bindImageTexture(0, 0, false, 0, GL_READ_WRITE, GL_R32UI);


		programSpawn->setUniform("radiusScale", radiusScale * scale); // Outer radius
		programSpawn->setUniform("outerRadius", scale);
		programSpawn->setUniform("clipRadiusScale", radiusScale * scale); // Also outer radius
		programSpawn->setUniform("clustering", cluster(interp));
		programSpawn->setUniform("individualSharpness", sharp(interp));
		programSpawn->setUniform("weight", weight);
		programSpawn->setUniform("interpolation", interp);


		vao->drawArrays(GL_POINTS, 0, vCount);

		m_intersectionCount[index]->unbind(GL_ATOMIC_COUNTER_BUFFER);
		m_intersectionBuffer[index]->unbind(GL_SHADER_STORAGE_BUFFER);
		m_offsetTexture[index]->unbindImageTexture(0);
		(index == 0 ? m_sphereLOD0PositionTexture : m_sphereLOD1PositionTexture)->unbindActive(0);
	}
		
	programSpawn->release();
	
	// m_spherePositionTexture->unbindActive(0);

	glMemoryBarrier(GL_ALL_BARRIER_BITS);

	m_sphereFramebuffer->unbind();

	//////////////////////////////////////////////////////////////////////////
	// Surface intersection pass
	//////////////////////////////////////////////////////////////////////////
	
	(bVisualizeOverlaps ? m_shadeFramebuffer : m_surfaceFramebuffer)->bind();
	// Framebuffer::defaultFBO()->bind();

	glMemoryBarrier(GL_ALL_BARRIER_BITS);
	
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glDepthMask(GL_TRUE);

	glClearDepth(1.0f);
	glClearColor(0.0f, 0.0f, 0.0f, 65535.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// m_spherePositionTexture->bindActive(0);
	// m_sphereNormalTexture->bindActive(1);
	m_offsetTexture[0]->bindActive(0);
	m_offsetTexture[1]->bindActive(1);
	m_intersectionBuffer[0]->bindBase(GL_SHADER_STORAGE_BUFFER, 1);
	m_intersectionBuffer[1]->bindBase(GL_SHADER_STORAGE_BUFFER, 2);
	m_intersectionCount[0]->bindBase(GL_ATOMIC_COUNTER_BUFFER, 1);
	m_intersectionCount[1]->bindBase(GL_ATOMIC_COUNTER_BUFFER, 2);
	// m_statisticsBuffer->bindBase(GL_SHADER_STORAGE_BUFFER, 2);
	m_sphereLOD0PositionTexture->bindActive(2);
	m_sphereLOD0NormalTexture->bindActive(3);
	m_sphereLOD1PositionTexture->bindActive(4);
	m_sphereLOD1NormalTexture->bindActive(5);

	programSurface->setUniform("modelViewMatrix", modelViewMatrix);
	programSurface->setUniform("projectionMatrix", projectionMatrix);
	programSurface->setUniform("modelViewProjectionMatrix", modelViewProjectionMatrix);
	programSurface->setUniform("inverseModelViewProjectionMatrix", inverseModelViewProjectionMatrix);
	programSurface->setUniform("normalMatrix", normalMatrix);
	programSurface->setUniform("lightPosition", vec3(worldLightPosition));
	programSurface->setUniform("ambientMaterial", ambientMaterial);
	programSurface->setUniform("diffuseMaterial", diffuseMaterial);
	programSurface->setUniform("specularMaterial", specularMaterial);
	programSurface->setUniform("shininess", shininess);
	programSurface->setUniform("focusPosition", focusPosition);
	programSurface->setUniform("sharpness", sharpness);
	programSurface->setUniform("coloring", uint(coloring));
	programSurface->setUniform("environment", environmentMapping);
	programSurface->setUniform("lens", lens);
	programSurface->setUniform("interpolation", clampedInterpolation(interpolation));

	programSurface->setUniform("gridScale", gridSize);
	programSurface->setUniform("gridDepth", gridDepth);
	programSurface->setUniform("minb", bounds.first);
	programSurface->setUniform("maxb", bounds.second);
	const auto t = float(glfwGetTime());
	programSurface->setUniform("time", t);

	m_vaoQuad->bind();
	programSurface->use();
	m_vaoQuad->drawArrays(GL_POINTS, 0, 1);
	programSurface->release();
	m_vaoQuad->unbind();

	m_intersectionBuffer[1]->unbind(GL_SHADER_STORAGE_BUFFER);
	m_intersectionBuffer[0]->unbind(GL_SHADER_STORAGE_BUFFER);
	m_intersectionCount[1]->unbind(GL_ATOMIC_COUNTER_BUFFER);
	m_intersectionCount[0]->unbind(GL_ATOMIC_COUNTER_BUFFER);

	m_sphereLOD1NormalTexture->unbindActive(5);
	m_sphereLOD1PositionTexture->unbindActive(4);
	m_sphereLOD0NormalTexture->unbindActive(3);
	m_sphereLOD0PositionTexture->unbindActive(2);
	m_offsetTexture[1]->unbindActive(1);
	m_offsetTexture[0]->unbindActive(0);;

	m_chainColors->unbind(GL_UNIFORM_BUFFER);
	m_residueColors->unbind(GL_UNIFORM_BUFFER);
	m_elementColorsRadii->unbind(GL_UNIFORM_BUFFER);

	if (!bVisualizeOverlaps) {
		m_surfaceFramebuffer->unbind();


		//////////////////////////////////////////////////////////////////////////
		// Shading
		//////////////////////////////////////////////////////////////////////////
		m_shadeFramebuffer->bind();
		glDepthMask(GL_FALSE);

		m_spherePositionTexture->bindActive(0);
		m_sphereNormalTexture->bindActive(1);
		m_sphereDiffuseTexture->bindActive(2);
		m_surfacePositionTexture->bindActive(3);
		m_surfaceNormalTexture->bindActive(4);
		m_surfaceDiffuseTexture->bindActive(5);
		m_depthTexture->bindActive(6);
		m_ambientTexture->bindActive(7);
		m_materialTextures[materialTextureIndex]->bindActive(8);
		m_environmentTextures[environmentTextureIndex]->bindActive(9);
		m_shadowColorTexture->bindActive(10);
		m_shadowDepthTexture->bindActive(11);

		programShade->setUniform("modelViewMatrix", modelViewMatrix);
		programShade->setUniform("projectionMatrix", projectionMatrix);
		programShade->setUniform("modelViewProjection", modelViewProjectionMatrix);
		programShade->setUniform("inverseModelViewProjectionMatrix", inverseModelViewProjectionMatrix);
		programShade->setUniform("normalMatrix", normalMatrix);
		programShade->setUniform("inverseNormalMatrix", inverseNormalMatrix);
		programShade->setUniform("modelLightMatrix", modelLightMatrix);
		programShade->setUniform("modelLightProjectionMatrix", modelLightProjectionMatrix);
		programShade->setUniform("lightPosition", vec3(worldLightPosition));
		programShade->setUniform("ambientMaterial", ambientMaterial);
		programShade->setUniform("diffuseMaterial", diffuseMaterial);
		programShade->setUniform("specularMaterial", specularMaterial);
		programShade->setUniform("distanceBlending", distanceBlending);
		programShade->setUniform("distanceScale", distanceScale);
		programShade->setUniform("shininess", shininess);
		programShade->setUniform("focusPosition", focusPosition);
		programShade->setUniform("objectCenter", objectCenter);
		programShade->setUniform("objectRadius", objectRadius);

		programShade->setUniform("spherePositionTexture", 0);
		programShade->setUniform("sphereNormalTexture", 1);
		programShade->setUniform("sphereDiffuseTexture", 2);

		programShade->setUniform("surfacePositionTexture", 3);
		programShade->setUniform("surfaceNormalTexture", 4);
		programShade->setUniform("surfaceDiffuseTexture", 5);

		programShade->setUniform("depthTexture", 6);
		programShade->setUniform("ambientTexture", 7);
		programShade->setUniform("materialTexture", 8);
		programShade->setUniform("environmentTexture", 9);
		programShade->setUniform("shadowColorTexture", 10);
		programShade->setUniform("shadowDepthTexture", 11);

		programShade->setUniform("environment", environmentMapping);
		programShade->setUniform("maximumCoCRadius", maximumCoCRadius);
		programShade->setUniform("aparture", aparture);
		programShade->setUniform("focalDistance", focalDistance);
		programShade->setUniform("focalLength", focalLength);
		programShade->setUniform("backgroundColor", viewer()->backgroundColor());


		m_vaoQuad->bind();
		programShade->use();
		m_vaoQuad->drawArrays(GL_POINTS, 0, 1);
		programShade->release();
		m_vaoQuad->unbind();

		m_shadowDepthTexture->unbindActive(11);
		m_shadowColorTexture->unbindActive(10);
		m_environmentTextures[environmentTextureIndex]->unbindActive(9);
		m_materialTextures[materialTextureIndex]->unbindActive(8);
		m_ambientTexture->unbindActive(7);
		m_depthTexture->unbindActive(6);
		m_surfaceDiffuseTexture->unbindActive(5);
		m_surfaceNormalTexture->unbindActive(4);
		m_surfacePositionTexture->unbindActive(3);
		m_sphereDiffuseTexture->unbindActive(2);
		m_sphereNormalTexture->unbindActive(1);
		m_spherePositionTexture->unbindActive(0);
	}

	m_shadeFramebuffer->unbind();


	// Draw test square
	// Framebuffer::defaultFBO()->bind();
	// glClearDepth(1.0f);
	// glClearColor(0.8f, 0.3f, 0.5f, 0.0f);
	// glDisable(GL_DEPTH_TEST);
	// glDepthFunc(GL_ALWAYS);
	// glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	// glDepthMask(GL_TRUE);
	// glDepthFunc(GL_ALWAYS);
	// glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// // // glPatchParameteri(GL_PATCH_VERTICES, 1);
	// programTest->use();

	// m_sphereLOD1PositionTexture->bindActive(0);
	// // m_offsetTexture->bindActive(1);
	// programTest->setUniform("modelViewProjectionMatrix", modelViewProjectionMatrix);
	// programTest->setUniform("interpolation", interpolation);
	// // glPointSize(10.f);
	// m_vaoQuad->drawArrays(GL_POINTS, 0, 1);
	// programTest->release();

	// // m_offsetTexture->unbindActive(1);
	// m_sphereMiddlePositionTexture->unbindActive(0);
	// m_sphereLOD0PositionTexture->unbindActive(0);

	// m_shadeFramebuffer->unbind();

	if (viewportSize == viewer()->viewportSize())
	{
		// Blit final image into visible framebuffer
		m_shadeFramebuffer->blit(GL_COLOR_ATTACHMENT0, { 0,0,viewer()->viewportSize().x, viewer()->viewportSize().y }, Framebuffer::defaultFBO().get(), GL_BACK, { 0,0,viewer()->viewportSize().x, viewer()->viewportSize().y }, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST);
	}
	else
	{
		m_colorTexture->bindActive(0);
		m_depthTexture->bindActive(1);

		glViewport(0, 0, viewer()->viewportSize().x, viewer()->viewportSize().y);
		glDepthMask(GL_TRUE);

		programDisplay->setUniform("colorTexture", 0);
		programDisplay->setUniform("depthTexture", 1);

		m_vaoQuad->bind();
		programDisplay->use();
		m_vaoQuad->drawArrays(GL_POINTS, 0, 1);
		programDisplay->release();
		m_vaoQuad->unbind();

		m_depthTexture->unbindActive(1);
		m_colorTexture->unbindActive(0);

	}

	// Restore OpenGL state
	currentState->apply();
}	