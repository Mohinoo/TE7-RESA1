#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <poll.h>

#include "common.h"
#include "server.h"

#define MAX_CLIENTS 100

struct client_node { // Liste chaînée
    int fd;
    char ip[INET_ADDRSTRLEN];
    int port;
    struct client_node *next;
};

struct client_node *clients_head = NULL;

void add_client(int fd, struct sockaddr_in *addr) {
    struct client_node *new_node = malloc(sizeof(struct client_node));
    new_node->fd = fd;
    inet_ntop(AF_INET, &(addr->sin_addr), new_node->ip, INET_ADDRSTRLEN);
    new_node->port = ntohs(addr->sin_port);
    new_node->next = clients_head;
    clients_head = new_node;
    printf("Nouveau client connecté : IP %s | Port %d | FD %d\n", new_node->ip, new_node->port, new_node->fd);
}

void remove_client(int fd) {
    struct client_node **curr = &clients_head;
    while (*curr) {
        if ((*curr)->fd == fd) {
            struct client_node *to_delete = *curr;
            *curr = (*curr)->next;
            printf("Client déconnecté (FD %d).\n", fd);
            free(to_delete);
            return;
        }
        curr = &((*curr)->next);
    }
}

void echo_server(int listen_sfd) {
    struct pollfd fds[MAX_CLIENTS + 1];
    int nfds = 1;

    fds[0].fd = listen_sfd;
    fds[0].events = POLLIN;

    while (1) {
        if (poll(fds, nfds, -1) < 0) {
            perror("poll()");
            break;
        }

        if (fds[0].revents & POLLIN) { //New connexion
            struct sockaddr_in cli;
            socklen_t len = sizeof(cli);
            int connfd = accept(listen_sfd, (struct sockaddr *)&cli, &len);
            if (connfd >= 0) {
                if (nfds < MAX_CLIENTS + 1) {
                    fds[nfds].fd = connfd;
                    fds[nfds].events = POLLIN;
                    nfds++;
                    add_client(connfd, &cli);
                } else {
                    printf("Limite de clients atteinte. Rejet.\n"); //Si 100 clients
                    close(connfd);
                }
            }
        }

        for (int i = 1; i < nfds; i++) {
            if (fds[i].revents & POLLIN) {
                int cfd = fds[i].fd;
                char buff[MSG_LEN];
                uint32_t net_size, size;

                int n = recv(cfd, &net_size, sizeof(net_size), 0);
                if (n <= 0) {
                    close(cfd);
                    remove_client(cfd);
                    fds[i] = fds[nfds - 1];
                    nfds--;
                    i--;
                    continue;
                }

                size = ntohl(net_size);
                memset(buff, 0, MSG_LEN);
                
                n = recv(cfd, buff, size, 0);
                
                if (n <= 0 || strcmp(buff, "/quit") == 0) {
                    close(cfd);
                    remove_client(cfd);
                    fds[i] = fds[nfds - 1];
                    nfds--;
                    i--;
                } else {
                    printf("Received (FD %d): %s\n", cfd, buff);
                    // Renvoi au client expéditeur (ECHO)
                    send(cfd, &net_size, sizeof(net_size), 0);
                    send(cfd, buff, size, 0);
                    printf("Message sent!\n");
                }
            }
        }
    }
}

int handle_bind(const char *server_port) {
    struct addrinfo hints, *result, *rp;
    int sfd;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    
    if (getaddrinfo(NULL, server_port, &hints, &result) != 0) {
        perror("getaddrinfo()");
        exit(EXIT_FAILURE);
    }
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sfd == -1) continue;
        int opt = 1;
        setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        if (bind(sfd, rp->ai_addr, rp->ai_addrlen) == 0) break;
        close(sfd);
    }
    if (rp == NULL) {
        fprintf(stderr, "Could not bind\n");
        exit(EXIT_FAILURE);
    }
    freeaddrinfo(result);
    return sfd;
}

int main(int argc, char *argv[]) {
    int sfd;
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <server_port>\n", argv[0]);
        return EXIT_FAILURE;
    }
    
    sfd = handle_bind(argv[1]);
    
    if ((listen(sfd, SOMAXCONN)) != 0) {
        perror("listen()\n");
        exit(EXIT_FAILURE);
    }
    
    printf("Serveur en écoute sur le port %s...\n", argv[1]);
    
    echo_server(sfd);
    
    close(sfd);
    return EXIT_SUCCESS;
}