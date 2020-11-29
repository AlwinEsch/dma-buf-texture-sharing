/*
 *  Copyright (C) 2020 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "OpenGLExtensionCheck.h"

#include <algorithm>
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#define EGL_EGLEXT_PROTOTYPES
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <vector>

namespace GLExtCheck
{

static std::vector<std::string> displayExts;

std::vector<std::string> SplitExtensions(const std::string& str)
{
  std::vector<std::string> strings;
  size_t start;
  size_t end = 0;
  while ((start = str.find_first_not_of(' ', end)) != std::string::npos)
  {
    end = str.find(' ', start);
    strings.push_back(str.substr(start, end - start));
  }
  return strings;
}

void FindSupportedExtensions(EGLDisplay m_eglDisplay)
{
  // Get the GL version number
  int renderVersionMajor = 0;
  int renderVersionMinor = 0;

  displayExts = SplitExtensions(eglQueryString(m_eglDisplay, EGL_EXTENSIONS));

  const char* ver = reinterpret_cast<const char*>(glGetString(GL_VERSION));
  if (ver != 0)
  {
    sscanf(ver, "%d.%d", &renderVersionMajor, &renderVersionMinor);
  }

  if (renderVersionMajor > 3 || (renderVersionMajor == 3 && renderVersionMinor >= 2))
  {
    GLint n = 0;
    glGetIntegerv(GL_NUM_EXTENSIONS, &n);
    if (n > 0)
    {
      GLint i;
      for (i = 0; i < n; i++)
      {
        displayExts.emplace_back(reinterpret_cast<const char*>(glGetStringi(GL_EXTENSIONS, i)));
      }
    }
  }
  else
  {
    auto extensions = reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
    if (extensions)
    {
      std::vector<std::string> ext = SplitExtensions(extensions);
      displayExts.insert(displayExts.begin(), ext.begin(), ext.end());
    }
  }
}

bool CheckExtensionSupported(const std::string& name)
{
  if (displayExts.empty())
  {
    FindSupportedExtensions(eglGetCurrentDisplay());
  }
  return std::find(displayExts.begin(), displayExts.end(), name) != displayExts.end();
}

void* GetEGLFunc(const std::string& name)
{
  return reinterpret_cast<void*>(eglGetProcAddress(name.c_str()));
}

} /* namespace OpenGLExtensionCheck */
