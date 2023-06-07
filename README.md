# Concurrent Web Proxy for HTTP Only Sites
In order to enhance the performance and efficiency of a web proxy, it is essential to implement concurrency, allowing the proxy to handle multiple requests concurrently instead of processing them sequentially. This concurrency feature becomes particularly important in real-world scenarios where the proxy needs to handle a high volume of incoming connection requests.

To achieve concurrency, there are two approaches that can be employed:

1. Process-based approach using file locking for synchronization: By utilizing this approach, a new process is created to handle each incoming connection request. This enables multiple requests to be processed simultaneously and independently. To ensure synchronization and prevent conflicts, file locking mechanisms are used to coordinate access to shared resources.

2. Thread-based approach using mutexes for synchronization: This approach involves creating a new thread for each incoming connection request. Threads allow for lightweight concurrent execution within a single process. To maintain synchronization and avoid race conditions, mutexes (mutual exclusion locks) are employed to protect critical sections of code that access shared resources.

The purpose of implementing a concurrent web proxy is to significantly improve its performance and scalability. By enabling the proxy to handle multiple requests concurrently, it can effectively manage a larger workload, serve clients more efficiently, and reduce response times. Concurrent processing ensures that the proxy can keep up with the demands of modern web applications and deliver a smooth and responsive user experience.

# Testing this Concurrent Web Proxy
You can download this repository to your local computer.
In order to make the executable, simply run "make" in the terminal.
To test the multi-process version of the concurrent web proxy, enter "./proxy_process <port number>" in the terminal.
To test the threaded version of the concurrent web proxy, enter "./proxy_thread <port number>" in the terminal.
To recompile the program, simply run "make clean" in the terminal.
To stop the multi-process or threaded version of the concurrent web proxy from running, press Ctrl + C in the terminal.

Below I mentioned a list of HTTP only websites for which I have tested both versions of my concurrent web proxy with. Some websites take more 
time to load than others.
1. http://astoundingshiningbeautifuleclipse.neverssl.com/online/
2. http://httpforever.com/
3. http://eu.httpbin.org/
4. http://example.com/
5. http://www.washington.edu/
