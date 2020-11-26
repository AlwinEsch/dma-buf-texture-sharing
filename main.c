
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

void parse_arguments(int argc, char **argv, int *is_server);
void rotate_data(int data[4]);

int main(int argc, char **argv)
{
    // Parse arguments
    int is_server;
    parse_arguments(argc, argv, &is_server);

    // Create X11 window
    Display *x11_display;
    Window x11_window;
    create_x11_window(is_server, &x11_display, &x11_window);

    // Initialize EGL
    EGLDisplay egl_display;
    EGLContext egl_context;
    EGLSurface egl_surface;
    initialize_egl(x11_display, x11_window, &egl_display, &egl_context, &egl_surface);

    // Setup GL scene
    gl_setup_scene();

    // Server texture data { red, greee, blue, white }
//     int texture_data[4] = {0x000000FF, 0x0000FF00, 0X00FF0000, 0x00FFFFFF};
    FILE *pFile = fopen("./test.data" , "rb");
    if (pFile==NULL)
    {
      fputs ("File error",stderr);
      exit (1);
    }

    fseek(pFile , 0 , SEEK_END);
    size_t lSize = ftell(pFile);
    rewind(pFile);
    char *buffer = (char*)malloc(sizeof(char)*lSize);
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
    };

    // GL texture that will be shared
    GLuint texture;

    // The next `if` block contains server code in the `true` branch and client code in the `false` branch. The `true` branch is always executed first and the `false` branch after it (in a different process). This is because the server loops at the end of the branch until it can send a message to the client and the client blocks at the start of the branch until it has a message to read. This way the whole `if` block from top to bottom represents the order of events as they happen.
    if (is_server)
    {
        // GL: Create and populate the texture
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 3072, 1355, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 3072, 1355, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        // EGL: Create EGL image from the GL texture
        EGLImage image = eglCreateImage(egl_display,
                                        egl_context,
                                        EGL_GL_TEXTURE_2D,
                                        (EGLClientBuffer)(uint64_t)texture,
                                        NULL);
        assert(image != EGL_NO_IMAGE);
        
        // The next line works around an issue in radeonsi driver (fixed in master at the time of writing). If you are
        // not having problems with texture rendering until the first texture update you can omit this line
        glFlush();

        // EGL (extension: EGL_MESA_image_dma_buf_export): Get file descriptor (texture_dmabuf_fd) for the EGL image and get its
        // storage data (texture_storage_metadata - fourcc, stride, offset)
        int texture_dmabuf_fd;
        struct texture_storage_metadata_t texture_storage_metadata;

        PFNEGLEXPORTDMABUFIMAGEQUERYMESAPROC eglExportDMABUFImageQueryMESA =
            (PFNEGLEXPORTDMABUFIMAGEQUERYMESAPROC)eglGetProcAddress("eglExportDMABUFImageQueryMESA");
        EGLBoolean queried = eglExportDMABUFImageQueryMESA(egl_display,
                                                           image,
                                                           &texture_storage_metadata.fourcc,
                                                           NULL,
                                                           NULL);
        assert(queried);
        PFNEGLEXPORTDMABUFIMAGEMESAPROC eglExportDMABUFImageMESA =
            (PFNEGLEXPORTDMABUFIMAGEMESAPROC)eglGetProcAddress("eglExportDMABUFImageMESA");
        EGLBoolean exported = eglExportDMABUFImageMESA(egl_display,
                                                       image,
                                                       &texture_dmabuf_fd,
                                                       &texture_storage_metadata.stride,
                                                       &texture_storage_metadata.offset);
        assert(exported);

        // Unix Domain Socket: Send file descriptor (texture_dmabuf_fd) and texture storage data (texture_storage_metadata)
        int sock = create_socket(SERVER_FILE);
        while (connect_socket(sock, CLIENT_FILE) != 0)
            ;
        write_fd(sock, texture_dmabuf_fd, &texture_storage_metadata, sizeof(texture_storage_metadata));
        close(sock);
        close(texture_dmabuf_fd);
    }
    else
    {
        // Unix Domain Socket: Receive file descriptor (texture_dmabuf_fd) and texture storage data (texture_storage_metadata)
        int texture_dmabuf_fd;
        struct texture_storage_metadata_t texture_storage_metadata;

        int sock = create_socket(CLIENT_FILE);
        read_fd(sock, &texture_dmabuf_fd, &texture_storage_metadata, sizeof(texture_storage_metadata));
        close(sock);

        // EGL (extension: EGL_EXT_image_dma_buf_import): Create EGL image from file descriptor (texture_dmabuf_fd) and storage
        // data (texture_storage_metadata)
        EGLAttrib const attribute_list[] = {
            EGL_WIDTH, 3072,
            EGL_HEIGHT, 1355,
            EGL_LINUX_DRM_FOURCC_EXT, texture_storage_metadata.fourcc,
            EGL_DMA_BUF_PLANE0_FD_EXT, texture_dmabuf_fd,
            EGL_DMA_BUF_PLANE0_OFFSET_EXT, texture_storage_metadata.offset,
            EGL_DMA_BUF_PLANE0_PITCH_EXT, texture_storage_metadata.stride,
            EGL_NONE};
        EGLImage image = eglCreateImage(egl_display,
                                        NULL,
                                        EGL_LINUX_DMA_BUF_EXT,
                                        (EGLClientBuffer)NULL,
                                        attribute_list);
        assert(image != EGL_NO_IMAGE);
        close(texture_dmabuf_fd);

        // GLES (extension: GL_OES_EGL_image_external): Create GL texture from EGL image
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, image);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }

    // -----------------------------
    // --- Texture sharing end ---
    // -----------------------------

    time_t last_time = time(NULL);
    while (1)
    {
        // Draw scene (uses shared texture)
        gl_draw_scene(texture);
        eglSwapBuffers(egl_display, egl_surface);

//         // Update texture data each second to see that the client didn't just copy the texture and is indeed referencing
//         // the same texture data.
//         if (is_server)
//         {
//             time_t cur_time = time(NULL);
//             if (last_time < cur_time)
//             {
//                 last_time = cur_time;
//                 rotate_data(texture_data);
//                 glBindTexture(GL_TEXTURE_2D, texture);
//                 glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 3072, 1355, GL_RGBA, GL_UNSIGNED_BYTE, texture_data);
//             }
//         }

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

void rotate_data(int data[4])
{
    int temp = data[0];
    data[0] = data[1];
    data[1] = data[3];
    data[3] = data[2];
    data[2] = temp;
}
