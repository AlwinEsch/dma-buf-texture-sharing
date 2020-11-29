all: dmabufshare-server dmabufshare-client

dmabufshare-client: main-client.cpp OpenGLExtensionCheck.cpp ViewRendererGL.cpp ViewRendererGL.h socket.h
	$(CXX) main-client.cpp OpenGLExtensionCheck.cpp ViewRendererGL.cpp -g -lEGL -lGL -lX11 -o dmabufshare-client

dmabufshare-server: main-server.cpp OpenGLExtensionCheck.cpp WorkRendererGL.cpp WorkRendererGL.h socket.h
	$(CXX) main-server.cpp OpenGLExtensionCheck.cpp WorkRendererGL.cpp -g -lEGL -lGL -lpthread -o dmabufshare-server


clean:
	rm -f dmabufshare-server dmabufshare-client
