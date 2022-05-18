#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <netdb.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <inttypes.h>  // for SCNd32, PRId32
#include "msg.h"
#define BUF 256
#define MAX_NAME_LENGTH 128
#define DEBUG 0

void Usage(char *progname);

int LookupName(char *name,
                unsigned short port,
                struct sockaddr_storage *ret_addr,
                size_t *ret_addrlen);

int Connect(const struct sockaddr_storage *addr,
             const size_t addrlen,
             int *ret_fd);

int 
main(int argc, char **argv) {
    if (argc != 3) {
        Usage(argv[0]);
    }

    unsigned short port = 0;
    if (sscanf(argv[2], "%hu", &port) != 1) {
        Usage(argv[0]);
    }

    // Get an appropriate sockaddr structure.
    struct sockaddr_storage addr;
    size_t addrlen;
    if (!LookupName(argv[1], port, &addr, &addrlen)) {
        Usage(argv[0]);
    }

    // Connect to the remote host.
    int socket_fd;
    if (!Connect(&addr, addrlen, &socket_fd)) {
        Usage(argv[0]);
    }
    
    int8_t choice, flag;
    flag = 1;
    while (flag){
        struct record inp;
        // pack input record for client msg
        printf("Enter your choice (1 to put, 2 to get, 3 to delete, 0 to quit): ");
        while (scanf("%" SCNd8 "%*c", &choice) != 1)
        {
            while (getchar() != '\n'); // clear input
            printf(" Enter your choice (1 to put, 2 to get, 3 to delete, 0 to quit): ");
        }
        switch (choice){
            case 1: // PUT
                printf("Enter the name: ");
                fgets(inp.name, MAX_NAME_LENGTH, stdin);
                inp.name[strlen(inp.name) - 1] = '\0';
                
                /*printf("Enter the id: ");
                scanf("%d", &inp.id);*/
                //break;
            case 2: case 3: // PUT, GET, DELETE
                printf("Enter the id: ");
                while (scanf("%d", &(inp.id)) != 1)
                {
                    while (getchar() != '\n'); // clear input
                    printf(" Enter the id (one integer): ");
                }
                break;
            default:
                close(socket_fd);
                return EXIT_SUCCESS;
                //flag = 0;
                break;
        }
        //if (flag == 0) {break;}
        struct msg client;
        client.type = choice;
        client.rd = inp;
        // send client msg
        while (1) {
            int wres = write(socket_fd, &client, sizeof(struct msg));
            if (wres == 0)
            {
                printf("socket to server closed prematurely \n");
                close(socket_fd);
                return EXIT_FAILURE;
            }
            if (wres == -1)
            {
                if (errno == EINTR)
                    continue;
                printf("socket to server write failure \n");
                close(socket_fd);
                return EXIT_FAILURE;
            }
            break;
        }
        
        struct msg server; server.type = 0;
        // get server msg
        while (1) {
            int rres = read(socket_fd, &server, sizeof(struct msg));
            // char clientbuf[1024]; //ssize_t rres = read(c_fd, clientbuf, 1023);
            if (rres == 0) {
                printf("read on socket to server closed prematurely \n");
                break;
                /* client
                printf("socket closed prematurely \n");
                close(socket_fd);
                return EXIT_FAILURE;*/
            }
            if (rres == -1)
            {
                if ((errno == EAGAIN) || (errno == EINTR))
                    continue;
                printf("socket to server read failure: %s \n", strerror(errno));
                /* client
                if (errno == EINTR)
                    continue;
                printf("socket read failure \n");
                close(socket_fd);
                return EXIT_FAILURE;*/
            }
            break;
        }
        
        // unpack server msg //fprintf(stderr, "server.type: %d\n", server.type);
        switch(server.type) {
            case 1:
                printf("Put success.\n");
                break;
            case 2:
                printf("name: %s\n", server.rd.name);
                printf("id: %d\n", server.rd.id);
                break;
            case 3:
                printf("Delete success.\n");
                break;
            case 4:
                printf("Put failed\n");
                break;
            case 5:
                printf("Get failed\n");
                break;
            case 6:
                printf("Delete failed\n");
                break;
            default:
                break;
        }
    }
    
    // Clean up.
    close(socket_fd);
    return EXIT_SUCCESS;
}

void 
Usage(char *progname) {
    printf("usage: %s    hostname port \n", progname);
    exit(EXIT_FAILURE);
}

int 
LookupName(char *name, unsigned short port,
            struct sockaddr_storage *ret_addr,
            size_t *ret_addrlen) {
    struct addrinfo hints, *results;
    int retval;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    // Do the lookup by invoking getaddrinfo().
    if ((retval = getaddrinfo(name, NULL, &hints, &results)) != 0) {
        printf( "getaddrinfo failed: %s", gai_strerror(retval));
        return 0;
    }

    // Set the port in the first result.
    if (results->ai_family == AF_INET) {
        struct sockaddr_in *v4addr = (struct sockaddr_in *) (results->ai_addr);
        v4addr->sin_port = htons(port);
    } else if (results->ai_family == AF_INET6) {
        struct sockaddr_in6 *v6addr = (struct sockaddr_in6 *)(results->ai_addr);
        v6addr->sin6_port = htons(port);
    } else {
        printf("getaddrinfo failed to provide an IPv4 or IPv6 address \n");
        freeaddrinfo(results);
        return 0;
    }

    // Return the first result.
    assert(results != NULL);
    memcpy(ret_addr, results->ai_addr, results->ai_addrlen);
    *ret_addrlen = results->ai_addrlen;

    // Clean up.
    freeaddrinfo(results);
    return 1;
}

int 
Connect(const struct sockaddr_storage *addr, const size_t addrlen, int *ret_fd) {
    // Create the socket.
    int socket_fd = socket(addr->ss_family, SOCK_STREAM, 0);
    if (socket_fd == -1) {
        printf("socket() failed: %s\n", strerror(errno));
        return 0;
    }

    // Connect the socket to the remote host.
    int res = connect(socket_fd,
                                        (const struct sockaddr *)(addr),
                                        addrlen);
    if (res == -1) {
        printf("connect() failed: %s\n", strerror(errno));
        return 0;
    }

    *ret_fd = socket_fd;
    return 1;
}
