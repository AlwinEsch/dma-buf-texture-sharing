all: dmabufshare

dmabufshare: main.cpp socket.h window.h render.h
	$(CXX) main.cpp -g -lEGL -lGL -lX11 -o dmabufshare

clean:
	rm -f dmabufshare
