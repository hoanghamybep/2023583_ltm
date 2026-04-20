#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <poll.h>

#define MAX_CLIENTS 1024

int check_login(char *user, char *pass) {
    FILE *f = fopen("databases.txt", "r");
    if (f == NULL) {
        perror("Khong tim thay file databases.txt");
        return 0;
    }

    char f_user[32], f_pass[32];
    while (fscanf(f, "%s %s", f_user, f_pass) != EOF) {
        if (strcmp(user, f_user) == 0 && strcmp(pass, f_pass) == 0) {
            fclose(f);
            return 1;
        }
    }
    fclose(f);
    return 0;
}

int main() {
    int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener < 0) {
        perror("socket() failed");
        return 1;
    }

    if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0) {
        perror("setsockopt() failed");
        return 1;
    }

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(8080);

    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind() failed");
        return 1;
    }
    
    listen(listener, 5);
    printf("Telnet Server is listening on port 8080...\n");

    struct pollfd fds[MAX_CLIENTS];
    int logged_in[MAX_CLIENTS] = {0};
    int nfds = 1;

    for (int i = 0; i < MAX_CLIENTS; i++) {
        fds[i].fd = -1;
        fds[i].events = POLLIN;
        logged_in[i] = 0;
    }

    fds[0].fd = listener;

    char buf[256], tmp_file[64];

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
                        logged_in[j] = 0;
                        if (j >= nfds) {
                            nfds = j + 1;
                        }
                        break;
                    }
                }
                if (j == MAX_CLIENTS) {
                    printf("Server full.\n");
                    close(client);
                } else {
                    send(client, "Hay gui user pass de dang nhap:\n", 32, 0);
                }
            }
        }

        for (int i = 1; i < nfds; i++) {
            if (fds[i].fd != -1 && (fds[i].revents & POLLIN)) {
                int client = fds[i].fd;
                ret = recv(client, buf, sizeof(buf) - 1, 0);
                
                if (ret <= 0) {
                    printf("Client %d disconnected\n", client);
                    close(client);
                    fds[i].fd = -1;
                    logged_in[i] = 0;
                } else {
                    buf[ret] = 0;
                    if (buf[ret-1] == '\n') buf[ret-1] = 0;
                    if (buf[ret-1] == '\r') buf[ret-1] = 0;

                    if (logged_in[i] == 0) {
                        char user[32], pass[32];
                        if (sscanf(buf, "%s %s", user, pass) == 2 && check_login(user, pass)) {
                            logged_in[i] = 1;
                            send(client, "Dang nhap thanh cong. Nhap lenh:\n", 33, 0);
                        } else {
                            send(client, "Sai tai khoan. Nhap lai:\n", 25, 0);
                        }
                    } else {
                        sprintf(tmp_file, "out_%d.txt", client);
                        char cmd[512];

                        sprintf(cmd, "%s > %s 2>&1", buf, tmp_file);
                        system(cmd);

                        FILE *f = fopen(tmp_file, "r");
                        if (f) {
                            char file_buf[1024];
                            while (fgets(file_buf, sizeof(file_buf), f)) {
                                send(client, file_buf, strlen(file_buf), 0);
                            }
                            fclose(f);

                            remove(tmp_file); 
                        }
                        send(client, "\nDone.\n", 7, 0);
                    }
                }
            }
        }
    }
    return 0;
}