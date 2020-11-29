#include "WorkRendererGL.h"

// Socket paths for sending/receiving file descriptor and image storage data
const char* CLIENT_FILE = "/tmp/test_client";
const char* SERVER_FILE = "/tmp/test_server";

int main(int argc, char** argv)
{
  EGLDisplay eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);

  // initialize the EGL display connection
  EGLint major, minor;
  eglInitialize(eglDisplay, &major, &minor);

  // Set OpenGL rendering API
  eglBindAPI(EGL_OPENGL_API);

  // get an appropriate EGL frame buffer configuration
  EGLConfig config;
  EGLint num_config;
  const EGLint attribute_list_config[] = {
      EGL_SURFACE_TYPE,
      EGL_PBUFFER_BIT,
      EGL_RENDERABLE_TYPE,
      EGL_OPENGL_BIT,
      EGL_RED_SIZE,
      1,
      EGL_GREEN_SIZE,
      1,
      EGL_BLUE_SIZE,
      1,
      EGL_ALPHA_SIZE,
      1,
      EGL_NONE,
  };

  eglChooseConfig(eglDisplay, attribute_list_config, &config, 1, &num_config);

  // create an EGL rendering context
  const EGLint attrib_list[] = {EGL_CONTEXT_MAJOR_VERSION_KHR,
                                3,
                                EGL_CONTEXT_MINOR_VERSION_KHR,
                                2,
                                EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR,
                                EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR,
                                EGL_NONE};
  EGLContext eglContext = eglCreateContext(eglDisplay, config, EGL_NO_CONTEXT, attrib_list);

  const EGLint pbufferAttribs[] = {
      EGL_WIDTH,          9,
      EGL_HEIGHT,         9,
      EGL_TEXTURE_TARGET, EGL_TEXTURE_2D,
      EGL_TEXTURE_FORMAT, EGL_TEXTURE_RGBA,
      EGL_NONE,
  };

  // create an EGL window surface
  EGLSurface eglSurface = eglCreatePbufferSurface(eglDisplay, config, pbufferAttribs);

  // connect the context to the surface
  eglMakeCurrent(eglDisplay, eglSurface, eglSurface, eglContext);

  FILE* pFile = fopen("./test.data", "rb");
  if (pFile == nullptr)
  {
    fputs("File error", stderr);
    exit(1);
  }

  fseek(pFile, 0, SEEK_END);
  size_t lSize = ftell(pFile);
  rewind(pFile);
  uint8_t* buffer = (uint8_t*)malloc(sizeof(uint8_t) * lSize * 2);
  if (buffer == nullptr)
  {
    fputs("Memory error", stderr);
    exit(2);
  }

  size_t result = fread(buffer, 1, lSize, pFile);
  if (result != lSize)
  {
    fputs("Reading error", stderr);
    exit(3);
  }

  fclose(pFile);

  unsigned int VAO;
  glGenVertexArrays(1, &VAO);
  glBindVertexArray(VAO);

  CWorkRendererGL* renderer = new CWorkRendererGL(SERVER_FILE, CLIENT_FILE, eglContext, 3072, 1355);
  if (!renderer->Init())
    return 1;

  time_t last_time = time(nullptr);
  bool swap = false;
  bool quit = false;
  while (!quit)
  {
    time_t cur_time = time(nullptr);
    if (last_time < cur_time)
    {
      last_time = cur_time;

      renderer->Render(buffer, swap ? GL_RGBA : GL_BGRA);
      swap = !swap;
    }

    // Check for errors
    assert(glGetError() == GL_NO_ERROR);
    assert(eglGetError() == EGL_SUCCESS);
  }

  glDeleteVertexArrays(1, &VAO);

  return 0;
}
