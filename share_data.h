#pragma once

#define EGL_EGLEXT_PROTOTYPES
#include <EGL/egl.h>
#include <EGL/eglext.h>

struct texture_storage_metadata_t
{
  int width;
  int height;
  int fourcc;
  EGLint offset;
  EGLint stride;
  int num_planes;
  EGLuint64KHR modifiers;
};
