/*
 *  Copyright (C) 2020 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "WorkRendererGL.h"

#include "OpenGLExtensionCheck.h"
#include "socket.h"

#include <thread>

CWorkRendererGL::CWorkRendererGL(const std::string& socketServerFile,
                                 const std::string& socketClientFile,
                                 EGLContext eglContext,
                                 int width,
                                 int height)
  : m_socketServerFile(socketServerFile),
    m_socketClientFile(socketClientFile),
    m_eglContext(eglContext),
    m_width(width),
    m_height(height)
{
}

CWorkRendererGL::~CWorkRendererGL()
{
  Deinit();
}

bool CWorkRendererGL::Init()
{
  std::string vertShader, fraqShader;
  GetShaderPath(vertShader, fraqShader);
  if (!LoadShaderFiles(vertShader, fraqShader) || !CompileAndLink())
  {
    kodi::Log(ADDON_LOG_ERROR, "Failed to create or compile shader");
    return false;
  }

  m_eglDisplay = eglGetCurrentDisplay();
  if (m_eglDisplay == EGL_NO_DISPLAY)
  {
    kodi::Log(ADDON_LOG_ERROR, "Not get EGL display");
    return false;
  }

  if (!GLExtCheck::CheckExtensionSupported("EGL_KHR_image_base"))
  {
    kodi::Log(ADDON_LOG_ERROR, "Needed EGL extension \"EGL_KHR_image_base\" not available");
    return false;
  }

  if (!GLExtCheck::CheckExtensionSupported("EGL_MESA_image_dma_buf_export"))
  {
    kodi::Log(ADDON_LOG_ERROR,
              "Needed EGL extension \"EGL_MESA_image_dma_buf_export\" not available");
    return false;
  }

  eglCreateImageKHR = (decltype(eglCreateImageKHR))GLExtCheck::GetEGLFunc("eglCreateImageKHR");
  eglDestroyImageKHR = (decltype(eglDestroyImageKHR))GLExtCheck::GetEGLFunc("eglDestroyImageKHR");
  eglExportDMABUFImageQueryMESA = (decltype(eglExportDMABUFImageQueryMESA))GLExtCheck::GetEGLFunc(
      "eglExportDMABUFImageQueryMESA");
  eglExportDMABUFImageMESA =
      (decltype(eglExportDMABUFImageMESA))GLExtCheck::GetEGLFunc("eglExportDMABUFImageMESA");

  // GL: Create and populate the texture
  glGenTextures(1, &m_texture);
  glBindTexture(GL_TEXTURE_2D, m_texture);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_width, m_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

  static const EGLint attrib[] = {EGL_IMAGE_PRESERVED_KHR, EGL_FALSE, EGL_GL_TEXTURE_LEVEL_KHR, 0,
                                  EGL_NONE};

  // EGL: Create EGL image from the GL texture
  m_eglImage = eglCreateImageKHR(m_eglDisplay, m_eglContext, EGL_GL_TEXTURE_2D_KHR,
                                 reinterpret_cast<EGLClientBuffer>(m_texture), attrib);
  if (m_eglImage == EGL_NO_IMAGE)
  {
    kodi::Log(ADDON_LOG_ERROR, "Failed to get EGL image");
    return false;
  }

  // The next line works around an issue in radeonsi driver (fixed in master at the time of writing). If you are
  // not having problems with texture rendering until the first texture update you can omit this line
  glFinish();

  EGLBoolean queried = eglExportDMABUFImageQueryMESA(
      m_eglDisplay, m_eglImage, &m_textureStorageMetadata.fourcc,
      &m_textureStorageMetadata.num_planes, &m_textureStorageMetadata.modifiers);
  if (!queried)
  {
    kodi::Log(ADDON_LOG_ERROR, "Failed to export DMABUF image query");
    return false;
  }

  EGLBoolean exported = eglExportDMABUFImageMESA(m_eglDisplay, m_eglImage, &m_textureDmaBufFd,
                                                 &m_textureStorageMetadata.stride, nullptr);
  if (!exported)
  {
    kodi::Log(ADDON_LOG_ERROR, "Failed to export DMABUF image");
    return false;
  }

  eglDestroyImageKHR(m_eglDisplay, m_eglImage);

  m_textureStorageMetadata.width = m_width;
  m_textureStorageMetadata.height = m_height;

  glGenBuffers(1, &m_vertexVBO);
  glGenBuffers(1, &m_indexVBO);

  glGenFramebuffers(1, &m_framebuffer);
  glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);
  //Attach 2D texture to this FBO
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_texture, 0);

  glGenRenderbuffers(1, &m_renderbuffer);
  glBindRenderbuffer(GL_RENDERBUFFER, m_renderbuffer);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, m_width, m_height);

  //Attach depth buffer to FBO
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_renderbuffer);

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glBindTexture(GL_TEXTURE_2D, 0);

  // Unix Domain Socket: Send file descriptor (texture_dmabuf_fd) and texture storage data (texture_storage_metadata)
  m_socket = create_socket(m_socketServerFile.c_str());
  std::thread thread = std::thread([&] {
    while (m_socket >= 0 && connect_socket(m_socket, m_socketClientFile.c_str()) != 0)
      ;
    write_fd(m_socket, m_textureDmaBufFd, &m_textureStorageMetadata,
             sizeof(m_textureStorageMetadata));
    close(m_socket);
    m_socket = -1;
    close(m_textureDmaBufFd);
    m_textureDmaBufFd = -1;
  });

  thread.detach();

  return true;
}

void CWorkRendererGL::Deinit()
{
  if (m_socket >= 0)
  {
    int socket = m_socket;
    m_socket = -1;
    close(socket);
    if (m_textureDmaBufFd >= 0)
    {
      m_textureDmaBufFd = -1;
      close(m_textureDmaBufFd);
    }
  }

  glBindFramebuffer(GL_RENDERBUFFER, 0);
  glDeleteRenderbuffers(1, &m_renderbuffer);
  m_renderbuffer = 0;

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glDeleteFramebuffers(1, &m_framebuffer);
  m_framebuffer = 0;

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, 0);
  glDeleteTextures(1, &m_texture);
  m_texture = 0;

  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glDeleteBuffers(1, &m_vertexVBO);
  m_vertexVBO = 0;

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
  glDeleteBuffers(1, &m_indexVBO);
  m_indexVBO = 0;
}

void CWorkRendererGL::Render(uint8_t* buffer, GLint format)
{
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, m_texture);
  glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);

  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_width, m_height, format, GL_UNSIGNED_BYTE, buffer);

  glBindBuffer(GL_ARRAY_BUFFER, m_vertexVBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(m_vertices), m_vertices, GL_STATIC_DRAW);

  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_indexVBO);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(m_indices), m_indices, GL_STATIC_DRAW);

  EnableShader();
  glDrawElements(GL_TRIANGLE_STRIP, 4, GL_UNSIGNED_BYTE, 0);
  DisableShader();

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glBindTexture(GL_TEXTURE_2D, 0);

  glFlush();
}

void CWorkRendererGL::OnCompiledAndLinked()
{
  m_aPosition = glGetAttribLocation(ProgramHandle(), "aPos");
  m_aCoord = glGetAttribLocation(ProgramHandle(), "aTexCoords");
}

bool CWorkRendererGL::OnEnabled()
{
  return true;
}

void CWorkRendererGL::GetShaderPath(std::string& vert, std::string& frag)
{
  std::string path;

#if defined(HAS_GL)
  int glslMajor = 0;
  int glslMinor = 0;

  const char* ver = reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION));
  if (ver)
  {
    sscanf(ver, "%d.%d", &glslMajor, &glslMinor);
  }
  else
  {
    glslMajor = 1;
    glslMinor = 0;
  }

  if (glslMajor >= 2 || (glslMajor == 1 && glslMinor >= 50))
    path = kodi::GetAddonPath("resources/shaders/GL/1.5/");
  else
    path = kodi::GetAddonPath("resources/shaders/GL/1.2/");
#else
  path = kodi::GetAddonPath("resources/shaders/GLES/");
#endif

  kodi::Log(ADDON_LOG_DEBUG, "Used addon shader language path: '%s'", path.c_str());

  vert = path + "/vert.glsl";
  frag = path + "/frag.glsl";
}
