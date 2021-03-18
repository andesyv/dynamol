#include "ScalableRenderer.h"
#include <globjects/State.h>
#include <iostream>
// #include <filesystem>
// #include <imgui.h>
#include "Viewer.h"
#include "Scene.h"
#include "Protein.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>

using namespace dynamol;
using namespace gl;
using namespace glm;
using namespace globjects;

ScalableRenderer::ScalableRenderer(Viewer *viewer) : Renderer(viewer)
{
	createShaderProgram("default", {
		{ GL_VERTEX_SHADER,"./res/scaling/default-vs.glsl" },
		{ GL_FRAGMENT_SHADER,"./res/scaling/default-fs.glsl" },
	});

	createShaderProgram("point", {
		{ GL_VERTEX_SHADER,"./res/scaling/point-vs.glsl" },
		{ GL_GEOMETRY_SHADER,"./res/scaling/point-gs.glsl" },
		{ GL_FRAGMENT_SHADER,"./res/scaling/point-fs.glsl" },
	});

	createShaderProgram("grid", {
		{ GL_VERTEX_SHADER,"./res/scaling/grid-vs.glsl" },
		{ GL_FRAGMENT_SHADER,"./res/scaling/grid-fs.glsl" },
	});

	// Screen spaced example buffer
	m_ssvbo.setData(std::vector{
		vec3{-1.f, -1.f, 0.f},
		vec3{1.f, -1.f, 0.f},
		vec3{1.f, 1.f, 0.f},
		vec3{-1.f, -1.f, 0.f},
		vec3{1.f, 1.f, 0.f},
		vec3{-1.f, 1.f, 0.f}
	}, GL_STATIC_DRAW);

	auto binding = m_ssvao.binding(0);
	binding->setAttribute(0);
	binding->setBuffer(&m_ssvbo, 0, sizeof(vec3));
	binding->setFormat(3, GL_FLOAT, GL_FALSE, 0);

	m_ssvao.enable(0);

	if (!viewer->scene()->protein()->atoms().empty()) {
		// m_atompos.setStorage(viewer->scene()->protein()->atoms().back(), gl::BufferStorageMask::GL_MAP_READ_BIT);
		m_atompos.bindBase(GL_SHADER_STORAGE_BUFFER, 4);

		m_staticpos.setData(viewer->scene()->protein()->atoms().back(), GL_STATIC_DRAW);
		auto binding = m_atomvao.binding(0);
		binding->setAttribute(0);
		binding->setBuffer(&m_staticpos, 0, sizeof(vec4));
		binding->setFormat(4, GL_FLOAT, GL_FALSE, 0);
		m_atomvao.enable(0);
	}


	m_framebuffer.bind();
	m_framebufferPositionTexture.bind();
	m_framebufferPositionTexture.image2D(0, GL_RGB, screenSize, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
	m_framebufferPositionTexture.setParameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	m_framebufferPositionTexture.setParameter(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	m_framebuffer.attachTexture(GL_COLOR_ATTACHMENT0, &m_framebufferPositionTexture, 0);
	m_framebufferCountTexture.bind();
	m_framebufferCountTexture.image2D(0, GL_RGB, screenSize, 0, GL_RGB, GL_UNSIGNED_SHORT, nullptr);
	m_framebufferCountTexture.setParameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	m_framebufferCountTexture.setParameter(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	m_framebuffer.attachTexture(GL_COLOR_ATTACHMENT1, &m_framebufferCountTexture, 0);
	m_framebuffer.unbind();

	switch (m_framebuffer.checkStatus()) {
		case GL_FRAMEBUFFER_UNDEFINED:
		case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
		case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
		case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
		case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER :
		case GL_FRAMEBUFFER_UNSUPPORTED :
		case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE :
		case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS :
			throw std::runtime_error{"Ohno"};
		case GL_FRAMEBUFFER_COMPLETE:
			break;
	}
	

	resizeSSBO(2);
}

void ScalableRenderer::display()
{
	if (viewer()->scene()->protein()->atoms().size() == 0)
		return;

	const auto& atom = viewer()->scene()->protein()->atoms().back();
	const auto modelViewProjectionMatrix = viewer()->modelViewProjectionTransform();
	const auto inverseModelViewProjectionMatrix = inverse(modelViewProjectionMatrix);
	const auto inverseModelMatrix = inverse(viewer()->modelTransform());
	const std::pair bounds{viewer()->scene()->protein()->minimumBounds(), viewer()->scene()->protein()->maximumBounds()};

	// const auto pointShader = shaderProgram("point");
	const auto shader = shaderProgram("default");
	const auto gridShader = shaderProgram("grid");

	// SaveOpenGL state
	auto currentState = State::currentState();
	// const auto framebufferSize = viewer()->viewportSize();
	

	// /// Forward position discretization step
	// m_framebuffer.bind();
	// glViewport(0, 0, screenSize.x, screenSize.y);
	// glClear(GL_COLOR_BUFFER_BIT);
	// pointShader->use();
	// pointShader->setUniform("modelViewProjectionMatrix", modelViewProjectionMatrix);

	// glDisable(GL_CULL_FACE);
	// glDisable(GL_DEPTH_TEST);

	// m_atomvao.bind();
	// glDrawArrays(GL_POINTS, 0, static_cast<int>(atom.size()));
	// m_atomvao.unbind();

	// m_framebuffer.unbind();
	// glViewport(0, 0, framebufferSize.x, framebufferSize.y);

	// Possibly resize grid:
	if (viewer()->gridSize != gridSize) {
		resizeSSBO(viewer()->gridSize);
	} else {
		// Clear SSBO
		const auto gSize = gridSize * gridSize * gridSize;
		m_atompos.clearSubData(GL_RGBA32UI, 0, sizeof(glm::uvec4) * gSize, GL_RGBA, GL_UNSIGNED_INT, nullptr);
		m_atompos.clearSubData(GL_RGBA32UI, sizeof(glm::uvec4) * gSize, sizeof(glm::uvec4) * gSize, GL_RGBA, GL_UNSIGNED_INT, nullptr);
	}



	gridShader->use();
	gridShader->setUniform("gridScale", gridSize);
	gridShader->setUniform("minb", bounds.first);
	gridShader->setUniform("maxb", bounds.second);
	gridShader->setUniform("modelViewProjectionMatrix", modelViewProjectionMatrix);
	gridShader->setUniform("inverseModelViewProjectionMatrix", inverseModelViewProjectionMatrix);

	m_atomvao.bind();
	glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(atom.size()));
	m_atomvao.unbind();


	// Raymarch render:
	glDisable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glDepthFunc(GL_LESS);

	shader->use();

	shader->setUniform("posCount", static_cast<int>(atom.size()));
	shader->setUniform("modelViewProjectionMatrix", modelViewProjectionMatrix);
	shader->setUniform("inverseModelViewProjectionMatrix", inverseModelViewProjectionMatrix);
	shader->setUniform("inverseModelMatrix", inverseModelMatrix);
	shader->setUniform("gridScale", gridSize);
	// m_framebufferPositionTexture.bindActive(0);
	
	m_ssvao.bind();
	glDrawArrays(GL_TRIANGLES, 0, 6);
	m_ssvao.unbind();

	shader->release();
	// Restore OpenGL state
	currentState->apply();
}

void ScalableRenderer::resizeSSBO(glm::uint size) {
	m_atompos.setData(
		(sizeof(glm::uvec4) + sizeof(glm::uvec4)) * size * size * size,
		nullptr, gl::GL_DYNAMIC_COPY);
	gridSize = size;
	// m_atompos.bindBase(GL_SHADER_STORAGE_BUFFER, 4);
}