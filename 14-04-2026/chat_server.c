#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <time.h>
#include <poll.h> 

#define PORT 8080
#define MAX_CLIENTS 1024

int main() {
    int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == -1) {
        perror("socket() failed");
        return 1;
    }
    
    if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int))) {
        perror("setsockopt() failed");
        return 1;
    }
    
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(PORT);
    
    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr))) {
        perror("bind() failed");
        return 1;
    }
    
    if (listen(listener, 5)) {
        perror("listen() failed");
        return 1;
    }
    
    printf("Chat Server is listening on port %d...\n", PORT);

    struct pollfd fds[MAX_CLIENTS];
    char *client_names[MAX_CLIENTS];
    int nfds = 1; 
    for (int i = 0; i < MAX_CLIENTS; i++) {
        fds[i].fd = -1;     
        fds[i].events = POLLIN;
        client_names[i] = NULL;
    }

    fds[0].fd = listener;

    char buf[256];
    char send_buf[512];

    while (1) {
        int ret = poll(fds, nfds, -1); 

        if (ret < 0) {
            perror("poll() failed");
            break;
        }

        if (fds[0].revents & POLLIN) {
            int client = accept(listener, NULL, NULL);
            if (client >= 0) {
                printf("New connection: %d\n", client);

                int j;
                for (j = 1; j < MAX_CLIENTS; j++) {
                    if (fds[j].fd == -1) {
                        fds[j].fd = client;
                        fds[j].events = POLLIN;
                        
                        if (j >= nfds) {
                            nfds = j + 1;
                        }
                        break;
                    }
                }
                
                if (j == MAX_CLIENTS) {
                    printf("Server da day, khong the nhan them client.\n");
                    close(client);
                } else {
                    send(client, "Hay nhap ten theo cu phap 'client_id: client_name':\n", 53, 0);
                }
            }
        }

        for (int i = 1; i < nfds; i++) {
            if (fds[i].fd != -1 && (fds[i].revents & POLLIN)) {
                int client = fds[i].fd;
                ret = recv(client, buf, sizeof(buf) - 1, 0);
                
                if (ret <= 0) {
                    printf("Client %d disconnected\n", client);
                    free(client_names[i]);
                    client_names[i] = NULL;
                    close(client);
                    fds[i].fd = -1;
                } else {
                    buf[ret] = 0;
                    
                    if (buf[ret-1] == '\n') buf[ret-1] = 0;
                    if (buf[ret-1] == '\r') buf[ret-1] = 0;

                    if (client_names[i] == NULL) {
                        char cmd[32], name[64];
                        int n = sscanf(buf, "%[^:]: %s", cmd, name);
                        
                        if (n >= 2) {
                            client_names[i] = strdup(name);
                            sprintf(send_buf, "Chao %s! Ban co the bat dau chat.\n", name);
                            send(client, send_buf, strlen(send_buf), 0);
                        } else {
                            char *msg = "Sai cu phap. Yeu cau 'client_id: client_name':\n";
                            send(client, msg, strlen(msg), 0);
                        }
                    } else {
                        time_t rawtime;
                        struct tm *timeinfo;
                        time(&rawtime);
                        timeinfo = localtime(&rawtime);
                        char time_str[20];
                        strftime(time_str, sizeof(time_str), "%Y/%m/%d %I:%M:%p", timeinfo);

                        sprintf(send_buf, "%s %s: %s\n", time_str, client_names[i], buf);
                        
                        for (int j = 1; j < nfds; j++) {
                            if (fds[j].fd != -1 && j != i && client_names[j] != NULL) {
                                send(fds[j].fd, send_buf, strlen(send_buf), 0);
                            }
                        }
                    }
                }
            }
        }
    }
    return 0;
}