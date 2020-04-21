all: dmabufshare

dmabufshare: main.c socket.h window.h render.h
	$(CC) main.c -g -lEGL -lGL -lX11 -o dmabufshare

clean:
	rm -f server
	rm -f client