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
#include "client.h"

void echo_client(int sockfd) {
    struct pollfd fds[2];
    char buff[MSG_LEN];
    uint32_t net_size, size;
    int n;

    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN;

    fds[1].fd = sockfd;
    fds[1].events = POLLIN;

    printf("Connecté au serveur. Saisissez votre message (Tapez /quit pour quitter).\n");
    
    printf("Message: ");
    fflush(stdout);

    while (1) {
        if (poll(fds, 2, -1) < 0) {
            perror("poll()");
            break;
        }

        // Message à envoyer
        if (fds[0].revents & POLLIN) {
            memset(buff, 0, MSG_LEN);
            n = 0;
            while ((buff[n] = getchar()) != '\n' && buff[n] != EOF) {
                if (n < MSG_LEN - 1) n++;
            }
            buff[n] = '\0';

            size = strlen(buff);
            net_size = htonl(size);

            if (send(sockfd, &net_size, sizeof(net_size), 0) <= 0) break;
            if (send(sockfd, buff, size, 0) <= 0) break;

            if (strcmp(buff, "/quit") == 0) {
                printf("Déconnexion...\n");
                break;
            }
            printf("Message sent!\n");
        }

        // Le serveur reçoit
        if (fds[1].revents & POLLIN) {
            n = recv(sockfd, &net_size, sizeof(net_size), 0);
            if (n <= 0) {
                printf("\nConnexion fermée par le serveur.\n");
                break;
            }
            size = ntohl(net_size);

            memset(buff, 0, MSG_LEN);
            n = recv(sockfd, buff, size, 0);
            if (n <= 0) break;
            
            printf("Received: %s\n", buff);

            printf("Message: ");
            fflush(stdout);
        }
    }
}

int handle_connect(const char *server_name, const char *server_port) {
    struct addrinfo hints, *result, *rp;
    int sfd;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    
    if (getaddrinfo(server_name, server_port, &hints, &result) != 0) {
        perror("getaddrinfo()");
        exit(EXIT_FAILURE);
    }
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sfd == -1) continue;
        if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1) break;
        close(sfd);
    }
    if (rp == NULL) {
        fprintf(stderr, "Could not connect\n");
        exit(EXIT_FAILURE);
    }
    freeaddrinfo(result);
    return sfd;
}

int main(int argc, char *argv[]) {
    int sfd;
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <server_name> <server_port>\n", argv[0]);
        return EXIT_FAILURE;
    }
    sfd = handle_connect(argv[1], argv[2]);
    echo_client(sfd);
    close(sfd);
    return EXIT_SUCCESS;
}