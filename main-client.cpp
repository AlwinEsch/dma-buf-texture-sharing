#include "ViewRendererGL.h"

const char *CLIENT_FILE = "/tmp/test_client";

int main(int argc, char **argv)
{
  // Open X11 display and create window
  Display *x11_display = XOpenDisplay(NULL);
  int screen = DefaultScreen(x11_display);
  Window x11_window = XCreateSimpleWindow(x11_display, RootWindow(x11_display, screen), 0, 0, 1280, 720, 1,
                                      BlackPixel(x11_display, screen), WhitePixel(x11_display, screen));
  XStoreName(x11_display, x11_window, "Client");
  XMapWindow(x11_display, x11_window);

  EGLDisplay eglDisplay = eglGetDisplay(x11_display);

  // initialize the EGL display connection
  EGLint major, minor;
  eglInitialize(eglDisplay, &major, &minor);

  // Set OpenGL rendering API
  eglBindAPI(EGL_OPENGL_API);

  // get an appropriate EGL frame buffer configuration
  EGLConfig config;
  EGLint num_config;

  EGLint const attribute_list_config[] = {
      EGL_RED_SIZE, 8,
      EGL_GREEN_SIZE, 8,
      EGL_BLUE_SIZE, 8,
      EGL_ALPHA_SIZE, 8,
      EGL_NONE};

  eglChooseConfig(eglDisplay, attribute_list_config, &config, 1, &num_config);

  // create an EGL rendering context
  EGLint const attrib_list[] = {
    EGL_CONTEXT_MAJOR_VERSION_KHR, 3,
    EGL_CONTEXT_MINOR_VERSION_KHR, 2,
    EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR,
      EGL_NONE};
  EGLContext eglContext = eglCreateContext(eglDisplay, config, EGL_NO_CONTEXT, attrib_list);

  EGLSurface eglSurface = eglCreateWindowSurface(eglDisplay, config, x11_window, nullptr);

  // connect the context to the surface
  eglMakeCurrent(eglDisplay, eglSurface, eglSurface, eglContext);

  unsigned int VAO;
  glGenVertexArrays(1, &VAO);
  glBindVertexArray(VAO);

  CViewRendererOpenGL renderer(CLIENT_FILE);
  if (!renderer.Init())
    return 1;

  bool quit = false;
  while (!quit)
  {
    // Draw scene (uses shared texture)
    // clear
    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    renderer.Render();

    eglSwapBuffers(eglDisplay, eglSurface);

    // Check for errors
    assert(glGetError() == GL_NO_ERROR);
    assert(eglGetError() == EGL_SUCCESS);

    while (XPending(x11_display))
    {
      XEvent xev;
      XNextEvent(x11_display, &xev);

      if (xev.type == KeyPress)
        quit = true;
    }
  }

  glDeleteVertexArrays(1, &VAO);

  eglTerminate(eglDisplay);
  XDestroyWindow(x11_display, x11_window);
  XCloseDisplay(x11_display);

  return 0;
}
