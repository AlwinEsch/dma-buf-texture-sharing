/*
 *  Copyright (C) 2020 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include "share_data.h"

#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#define EGL_EGLEXT_PROTOTYPES
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "kodi/gui/gl/Shader.h"

class ATTRIBUTE_HIDDEN CWorkRendererGL : public kodi::gui::gl::CShaderProgram
{
public:
  CWorkRendererGL(const std::string& socketServerFile,
                  const std::string& socketClientFile,
                  EGLContext eglContext,
                  int width,
                  int height);
  ~CWorkRendererGL();

  bool Init();
  void Deinit();
  void Render(uint8_t* buffer, GLint format);

  void OnCompiledAndLinked() override;
  bool OnEnabled() override;

private:
  void GetShaderPath(std::string& vert, std::string& frag);

  const float m_vertices[20] = {
      -1.0f, 1.0f,  0.0f, 0.0f, 0.0f, // top right
      1.0f,  -1.0f, 0.0f, 1.0f, 1.0f, // bottom left
      1.0f,  1.0f,  0.0f, 1.0f, 0.0f, // bottom right
      1.0f,  -1.0f, 0.0f, 0.0f, 1.0f, // bottom left
  };
  const unsigned char m_indices[4] = {0, 1, 3, 2};

  PFNEGLCREATEIMAGEKHRPROC eglCreateImageKHR = nullptr;
  PFNEGLDESTROYIMAGEKHRPROC eglDestroyImageKHR = nullptr;
  PFNEGLEXPORTDMABUFIMAGEQUERYMESAPROC eglExportDMABUFImageQueryMESA = nullptr;
  PFNEGLEXPORTDMABUFIMAGEMESAPROC eglExportDMABUFImageMESA = nullptr;

  const std::string m_socketServerFile;
  const std::string m_socketClientFile;

  EGLDisplay m_eglDisplay = EGL_NO_DISPLAY;
  EGLContext m_eglContext = EGL_NO_CONTEXT;
  EGLImageKHR m_eglImage = EGL_NO_IMAGE_KHR;

  int m_width;
  int m_height;

  // EGL (extension: EGL_MESA_image_dma_buf_export): Get file descriptor (texture_dmabuf_fd) for the EGL image and get its
  // storage data (texture_storage_metadata - fourcc, stride, offset)
  int m_socket = -1;
  int m_textureDmaBufFd = -1;
  struct texture_storage_metadata_t m_textureStorageMetadata;

  GLuint m_vertexVBO = 0;
  GLuint m_indexVBO = 0;

  GLint m_aPosition = -1;
  GLint m_aCoord = -1;

  GLuint m_texture = 0;
  GLuint m_framebuffer = 0;
  GLuint m_renderbuffer = 0;
};
