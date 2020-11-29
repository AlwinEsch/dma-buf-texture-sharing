/*
 *  Copyright (C) 2020 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#define EGL_EGLEXT_PROTOTYPES
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "kodi/gui/gl/Shader.h"

class ATTRIBUTE_HIDDEN CRendererOpenGL : public kodi::gui::gl::CShaderProgram
{
public:
  CRendererOpenGL(const std::string& socketClientFile);
  ~CRendererOpenGL();

  bool Init();
  void Deinit();
  void Render();

  void OnCompiledAndLinked() override;
  bool OnEnabled() override;

private:
  void GetShaderPath(std::string& vert, std::string& frag);

  const float m_vertices[20] = {
      -0.9f, 0.9f,  0.0f, 0.0f, 0.0f, // bottom left
      0.9f,  0.9f,  0.0f, 1.0f, 0.0f, // bottom right
      0.9f,  -0.9f, 0.0f, 1.0f, 1.0f, // top right
      -0.9f, -0.9f, 0.0f, 0.0f, 1.0f // top left
  };
  const unsigned char m_indices[4] = {0, 1, 3, 2};

  const std::string m_socketClientFile;

  EGLDisplay m_eglDisplay = EGL_NO_DISPLAY;
  EGLImage m_image = EGL_NO_IMAGE;

  GLuint m_vertexVBO = 0;
  GLuint m_indexVBO = 0;

  GLint m_aPosition = -1;
  GLint m_aCoord = -1;
  GLuint m_texture = 0;

  PFNEGLCREATEIMAGEKHRPROC eglCreateImageKHR = nullptr;
  PFNEGLDESTROYIMAGEKHRPROC eglDestroyImageKHR = nullptr;
  PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES = nullptr;
};
