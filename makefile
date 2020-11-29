all: dmabufshare-server dmabufshare-client

dmabufshare-client: main-client.cpp OpenGLExtensionCheck.cpp RendererGL.cpp RendererGL.h socket.h
	$(CXX) main-client.cpp OpenGLExtensionCheck.cpp RendererGL.cpp -g -lEGL -lGL -lX11 -o dmabufshare-client

dmabufshare-server: main-server.cpp OpenGLExtensionCheck.cpp socket.h render.h
	$(CXX) main-server.cpp OpenGLExtensionCheck.cpp -g -lEGL -lGL -lX11 -o dmabufshare-server


clean:
	rm -f dmabufshare-server dmabufshare-client
