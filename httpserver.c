#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <sched.h>

#include "libhttp.h"
#include "wq.h"


/*
 * Global configuration variables.
 * You need to use these in your implementation of handle_files_request and
 * handle_proxy_request. Their values are set up in main() using the
 * command line arguments (already implemented for you).
 */
wq_t work_queue; // Only used by poolserver
int num_threads; // Only used by poolserver
int server_port; // Default value: 8000
char* server_files_directory;
char* server_proxy_hostname;
int server_proxy_port;


int get_fsize(char* path) {

  struct stat sb;
  if (stat(path, &sb) == -1) {
    perror("Error getting stat");
    return 0;
  }

  return sb.st_size;
}

int send_data (int fdr, int fdw, void* buff, int size) {
  int nread; int nwrite;
  
  while ((nread = read(fdr, buff, size)) > 0) {
    if ((nwrite = write(fdw, buff, nread)) != nread) {
      perror("Error writing to socket");
      return 1;
    }
  }
   
  if (nread < 0) {
    perror("Error reading file");
    return 1;
  }
  return 0;
}

int path_type(char* path) {
  
  struct stat sb;
  if (stat(path, &sb) == -1) {
    return 0;
  } else if (S_ISREG(sb.st_mode)) {
    return 1;
  } else if (S_ISDIR(sb.st_mode)) {
    return 2;
  } else {
    return 0;
  }
}

/*
 * Serves the contents the file stored at `path` to the client socket `fd`.
 * It is the caller's reponsibility to ensure that the file stored at `path` exists.
 */
void serve_file(int fd, char* path) {

  /* PART 2 BEGIN */
  int path_fd = open(path, O_RDONLY);
  if (path_fd == -1) {
    perror("Failed to open file at");
    return;
  } 
  
  int file_size = get_fsize(path);
  char cnt_len[10];
  sprintf(cnt_len, "%d", file_size);

  http_start_response(fd, 200);
  http_send_header(fd, "Content-Type", http_get_mime_type(path));
  http_send_header(fd, "Content-Length", cnt_len); // TODO: change this line too
  http_end_headers(fd);


  int size = 1024;
  char* msg = (char *) calloc(size, sizeof(char));
  int status = send_data(path_fd, fd, (void *) msg, size);
  if (status) {
    perror("Error sending data");
  }

  free(msg);
  close(path_fd);
  /* PART 2 END */
}



void serve_directory(int fd, char* path) {
  http_start_response(fd, 200);
  http_send_header(fd, "Content-Type", http_get_mime_type(".html"));
  http_end_headers(fd);
    
  DIR* dir = opendir(path);
  struct dirent* entry;
  if (dir == NULL) {
    perror("Error opening directory");
    return;
  } 
  
  char* buff = (char *) calloc(512, sizeof(char));
  if (buff == NULL) {
    perror("Could not allocate space");
    closedir(dir);
    return;
  }

  char* parent = "<a href=\"../\">Parent directory</a><br/>";
  while ((entry = readdir(dir)) != NULL) {
    if (strcmp(entry->d_name, ".") == 0) {
      continue;
    }
    
    if (strcmp(entry->d_name, "..") == 0) {
      memcpy((void *) buff, (void *) parent, strlen(parent) + 1); 
    } else {
      http_format_href(buff, path, entry->d_name);
    }
    write(fd, buff, strlen(buff));
  }
  
  free(buff);
  closedir(dir);
  /* PART 3 END */
}

/*
 * Reads an HTTP request from client socket (fd), and writes an HTTP response
 * containing:
 *
 *   1) If user requested an existing file, respond with the file
 *   2) If user requested a directory and index.html exists in the directory,
 *      send the index.html file.
 *   3) If user requested a directory and index.html doesn't exist, send a list
 *      of files in the directory with links to each.
 *   4) Send a 404 Not Found response.
 *
 *   Closes the client socket (fd) when finished.
 */

void handle_files_request(int fd) {

  struct http_request* request = http_request_parse(fd);
  
  if (request == NULL || request->path[0] != '/') {
    http_start_response(fd, 400);
    http_send_header(fd, "Content-Type", "text/html");
    http_end_headers(fd);
    close(fd);
    return;
  }

  if (strstr(request->path, "..") != NULL) {
    http_start_response(fd, 403);
    http_send_header(fd, "Content-Type", "text/html");
    http_end_headers(fd);
    close(fd);
    return;
  }

  /* Remove beginning `./` */
  char* path = malloc(2 + strlen(request->path) + 1);
  path[0] = '.';
  path[1] = '/';
  memcpy(path + 2, request->path, strlen(request->path) + 1);
  
  printf("Thread tid (%lu) is handling file request for client %d\n", pthread_self(), fd);

  /*
   * PART 2 is to serve files. If the file given by `path` exists,
   * call serve_file() on it. Else, serve a 404 Not Found error below.
   * The `stat()` syscall will be useful here.
   *
   * PART 3 is to serve both files and directories. You will need to
   * determine when to call serve_file() or serve_directory() depending
   * on `path`. Make your edits below here in this function.
   */

  /* PART 2 & 3 BEGIN */
  int type = path_type(path);
  if (type  == 0) {
    http_start_response(fd, 404);
    http_send_header(fd, "Content-Type", "text/html");
    http_end_headers(fd);
  }

  else if (type == 1) {
    serve_file(fd, path);
  }

  else if (type == 2) {
    char* index = "/index.html";
    char* path_index = (char *) calloc(strlen(path) + strlen(index) + 1, sizeof(char));
    memcpy(path_index, path, strlen(path));
    memcpy(path_index + strlen(path), index, strlen(index) + 1);
    
    if (path_type(path_index) == 1) {
      serve_file(fd, path_index);
    } else {
      free(path_index);
      serve_directory(fd, path);
    }
  } 
    /* PART 2 & 3 END */
  else {
    perror("Error checking path");
  }
  printf("Closing socket\n");
  shutdown(fd, SHUT_RDWR);
  close(fd);
  return;
}




void* handle_proxy_send (void* params) {
  int* fds = (int*) params;
  int fd1 = fds[0];
  int fd2 = fds[1];
  int rbytes; int wbytes;
  
  int size = 1024;
  char* buff[size];
  while (1) {
    rbytes = 0;
    memset(&buff, 0, size);
    while ((rbytes = read(fd1, (void *) buff, sizeof(buff))) > 0) {
      
      if ((wbytes = write(fd2, buff, rbytes) < 0)) {
        perror("Error writing to file");
        goto End;
      }

    }
    sched_yield();
  }
  
End:
  shutdown(fd1, SHUT_RDWR);
  shutdown(fd2, SHUT_RDWR);
  close(fd2);
  close(fd2);
  return NULL;
}



/*
 * Opens a connection to the proxy target (hostname=server_proxy_hostname and
 * port=server_proxy_port) and relays traffic to/from the stream fd and the
 * proxy target_fd. HTTP requests from the client (fd) should be sent to the
 * proxy target (target_fd), and HTTP responses from the proxy target (target_fd)
 * should be sent to the client (fd).
 *
 *   +--------+     +------------+     +--------------+
 *   | client | <-> | httpserver | <-> | proxy target |
 *   +--------+     +------------+     +--------------+
 *
 *   Closes client socket (fd) and proxy target fd (target_fd) when finished.
 */
void handle_proxy_request(int fd) {

  /*
  * The code below does a DNS lookup of server_proxy_hostname and
  * opens a connection to it. Please do not modify.
  */
  struct sockaddr_in target_address;
  memset(&target_address, 0, sizeof(target_address));
  target_address.sin_family = AF_INET;
  target_address.sin_port = htons(server_proxy_port);

  // Use DNS to resolve the proxy target's IP address
  struct hostent* target_dns_entry = gethostbyname2(server_proxy_hostname, AF_INET);

  // Create an IPv4 TCP socket to communicate with the proxy target.
  int target_fd = socket(PF_INET, SOCK_STREAM, 0);
  if (target_fd == -1) {
    fprintf(stderr, "Failed to create a new socket: error %d: %s\n", errno, strerror(errno));
    close(fd);
    exit(errno);
  }

  if (target_dns_entry == NULL) {
    fprintf(stderr, "Cannot find host: %s\n", server_proxy_hostname);
    close(target_fd);
    close(fd);
    exit(ENXIO);
  }

  char* dns_address = target_dns_entry->h_addr_list[0];

  // Connect to the proxy target.
  memcpy(&target_address.sin_addr, dns_address, sizeof(target_address.sin_addr));
  int connection_status =
      connect(target_fd, (struct sockaddr*)&target_address, sizeof(target_address));

  if (connection_status < 0) {
    /* Dummy request parsing, just to be compliant. */
    http_request_parse(fd);

    http_start_response(fd, 502);
    http_send_header(fd, "Content-Type", "text/html");
    http_end_headers(fd);
    close(target_fd);
    close(fd);
    return;
  }

  /* PART 4 BEGIN */
  pthread_t tidA; pthread_t tidB;
  
  int paramsA[2]; int paramsB[2];
  paramsA[0] = fd;
  paramsA[1] = target_fd;
  paramsB[0] = target_fd;
  paramsB[1] = fd;

  int error = pthread_create(&tidA, NULL, &handle_proxy_send, (void *) paramsA);
  if (error) {
    perror("Failed to create thread A");
    close(target_fd);
    close(fd);
    return;
  } 

  error = pthread_create(&tidB, NULL, &handle_proxy_send, (void *) paramsB);
  if (error) {
    perror("Failed to create thread B");
    close(target_fd);
    close(fd);
    return;
  }

  /* PART 4 END */
}



#ifdef POOLSERVER
/*
 * All worker threads will run this function until the server shutsdown.
 * Each thread should block until a new request has been received.
 * When the server accepts a new connection, a thread should be dispatched
 * to send a response to the client.
 */
void* handle_clients(void* void_request_handler) {
  void (*request_handler)(int) = (void (*)(int))void_request_handler;
  /* (Valgrind) Detach so thread frees its memory on completion, since we won't
   * be joining on it. */
  pthread_detach(pthread_self());

  /* PART 7 BEGIN */
  while (1) {
    int fd = wq_pop(&work_queue);
    request_handler(fd);
    printf("Client %d request is complete\n\n", fd);
  }
  return NULL;
  /* PART 7 END */
}

/*
 * Creates `num_threads` amount of threads. Initializes the work queue.
 */
void init_thread_pool(int num_threads, void (*request_handler)(int)) {

  /* PART 7 BEGIN */
  wq_init(&work_queue);
  for (int i = 0; i < num_threads; i++) {
    pthread_t tid;
    int error = pthread_create(&tid, NULL, &handle_clients, request_handler);
    if (error) {
      perror("Failed to create thread");
    }
    printf("Created thread (%lu)\n", tid);
  }
  printf("\n");
  /* PART 7 END */
}
#endif

/*
 * Opens a TCP stream socket on all interfaces with port number PORTNO. Saves
 * the fd number of the server socket in *socket_number. For each accepted
 * connection, calls request_handler with the accepted fd number.
 */
void serve_forever(int* socket_number, void (*request_handler)(int)) {

  struct sockaddr_in server_address, client_address;
  size_t client_address_length = sizeof(client_address);
  int client_socket_number;

  // Creates a socket for IPv4 and TCP.
  *socket_number = socket(PF_INET, SOCK_STREAM, 0);
  if (*socket_number == -1) {
    perror("Failed to create a new socket");
    exit(errno);
  }

  int socket_option = 1;
  if (setsockopt(*socket_number, SOL_SOCKET, SO_REUSEADDR, &socket_option, sizeof(socket_option)) ==
      -1) {
    perror("Failed to set socket options");
    exit(errno);
  }

  // Setup arguments for bind()
  memset(&server_address, 0, sizeof(server_address));
  server_address.sin_family = AF_INET;
  server_address.sin_addr.s_addr = INADDR_ANY;
  server_address.sin_port = htons(server_port);

  /*
   * PART 1
   *
   * Given the socket created above, call bind() to give it
   * an address and a port. Then, call listen() with the socket.
   * An appropriate size of the backlog is 1024, though you may
   * play around with this value during performance testing.
   */

  /* PART 1 BEGIN */
  int status = bind(*socket_number, (struct sockaddr *) &server_address, sizeof(server_address));
  if (status < 0) {
    perror("Failed to bind to port");
    exit(errno);
  }
  
  status = listen(*socket_number, 1024);
  if (status < 0) {
    perror("Failed to listen to new connections");
    exit(errno);
  }

  /* PART 1 END */
  printf("Listening on port %d...\n", server_port);

#ifdef POOLSERVER
  /*
   * The thread pool is initialized rbefore* the server
   * begins accepting client connections.
   */
  printf("Initializing thread pool\n\n");
  init_thread_pool(num_threads, request_handler);

#endif

  while (1) {
    client_socket_number = accept(*socket_number, (struct sockaddr*)&client_address,
                                  (socklen_t*)&client_address_length);
    if (client_socket_number < 0) {
      perror("Error accepting socket");
      continue;
    }

    printf("Accepted connection from %s on port %d\n", inet_ntoa(client_address.sin_addr),
           client_address.sin_port);

#ifdef BASICSERVER
    /*
     * This is a single-process, single-threaded HTTP server.
     * When a client connection has been accepted, the main
     * process sends a response to the client. During this
     * time, the server does not listen and accept connections.
     * Only after a response has been sent to the client can
     * the server accept a new connection.
     */
    request_handler(client_socket_number);

#elif POOLSERVER
    /*
     * PART 7
     *
     * When a client connection has been accepted, add the
     * client's socket number to the work queue. A thread
     * in the thread pool will send a response to the client.
     */

    /* PART 7 BEGIN */
    wq_push(&work_queue, client_socket_number);

    /* PART 7 END */
#endif
  }

  shutdown(*socket_number, SHUT_RDWR);
  close(*socket_number);
}

int server_fd;
void signal_callback_handler(int signum) {
  printf("Caught signal %d: %s\n", signum, strsignal(signum));
  printf("Closing socket %d\n", server_fd);
  if (close(server_fd) < 0)
    perror("Failed to close server_fd (ignoring)\n");
  exit(0);
}

char* USAGE =
    "Usage: ./httpserver --files some_directory/ [--port 8000 --num-threads 5]\n"
    "       ./httpserver --proxy inst.eecs.berkeley.edu:80 [--port 8000 --num-threads 5]\n";

void exit_with_usage() {
  fprintf(stderr, "%s", USAGE);
  exit(EXIT_SUCCESS);
}

int main(int argc, char** argv) {
  signal(SIGINT, signal_callback_handler);
  signal(SIGPIPE, SIG_IGN);

  /* Default settings */
  server_port = 8000;
  void (*request_handler)(int) = NULL;

  int i;
  for (i = 1; i < argc; i++) {
    if (strcmp("--files", argv[i]) == 0) {
      request_handler = handle_files_request;
      server_files_directory = argv[++i];
      if (!server_files_directory) {
        fprintf(stderr, "Expected argument after --files\n");
        exit_with_usage();
      }
    } else if (strcmp("--proxy", argv[i]) == 0) {
      request_handler = handle_proxy_request;

      char* proxy_target = argv[++i];
      if (!proxy_target) {
        fprintf(stderr, "Expected argument after --proxy\n");
        exit_with_usage();
      }

      char* colon_pointer = strchr(proxy_target, ':');
      if (colon_pointer != NULL) {
        *colon_pointer = '\0';
        server_proxy_hostname = proxy_target;
        server_proxy_port = atoi(colon_pointer + 1);
      } else {
        server_proxy_hostname = proxy_target;
        server_proxy_port = 80;
      }
    } else if (strcmp("--port", argv[i]) == 0) {
      char* server_port_string = argv[++i];
      if (!server_port_string) {
        fprintf(stderr, "Expected argument after --port\n");
        exit_with_usage();
      }
      server_port = atoi(server_port_string);
    } else if (strcmp("--num-threads", argv[i]) == 0) {
      char* num_threads_str = argv[++i];
      if (!num_threads_str || (num_threads = atoi(num_threads_str)) < 1) {
        fprintf(stderr, "Expected positive integer after --num-threads\n");
        exit_with_usage();
      }
    } else if (strcmp("--help", argv[i]) == 0) {
      exit_with_usage();
    } else {
      fprintf(stderr, "Unrecognized option: %s\n", argv[i]);
      exit_with_usage();
    }
  }

  if (server_files_directory == NULL && server_proxy_hostname == NULL) {
    fprintf(stderr, "Please specify either \"--files [DIRECTORY]\" or \n"
                    "                      \"--proxy [HOSTNAME:PORT]\"\n");
    exit_with_usage();
  }

#ifdef POOLSERVER
  if (num_threads < 1) {
    fprintf(stderr, "Please specify \"--num-threads [N]\"\n");
    exit_with_usage();
  }
#endif

  chdir(server_files_directory);
  serve_forever(&server_fd, request_handler);

  return EXIT_SUCCESS;
}
