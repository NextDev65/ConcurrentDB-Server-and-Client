#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "msg.h"
//common_threads.h
#include <assert.h>
#include <pthread.h>
#define RCD_SIZE 512
#define DEBUG 0

void Usage(char *progname);
void PrintOut(int fd, struct sockaddr *addr, size_t addrlen);
void PrintReverseDNS(struct sockaddr *addr, size_t addrlen);
void PrintServerSide(int client_fd, int sock_family);
int  Listen(char *portnum, int *sock_family);
//void HandleClient(int c_fd, struct sockaddr *addr, size_t addrlen, int sock_family);
void HandleClient(void *arg);

//common_threads.h
void Pthread_create(pthread_t *t, const pthread_attr_t *attr,  
		    void *(*start_routine)(void *), void *arg) {
    int rc = pthread_create(t, attr, start_routine, arg);
    assert(rc == 0);
}

struct client_info
{
    int c_fd;
    struct sockaddr *addr;
    size_t addrlen;
    int sock_family;
};

int 
main(int argc, char **argv) {
    // Expect the port number as a command line argument.
    if (argc != 2) {
        Usage(argv[0]);
    }

    int sock_family;
    int listen_fd = Listen(argv[1], &sock_family);
    if (listen_fd <= 0) {
        // We failed to bind/listen to a socket.    Quit with failure.
        printf("Couldn't bind to any addresses.\n");
        return EXIT_FAILURE;
    }

    // create file if not exist
    FILE *fp;
    fp = fopen("./p3db.txt", "a");
    fclose(fp);

    struct client_info c_inf;
    // Loop forever, accepting a connection from a client and doing
    // an echo trick to it.
    while (1) {
        struct sockaddr_storage caddr;
        socklen_t caddr_len = sizeof(caddr);
        int client_fd = accept(listen_fd, (struct sockaddr *)(&caddr), &caddr_len);
        if (client_fd < 0) {
            if ((errno == EINTR) || (errno == EAGAIN) || (errno == EWOULDBLOCK))
                continue;
            printf("Failure on accept:%s \n ", strerror(errno));
            break;
        }
        
        //HandleClient(client_fd, (struct sockaddr *)(&caddr), caddr_len, sock_family);
        c_inf.c_fd = client_fd;
        c_inf.addr = (struct sockaddr *)(&caddr);
        c_inf.addrlen = caddr_len;
        c_inf.sock_family = sock_family;
        
        pthread_t pt;
        Pthread_create(&pt, NULL, (void *)HandleClient, &c_inf);
    }

    // Close socket
    close(listen_fd);
    return EXIT_SUCCESS;
}

void Usage(char *progname) {
    printf("usage: %s port \n", progname);
    exit(EXIT_FAILURE);
}

void 
PrintOut(int fd, struct sockaddr *addr, size_t addrlen) {
    printf("Socket [%d] is bound to: \n", fd);
    if (addr->sa_family == AF_INET) {
        // Print out the IPV4 address and port

        char astring[INET_ADDRSTRLEN];
        struct sockaddr_in *in4 = (struct sockaddr_in *)(addr);
        inet_ntop(AF_INET, &(in4->sin_addr), astring, INET_ADDRSTRLEN);
        printf(" IPv4 address %s", astring);
        printf(" and port %d\n", ntohs(in4->sin_port));

    } else if (addr->sa_family == AF_INET6) {
        // Print out the IPV6 address and port

        char astring[INET6_ADDRSTRLEN];
        struct sockaddr_in6 *in6 = (struct sockaddr_in6 *)(addr);
        inet_ntop(AF_INET6, &(in6->sin6_addr), astring, INET6_ADDRSTRLEN);
        printf("IPv6 address %s", astring);
        printf(" and port %d\n", ntohs(in6->sin6_port));

    } else {
        printf(" ???? address and port ???? \n");
    }
}

void 
PrintReverseDNS(struct sockaddr *addr, size_t addrlen) {
    char hostname[1024];    // ought to be big enough.
    if (getnameinfo(addr, addrlen, hostname, 1024, NULL, 0, 0) != 0) {
        sprintf(hostname, "[reverse DNS failed]");
    }
    printf("DNS name: %s \n", hostname);
}

void 
PrintServerSide(int client_fd, int sock_family) {
    char hname[1024];
    hname[0] = '\0';

    printf("Server side interface is ");
    if (sock_family == AF_INET) {
        // The server is using an IPv4 address.
        struct sockaddr_in srvr;
        socklen_t srvrlen = sizeof(srvr);
        char addrbuf[INET_ADDRSTRLEN];
        getsockname(client_fd, (struct sockaddr *) &srvr, &srvrlen);
        inet_ntop(AF_INET, &srvr.sin_addr, addrbuf, INET_ADDRSTRLEN);
        printf("%s", addrbuf);
        // Get the server's dns name, or return it's IP address as
        // a substitute if the dns lookup fails.
        getnameinfo((const struct sockaddr *) &srvr,
                                srvrlen, hname, 1024, NULL, 0, 0);
        printf(" [%s]\n", hname);
    } else {
        // The server is using an IPv6 address.
        struct sockaddr_in6 srvr;
        socklen_t srvrlen = sizeof(srvr);
        char addrbuf[INET6_ADDRSTRLEN];
        getsockname(client_fd, (struct sockaddr *) &srvr, &srvrlen);
        inet_ntop(AF_INET6, &srvr.sin6_addr, addrbuf, INET6_ADDRSTRLEN);
        printf("%s", addrbuf);
        // Get the server's dns name, or return it's IP address as
        // a substitute if the dns lookup fails.
        getnameinfo((const struct sockaddr *) &srvr,
                                srvrlen, hname, 1024, NULL, 0, 0);
        printf(" [%s]\n", hname);
    }
}

int 
Listen(char *portnum, int *sock_family) {

    // Populate the "hints" addrinfo structure for getaddrinfo().
    // ("man addrinfo")
    struct addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;             // IPv6 (also handles IPv4 clients)
    hints.ai_socktype = SOCK_STREAM;    // stream
    hints.ai_flags = AI_PASSIVE;            // use wildcard "in6addr_any" address
    hints.ai_flags |= AI_V4MAPPED;        // use v4-mapped v6 if no v6 found
    hints.ai_protocol = IPPROTO_TCP;    // tcp protocol
    hints.ai_canonname = NULL;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;

    // Use argv[1] as the string representation of our portnumber to
    // pass in to getaddrinfo().    getaddrinfo() returns a list of
    // address structures via the output parameter "result".
    struct addrinfo *result;
    int res = getaddrinfo(NULL, portnum, &hints, &result);

    // Did addrinfo() fail?
    if (res != 0) {
	printf( "getaddrinfo failed: %s", gai_strerror(res));
        return -1;
    }

    // Loop through the returned address structures until we are able
    // to create a socket and bind to one.    The address structures are
    // linked in a list through the "ai_next" field of result.
    int listen_fd = -1;
    struct addrinfo *rp;
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        listen_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (listen_fd == -1) {
            // Creating this socket failed.    So, loop to the next returned
            // result and try again.
            printf("socket() failed:%s \n ", strerror(errno));
            listen_fd = -1;
            continue;
        }

        // Configure the socket; we're setting a socket "option."    In
        // particular, we set "SO_REUSEADDR", which tells the TCP stack
        // so make the port we bind to available again as soon as we
        // exit, rather than waiting for a few tens of seconds to recycle it.
        int optval = 1;
        setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

        // Try binding the socket to the address and port number returned
        // by getaddrinfo().
        if (bind(listen_fd, rp->ai_addr, rp->ai_addrlen) == 0) {
            // Bind worked!    Print out the information about what
            // we bound to.
            if (DEBUG) {PrintOut(listen_fd, rp->ai_addr, rp->ai_addrlen);}

            // Return to the caller the address family.
            *sock_family = rp->ai_family;
            break;
        }

        // The bind failed.    Close the socket, then loop back around and
        // try the next address/port returned by getaddrinfo().
        close(listen_fd);
        listen_fd = -1;
    }

    // Free the structure returned by getaddrinfo().
    freeaddrinfo(result);

    // If we failed to bind, return failure.
    if (listen_fd == -1)
        return listen_fd;

    // Success. Tell the OS that we want this to be a listening socket.
    if (listen(listen_fd, SOMAXCONN) != 0) {
        printf("Failed to mark socket as listening:%s \n ", strerror(errno));
        close(listen_fd);
        return -1;
    }

    // Return to the client the listening file descriptor.
    return listen_fd;
}

void 
HandleClient(void* arg) {
    // unpack arg struct
    struct client_info *c_inf = (struct client_info *) arg;
    int c_fd = c_inf->c_fd;
    if (DEBUG) {
        struct sockaddr *addr = c_inf->addr;
        size_t addrlen = c_inf->addrlen;
        int sock_family = c_inf->sock_family;
        
        // Print out information about the client.
        printf("\nNew client connection \n" );
        PrintOut(c_fd, addr, addrlen);
        PrintReverseDNS(addr, addrlen);
        PrintServerSide(c_fd, sock_family);
    }

    // Loop, reading data and echo'ing it back, until the client
    // closes the connection.
    while (1) {
        struct msg client;
        // get client msg
        while (1) {
            ssize_t rres = read(c_fd, &client, sizeof(struct msg));
            // char clientbuf[1024]; //ssize_t rres = read(c_fd, clientbuf, 1023);
            if (rres == 0) {
                printf("[The client disconnected.] \n"); //printf("socket closed prematurely \n");
                goto close_socket;
                //break;
            }
            if (rres == -1)
            {
                if ((errno == EAGAIN) || (errno == EINTR))
                    continue;
                printf(" Error on client socket:%s \n ", strerror(errno));//printf("socket read failure \n");
                goto close_socket;
                //break;
            }
            break;
        }
        
        // pack server msg && write record
        struct msg server;
        server.type = client.type;
        //server.rd = NULL;
        size_t fres = -1;
        //char first_char[512];
        FILE *fp;
        switch (client.type) {
            case 1: // GET
                // fopen: read + write
                fp = fopen("./p3db.txt", "r+");
                if (fp == NULL) {server.type = 4; break;};
                // overwrite null record (as per prof inst) // ideal: overwrite ((null OR matching id)) record
                while (fres != 0)
                {
                    // pull record
                    fres = fread(&(server.rd), RCD_SIZE, 1, fp);
                    // null (as per prof inst) // ideal: null OR id match
                    if ((server.rd.id == -1 && *(server.rd.name) == '\0')) // (as per prof inst) // ideal: || (server.rd.id == client.rd.id))
                    {
                        // shift pointer back one record
                        if (fseek(fp, -512, SEEK_CUR) == -1) {server.type = 4; break;};
                        // overwrite
                        if (fwrite(&(client.rd), RCD_SIZE, 1, fp) == 0) {server.type = 4; break;};
                        break;
                    }
                    // id match -> FAIL? if (server.rd.id == client.rd.id) {server.type = 4;}
                }
                // else append
                if (fres == 0) {
                    // fopen: append, write record
                    //fclose(fp);
                    //fp = fopen("./p3db.txt", "a");
                    //if (fp == NULL) {server.type = 4; break;};
                    
                    // goto EOF, write record
                    if (fseek(fp, 0, SEEK_END) == -1) {server.type = 4; break;};
                    if (fwrite(&(client.rd), RCD_SIZE, 1, fp) == 0) {server.type = 4; break;};
                }
                // put has no fail case
                break;
            case 2: // PUT
                // fopen: read
                fp = fopen("./p3db.txt", "r");
                if (fp == NULL) {server.type = 5; break;};
                // pull record with matching id
                while (fres != 0)
                {
                    // pull record
                    fres = fread(&(server.rd), RCD_SIZE, 1, fp);
                    // id match
                    if (server.rd.id == client.rd.id) {break;}
                }
                // else FAIL
                if (fres == 0) {server.type = 5;}
                break;
            case 3: // DELETE
                // fopen: read + write
                fp = fopen("./p3db.txt", "r+");
                if (fp == NULL) {server.type = 6; break;};
                while (fres != 0)
                {
                    // pull record
                    fres = fread(&(server.rd), RCD_SIZE, 1, fp);
                    // id match
                    if (server.rd.id == client.rd.id) {
                        // write null/blank record
                        struct record out;
                        out.id = -1; *(out.name) = '\0';
                        // shift pointer back one record
                        if (fseek(fp, -512, SEEK_CUR) == -1) {server.type = 6; break;};
                        // overwrite (delete)
                        if (fwrite(&out, RCD_SIZE, 1, fp) == 0) {server.type = 6; break;};
                        break;
                        
                        break;
                    }
                }
                // else FAIL
                if (fres == 0) {server.type = 6;}
                break;
            default:
                fprintf(stderr, "client.type (%d) != 1,2,3", client.type);
                break;
        }
        fclose(fp);
        
        // send server msg
        while (1) {
            ssize_t wres = write(c_fd, &server, sizeof(struct msg));
            if (wres == 0)
            {
                printf("socket to client closed prematurely \n");
                //close(c_fd);
                //break;
            }
            if (wres == -1)
            {
                if (errno == EINTR)
                    continue;
                printf("socket to client write failure \n");
                //close(c_fd);
                //break;
            }
            break;
        }
    }
    close_socket: close(c_fd);
}
