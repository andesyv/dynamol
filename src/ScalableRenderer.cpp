#include "ScalableRenderer.h"
#include <globjects/State.h>
#include <iostream>
// #include <filesystem>
// #include <imgui.h>
#include "Viewer.h"
// #include "Scene.h"
// #include "Protein.h"

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
}

void ScalableRenderer::display()
{
	// if (viewer()->scene()->protein()->atoms().size() == 0)
	// 	return;

	// SaveOpenGL state
	auto currentState = State::currentState();

	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);

	const auto shader = shaderProgram("default");
	shader->use();
	
	m_ssvao.bind();
	glDrawArrays(GL_TRIANGLES, 0, 6);
	m_ssvao.unbind();

	shader->release();
	// Restore OpenGL state
	currentState->apply();
}