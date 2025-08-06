#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>

#define MAX 800
#define PORT 5006
#define SA struct sockaddr

void func(int sockfd) {
    char buff[MAX];
    char send_buff[800];
    int n;
    fd_set read_fds;
    FD_ZERO(&read_fds);
    

 while (1) {   
        FD_SET(STDIN_FILENO, &read_fds);
        FD_SET(sockfd, &read_fds);

        if (select(sockfd + 1, &read_fds, NULL, NULL, NULL) < 0) {
            perror("select");
            exit(EXIT_FAILURE);
        }

        if (FD_ISSET(STDIN_FILENO, &read_fds)) {
            bzero(buff, sizeof(buff));
            n = 0;
            printf("Enter your message:");
	    fflush(stdout);
           //while ((buff[n++] = getchar()) != '\n')
                ;
            if(read(0,buff,sizeof(buff))>0){
            if (send(sockfd, buff, sizeof(buff), 0) < 0) {
                perror("write");
                exit(EXIT_FAILURE);
            }
            }
        }

        if (FD_ISSET(sockfd, &read_fds)) {
            bzero(buff, sizeof(buff));
            if (recv(sockfd, buff, sizeof(buff), 0) < 0) {
                if (errno == EINTR) continue;  // interrupted by a signal, try again
                perror("read");
                exit(EXIT_FAILURE);
            }
            //fflush(stdout);
            else{
            printf("\n%s\n", buff);
            fflush(stdout);}
            //continue;
        }
    }
}
/*void CtrlC_Handler(int sig) {

     close(_c->_client_socket);
}*/

int main() {
    //signal(SIGINT, CtrlC_Handler);
    
    int sockfd;
    struct sockaddr_in servaddr, cli;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    servaddr.sin_port = htons(PORT);

    if (connect(sockfd, (SA *)&servaddr, sizeof(servaddr)) != 0) {
        perror("connect");
        exit(EXIT_FAILURE);
    }

    func(sockfd);
    close(sockfd);

    return 0;
}

