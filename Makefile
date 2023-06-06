proxy:
	gcc $(CFLAGS) proxy_process.c csapp.c -o proxy_process 
	gcc $(CFLAGS) proxy_thread.c csapp.c -o proxy_thread -pthread
clean:
	rm -f *.o proxy_process proxy_thread
