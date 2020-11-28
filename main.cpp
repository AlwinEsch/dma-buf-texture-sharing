
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <time.h>

#include <X11/Xlib.h>
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#define EGL_EGLEXT_PROTOTYPES
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "socket.h"
#include "window.h"
#include "render.h"

#include <algorithm>
#include <string>
#include <vector>

void parse_arguments(int argc, char **argv, int *is_server);

namespace CShareProcess
{

EGLDisplay m_eglDisplay;
EGLContext m_eglContext;
EGLSurface m_eglSurface;
EGLDeviceEXT m_eglDevice = EGL_NO_DEVICE_EXT;

PFNEGLCREATEIMAGEKHRPROC eglCreateImageKHR = nullptr;
PFNEGLDESTROYIMAGEKHRPROC eglDestroyImageKHR = nullptr;
PFNEGLEXPORTDMABUFIMAGEQUERYMESAPROC eglExportDMABUFImageQueryMESA = nullptr;
PFNEGLEXPORTDMABUFIMAGEMESAPROC eglExportDMABUFImageMESA = nullptr;
PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES = nullptr;

} /* namespace CShareProcess */

int main(int argc, char **argv)
{
  using namespace CShareProcess;

  // Parse arguments
  int is_server;
  parse_arguments(argc, argv, &is_server);

  Window x11_window;
  if (!is_server)
  {
    // Create X11 window
    Display *x11_display;
    create_x11_window(is_server, &x11_display, &x11_window);

    m_eglDisplay = eglGetDisplay(x11_display);
  }
  else
  {
    // get an EGL display connection

    m_eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  }
  // initialize the EGL display connection
  EGLint major, minor;
  eglInitialize(m_eglDisplay, &major, &minor);

  // Set OpenGL rendering API
  eglBindAPI(EGL_OPENGL_API);

  // get an appropriate EGL frame buffer configuration
  EGLConfig config;
  EGLint num_config;
  if (is_server)
  {
    EGLint const attribute_list_config[] = {
      EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
      EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
      EGL_RED_SIZE,   1,
      EGL_GREEN_SIZE, 1,
      EGL_BLUE_SIZE,  1,
      EGL_ALPHA_SIZE, 1,
      EGL_NONE,};

    eglChooseConfig(m_eglDisplay, attribute_list_config, &config, 1, &num_config);
  }
  else
  {
    EGLint const attribute_list_config[] = {
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_NONE};

    eglChooseConfig(m_eglDisplay, attribute_list_config, &config, 1, &num_config);
  }


  // create an EGL rendering context
  EGLint const attrib_list[] = {
    EGL_CONTEXT_MAJOR_VERSION_KHR, 3,
    EGL_CONTEXT_MINOR_VERSION_KHR, 2,
    EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR,
      EGL_NONE};
  m_eglContext = eglCreateContext(m_eglDisplay, config, EGL_NO_CONTEXT, attrib_list);

  const EGLint pbufferAttribs[] =
  {
    EGL_WIDTH, 3072,
    EGL_HEIGHT, 1355,
    EGL_TEXTURE_TARGET,
    EGL_TEXTURE_2D,
    EGL_TEXTURE_FORMAT,
    EGL_TEXTURE_RGBA,
    EGL_NONE,
  };

    // create an EGL window surface
  if (is_server)
    m_eglSurface = eglCreatePbufferSurface(m_eglDisplay, config, pbufferAttribs);
  else
    m_eglSurface = eglCreateWindowSurface(m_eglDisplay, config, x11_window, nullptr);

  // connect the context to the surface
  eglMakeCurrent(m_eglDisplay, m_eglSurface, m_eglSurface, m_eglContext);

  // Setup GL scene
  gl_setup_scene();

  // -----------------------------
  // --- Texture sharing start ---
  // -----------------------------

  // Socket paths for sending/receiving file descriptor and image storage data
  const char *SERVER_FILE = "/tmp/test_server";
  const char *CLIENT_FILE = "/tmp/test_client";
  // Custom image storage data description to transfer over socket
  struct texture_storage_metadata_t
  {
      int fourcc;
      EGLint offset;
      EGLint stride;
      int num_planes;
      EGLuint64KHR modifiers;
  };

  // GL texture that will be shared
  GLuint texture;
  GLuint framebuffer;
  unsigned int VBO, VAO, EBO;

  CShareProcess::eglCreateImageKHR = (PFNEGLCREATEIMAGEKHRPROC)eglGetProcAddress("eglCreateImageKHR");
  CShareProcess::eglDestroyImageKHR = (PFNEGLDESTROYIMAGEKHRPROC)eglGetProcAddress("eglDestroyImageKHR");
  CShareProcess::eglExportDMABUFImageQueryMESA = (PFNEGLEXPORTDMABUFIMAGEQUERYMESAPROC)eglGetProcAddress("eglExportDMABUFImageQueryMESA");
  CShareProcess::eglExportDMABUFImageMESA = (PFNEGLEXPORTDMABUFIMAGEMESAPROC)eglGetProcAddress("eglExportDMABUFImageMESA");
  CShareProcess::glEGLImageTargetTexture2DOES = (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)eglGetProcAddress("glEGLImageTargetTexture2DOES");

  // The next `if` block contains server code in the `true` branch and client code in the `false` branch. The `true` branch is always executed first and the `false` branch after it (in a different process). This is because the server loops at the end of the branch until it can send a message to the client and the client blocks at the start of the branch until it has a message to read. This way the whole `if` block from top to bottom represents the order of events as they happen.
  uint8_t *buffer = nullptr;
  if (is_server)
  {
    FILE *pFile = fopen("./test.data" , "rb");
    if (pFile==NULL)
    {
      fputs ("File error",stderr);
      exit (1);
    }

    fseek(pFile , 0 , SEEK_END);
    size_t lSize = ftell(pFile);
    rewind(pFile);
    buffer = (uint8_t*)malloc(sizeof(uint8_t)*lSize*2);
    if (buffer == NULL)
    {
      fputs ("Memory error",stderr);
      exit (2);
    }

    size_t result = fread (buffer,1,lSize,pFile);
    if (result != lSize)
    {
      fputs ("Reading error",stderr);
      exit (3);
    }

    fclose (pFile);

    // GL: Create and populate the texture
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);


    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 3072, 1355, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    static const EGLint attrib[] = {
      EGL_IMAGE_PRESERVED_KHR, EGL_FALSE,
      EGL_GL_TEXTURE_LEVEL_KHR, 0,
      EGL_NONE
    };

    // EGL: Create EGL image from the GL texture
    EGLImage image = CShareProcess::eglCreateImageKHR(m_eglDisplay,
                                                      m_eglContext,
                                                      EGL_GL_TEXTURE_2D_KHR,
                                                      reinterpret_cast<EGLClientBuffer>(texture),
                                                      attrib);
    assert(image != EGL_NO_IMAGE);

    // The next line works around an issue in radeonsi driver (fixed in master at the time of writing). If you are
    // not having problems with texture rendering until the first texture update you can omit this line
    glFinish();

    // EGL (extension: EGL_MESA_image_dma_buf_export): Get file descriptor (texture_dmabuf_fd) for the EGL image and get its
    // storage data (texture_storage_metadata - fourcc, stride, offset)
    int texture_dmabuf_fd;
    struct texture_storage_metadata_t texture_storage_metadata;

    EGLBoolean queried = CShareProcess::eglExportDMABUFImageQueryMESA(m_eglDisplay,
                                                        image,
                                                        &texture_storage_metadata.fourcc,
                                                        &texture_storage_metadata.num_planes,
                                                        &texture_storage_metadata.modifiers);
    assert(queried);

    EGLBoolean exported = CShareProcess::eglExportDMABUFImageMESA(m_eglDisplay,
                                                    image,
                                                    &texture_dmabuf_fd,
                                                    &texture_storage_metadata.stride,
                                                    nullptr);
    assert(exported);

    CShareProcess::eglDestroyImageKHR(m_eglDisplay, image);


    // Unix Domain Socket: Send file descriptor (texture_dmabuf_fd) and texture storage data (texture_storage_metadata)
    int sock = create_socket(SERVER_FILE);
    while (connect_socket(sock, CLIENT_FILE) != 0)
        ;
    write_fd(sock, texture_dmabuf_fd, &texture_storage_metadata, sizeof(texture_storage_metadata));
    close(sock);
    close(texture_dmabuf_fd);

    // Shader source that draws a textures quad
    const char *vertex_shader_source = "#version 330 core\n"
                                      "layout (location = 0) in vec3 aPos;\n"
                                      "layout (location = 1) in vec2 aTexCoords;\n"

                                      "out vec2 TexCoords;\n"

                                      "void main()\n"
                                      "{\n"
                                      "   TexCoords = aTexCoords;\n"
                                      "   gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0);\n"
                                      "}\0";
    const char *fragment_shader_source = "#version 330 core\n"
                                        "out vec4 FragColor;\n"

                                        "in vec2 TexCoords;\n"

                                        "uniform sampler2D Texture1;\n"

                                        "void main()\n"
                                        "{\n"
                                        "   FragColor = texture(Texture1, TexCoords);\n"
                                        "}\0";

    // vertex shader
    int vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_shader, 1, &vertex_shader_source, NULL);
    glCompileShader(vertex_shader);
    // fragment shader
    int fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 1, &fragment_shader_source, NULL);
    glCompileShader(fragment_shader);
    // link shaders
    int shader_program = glCreateProgram();
    glAttachShader(shader_program, vertex_shader);
    glAttachShader(shader_program, fragment_shader);
    glLinkProgram(shader_program);
    // delete shaders
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);


    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);
    glBindVertexArray(VAO);


    GLuint depth_rb;


    glGenFramebuffersEXT(1, &framebuffer);
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, framebuffer);
    //Attach 2D texture to this FBO
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, texture, 0);

    glGenRenderbuffersEXT(1, &depth_rb);
    glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, depth_rb);
    glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_DEPTH_COMPONENT24, 3072, 1355);

    //Attach depth buffer to FBO
    glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, depth_rb);

    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
  }
  else
  {
    glEnable(GL_BLEND);

    // Unix Domain Socket: Receive file descriptor (texture_dmabuf_fd) and texture storage data (texture_storage_metadata)
    int texture_dmabuf_fd;
    struct texture_storage_metadata_t texture_storage_metadata;

    int sock = create_socket(CLIENT_FILE);
    read_fd(sock, &texture_dmabuf_fd, &texture_storage_metadata, sizeof(texture_storage_metadata));
    close(sock);

    glGenTextures(1, &texture);

    int i = 0;
    EGLint attribute_list[64];

    attribute_list[i++] = EGL_WIDTH;
    attribute_list[i++] = 3072;
    attribute_list[i++] = EGL_HEIGHT;
    attribute_list[i++] = 1355;
    attribute_list[i++] = EGL_LINUX_DRM_FOURCC_EXT;
    attribute_list[i++] = texture_storage_metadata.fourcc;

    attribute_list[i++] = EGL_DMA_BUF_PLANE0_FD_EXT;
    attribute_list[i++] = texture_dmabuf_fd;
    attribute_list[i++] = EGL_DMA_BUF_PLANE0_PITCH_EXT;
    attribute_list[i++] = texture_storage_metadata.stride;
    attribute_list[i++] = EGL_DMA_BUF_PLANE0_OFFSET_EXT;
    attribute_list[i++] = 0;//texture_storage_metadata.offset;
#ifdef EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT
//         if (dmabuf->modifier)
    {
      attribute_list[i++] = EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT;
      attribute_list[i++] = static_cast<EGLint>(texture_storage_metadata.modifiers & 0xFFFFFFFF);
      attribute_list[i++] = EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT;
      attribute_list[i++] = static_cast<EGLint>(texture_storage_metadata.modifiers >> 32);
    }
#endif
    attribute_list[i++] = EGL_NONE;

    EGLImage image = CShareProcess::eglCreateImageKHR(m_eglDisplay,
                                                      EGL_NO_CONTEXT,
                                                      EGL_LINUX_DMA_BUF_EXT,
                                                      nullptr,
                                                      attribute_list);
    if (image == EGL_NO_IMAGE)
    {
      assert(image != EGL_NO_IMAGE);
      return 0;
    }

    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    CShareProcess::glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, image);
    CShareProcess::eglDestroyImageKHR(m_eglDisplay, image);

    close(texture_dmabuf_fd);
  }

  // -----------------------------
  // --- Texture sharing end ---
  // -----------------------------

  time_t last_time = time(NULL);
  bool swap = false;
  while (1)
  {
    // Draw scene (uses shared texture)
    // clear
    if (!is_server)
    {
      glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
      glClear(GL_COLOR_BUFFER_BIT);

      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D, texture);
      glDrawElements(GL_TRIANGLE_STRIP, 4, GL_UNSIGNED_INT, 0);

    }
    else
    {
      time_t cur_time = time(NULL);
      if (last_time < cur_time)
      {
        float vertices1[] = {
            -1.0f, 1.0f, 0.0f, 0.0f, 0.0f,   // top right
            1.0f, -1.0f, 0.0f, 1.0f, 1.0f, // bottom left
            1.0f, 1.0f, 0.0f, 1.0f, 0.0f,  // bottom right
            1.0f, -1.0f, 0.0f, 0.0f, 1.0f, // bottom left
        };
        unsigned int indices[] = {
            0, 1, 3, 2
        };

        last_time = cur_time;

        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, framebuffer);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        if (swap)
          glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 3072, 1355, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
        else
          glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 3072, 1355, GL_BGRA, GL_UNSIGNED_BYTE, buffer);
        swap = !swap;


        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices1), vertices1, GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)(3 * sizeof(float)));

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

        glDrawElements(GL_TRIANGLE_STRIP, 4, GL_UNSIGNED_INT, 0);

        glFlush();
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
        glBindTexture(GL_TEXTURE_2D, 0);
      }
    }
    eglSwapBuffers(m_eglDisplay, m_eglSurface);

    // Check for errors
    assert(glGetError() == GL_NO_ERROR);
    assert(eglGetError() == EGL_SUCCESS);
  }

  return 0;
}

void help()
{
    printf("USAGE:\n"
           "    dmabufshare server\n"
           "    dmabufshare client\n");
}

void parse_arguments(int argc, char **argv, int *is_server)
{
    if (2 == argc)
    {
        if (strcmp(argv[1], "server") == 0)
        {
            *is_server = 1;
        }
        else if (strcmp(argv[1], "client") == 0)
        {
            *is_server = 0;
        }
        else if (strcmp(argv[1], "--help") == 0)
        {
            help();
            exit(0);
        }
        else
        {
            help();
            exit(-1);
        }
    }
    else
    {
        help();
        exit(-1);
    }
}
