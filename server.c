#include <stdio.h>

#include <stdlib.h>

#include <string.h>

#include <unistd.h>

#include <sys/socket.h>

#include <sys/types.h>

#include <arpa/inet.h>

#include <netinet/in.h>

#include <errno.h>

#include <signal.h>

#include <sys/ipc.h>

#include <sys/shm.h>

#include <time.h>

#include <sys/select.h>

#include <semaphore.h>

#include <fcntl.h>

#define MAX_CLIENTS 10
#define MAX_MESSAGE_LOG_SIZE 100
#define MAX_MESSAGE_SIZE 1024
#define KEY_SHM_REC_CLIENT 1234

struct message_entry {
  char message[MAX_MESSAGE_SIZE];
  size_t message_length;
  int client_id;
};

struct client {
  int id, des, status;
};

struct group {
  int id, admin[5], broadcast;
  int arr[5];

};
int l = 0;
struct client active_clients[MAX_CLIENTS];
struct group active_groups[252];

// Define a global array to store the messages
struct message_entry message_log[MAX_MESSAGE_LOG_SIZE];
int message_log_count = 0;
// int active_clients[MAX_CLIENTS];

// Define a semaphore to synchronize access to message log and active clients list
sem_t mutex;
int generateid() {
  srand(time(NULL)); // seed the random number generator with the current time
  return rand() % 90000 + 10000; // generate a random number between 10000 and 99999
}

int main(int argc, char ** argv) {
  int server_socket_fd, client_socket_fd;
  struct sockaddr_in server_address, client_address;
  fd_set readfds;
  int max_fd, sd, activity, new_socket, i, valread, client_id;
  int shmid;
  char buffer[800], buffer1[800];
  char message[1024];
  char client_info[50];
  char * token;
  int num, m;

  // Create a socket
  if ((server_socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    perror("socket");

  int flags = fcntl(server_socket_fd, F_GETFL, 0);
  fcntl(server_socket_fd, F_SETFL, flags | O_NONBLOCK);
  // Set server address
  memset( & server_address, 0, sizeof(server_address));
  server_address.sin_family = AF_INET;
  server_address.sin_addr.s_addr = INADDR_ANY;
  server_address.sin_port = htons(5006);

  // Bind the socket to the server address
  if (bind(server_socket_fd, (struct sockaddr * ) & server_address, sizeof(server_address)) == -1)
    perror("bind");

  // Listen for incoming connections
  if (listen(server_socket_fd, 10) == -1)
    perror("listen");

  // Initialize the active clients list

  for (int i = 0; i < MAX_CLIENTS; i++) {
    struct client new_entry;
    new_entry.id = 0;
    new_entry.des = -1;
    new_entry.status = 0;
    active_clients[i] = new_entry;
  }

  // Initialize the semaphore
  sem_init( & mutex, 0, 1);

  while (1) {
    // Clear the read file descriptor set
    FD_ZERO( & readfds);

    // Add the server socket to the read file descriptor set
    FD_SET(server_socket_fd, & readfds);
    max_fd = server_socket_fd;

    // Add the active client sockets to the read file descriptor set
    for (i = 0; i < MAX_CLIENTS; i++) {
      sd = active_clients[i].des;
      if (sd > 0)
        FD_SET(sd, & readfds);

      if (sd > max_fd)
        max_fd = sd;
    }

    // Wait for activity on any of the sockets
    activity = select(max_fd + 1, & readfds, NULL, NULL, NULL);

    if (activity < 0 && errno != EINTR) {
      perror("select");
      continue;
    }

    // Check if there is activity on the server socket (i.e., a new client is connecting)
    if (FD_ISSET(server_socket_fd, & readfds)) {
      // Accept the incoming connection
      int client_address_length = sizeof(client_address);
      if ((client_socket_fd = accept(server_socket_fd, (struct sockaddr * ) & client_address, & client_address_length)) == -1) {
        perror("accept");
        continue;
      }

      // Add the new client to the active clients list
      for (i = 0; i < MAX_CLIENTS; i++) {
        if (active_clients[i].des == -1) {
          active_clients[i].id = generateid();
          active_clients[i].des = client_socket_fd;
          active_clients[i].status = 1;
          printf("New connection, socket fd is %d, ip is : %s, port : %d,  client id : %d\n", client_socket_fd, inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port), active_clients[i].id);
          //fflush(stdout);
          bzero(buffer, sizeof(buffer));
          sprintf(buffer, "Welcome !! Your client id is %d.\n", active_clients[i].id);
          if (send(active_clients[i].des, buffer, sizeof(buffer), 0) == -1)
            perror("send");
          bzero(buffer, sizeof(buffer));
          break;
        } else if (i == 10) {
          sprintf(buffer, "Sorry !! Maximum 10 clients can be added.\n");
          if (send(active_clients[i].des, buffer, sizeof(buffer), 0) == -1)
            perror("send");
          bzero(buffer, sizeof(buffer));
          break;
        }
      }
    } else {
      for (i = 0; i < MAX_CLIENTS; i++) {
        sd = active_clients[i].des;

        if (FD_ISSET(sd, & readfds)) {
          // Read the incoming message from the client

          bzero(message, sizeof(message));
          valread = recv(sd, buffer, sizeof(buffer), 0);
          //printf("%s",buffer);
          //fflush(stdout);

          // If the read operation returns 0, the client has disconnected
          if (valread == 0) {
            // Close the socket and mark the client as inactive
            close(sd);
            active_clients[i].des = -1;
            active_clients[i].status = 0;
            printf("Client disconnected, socket fd is %d, client id : %d\n", sd, i);

            fflush(stdout);

          } else {

            /* buffer[valread] = '\0';
             printf("Client %d: %s", active_clients[k].id, buffer);*/

            if (strncmp(buffer, "/quit", 5) == 0) {
              // get_client_id(client_socket, sd, &client_id);
              bzero(buffer, sizeof(buffer));
              sprintf(buffer, "Connection terminated.");
              if (send(sd, buffer, strlen(buffer), 0) == -1)
                perror("write");
              shutdown(sd, SHUT_RDWR);
              close(sd);
              active_clients[i].des = -1;
              active_clients[i].status = 0;
              printf("Client %d disconnected\n", active_clients[i].id);
              fflush(stdout);

              // Inform other active clients that the disconnected client left the chat

              sprintf(message, "Client %d left the chat\n", active_clients[i].id);
              puts(message);

              // add_message_entry(message, strlen(message), active_clients[i].id);
              bzero(message, sizeof(message));
              bzero(buffer, sizeof(buffer));
            }

            // If the message is "/list", print list of active clients
            else if (strncmp(buffer, "/list", 4) == 0) {

              bzero(buffer, sizeof(buffer));
              for (int j = 0; j < MAX_CLIENTS; j++) {
                if (active_clients[j].status == 1) {

                  sprintf(client_info, "client id: %d, ip: %s, port: %d\n", active_clients[j].id, inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port));
                  strcat(buffer, client_info);
                }
              }
              bzero(client_info, sizeof(client_info));
              if (send(sd, buffer, strlen(buffer), 0) == -1)
                perror("write");
              bzero(buffer, sizeof(buffer));
            }

            /* char message[1024];
                         strcpy(message,"List of active clients:\n");
                         for (int j = 0; j < MAX_CLIENTS; j++) {
                             if (active_clients[j].status  != 0) {
                                 sprintf(buffer1,"%d",active_clients[j].id);
                                 strcat(message,buffer1);
                             }
                         }
                        send(active_clients[k].des, message, strlen(message), 0);

                        // add_message_entry(message, strlen(message), client_id);
                     }*/

            // Otherwise, add the message to the list of messages from clients and broadcast it to other active clients
            else if (strncmp(buffer, "/broadcast", 10) == 0) {
              for (int j = 0; j < MAX_CLIENTS; j++) {
                int client_socket_fd = active_clients[j].des;
                sprintf(buffer1, "(broadcast)client %d: ", active_clients[i].id);
                strcat(buffer1, buffer + 11);

                // Skip over inactive clients and the client that sent the message
                if (client_socket_fd > 0 && j != i) {
                  if (send(client_socket_fd, buffer1, strlen(buffer1), 0) == -1)
                    perror("send");

                  // get_client_id(client_socket, sd, &client_id);
                  /*   char message[1024];
                     sprintf(message, "Client %d: %s", active_clients[k].id, buffer);
                     add_message_entry(message, strlen(message), client_id);

                     // broadcast message to other active clients
                     for (int j = 0; j < MAX_CLIENTS; j++) {
                         if (active_clients[j].status  != 0 && active_clients[j].des  != sd) {
                             send(active_clients[j].des , message, strlen(message), 0);
                         }
                     }*/
                }
              }
              bzero(buffer, sizeof(buffer));
              bzero(buffer1, sizeof(buffer1));

            } else if (strncmp(buffer, "/sendgroup", 10) == 0) {
              int group_id;
              char grp_id[10];
              sscanf(buffer, "/sendgroup %s %[^\n]", grp_id, message);
              bzero(buffer, sizeof(buffer));
              sprintf(buffer, "client %d : ", active_clients[i].id);
              strcat(buffer, message);
              group_id = atoi(grp_id);
              for (int k = 0; k < 5; k++) {
                if (active_clients[i].id == active_groups[group_id].arr[k]) {
                  for (int s = 0; s < 5; s++) {
                    if (active_groups[group_id].broadcast != 1 || active_groups[group_id].admin[s] == active_clients[i].id) {
                      for (int j = 0; j < MAX_CLIENTS; j++) {
                        for (m = 0; m < 5; m++)
                          if (active_clients[j].status != 0 && active_clients[j].id == active_groups[group_id].arr[m]) {
                            send(active_clients[j].des, buffer, strlen(buffer), 0);
                          }
                      }
                      break;
                    } else if (s < 5) {
                      continue;
                    } else {
                      bzero(buffer, sizeof(buffer));
                      bzero(message, sizeof(message));
                      sprintf(buffer, "Sorry,You are not allowed to send message in this group.\n");
                      send(active_clients[i].des, buffer, strlen(buffer), 0);
                      break;
                    }
                  }
                  break;
                } else if (k < 5) {
                  continue;
                } else {
                  bzero(buffer, sizeof(buffer));
                  bzero(message, sizeof(message));
                  strcpy(buffer, "Sorry,You are not a member of this group.\n");
                  send(active_clients[i].des, buffer, strlen(buffer), 0);
                  break;
                }
              }
              bzero(grp_id, sizeof(grp_id));
              bzero(buffer, sizeof(buffer));
              bzero(message, sizeof(message));

            } else if (strncmp(buffer, "/send", 5) == 0) {
              // Extract client ID and message from buffer
              char client_id[10];
              char message[MAX_MESSAGE_SIZE];
              sscanf(buffer, "/send %s %[^\n]", client_id, message);
              int cli_id = atoi(client_id);

              // Find the client with the specified ID
              int found_client = 0;
              for (int j = 0; j < MAX_CLIENTS; j++) {
                if (active_clients[j].des > 0 && active_clients[j].id == cli_id) {
                  // Send the message to the client
                  sprintf(buffer, "client %d: ", active_clients[i].id);
                  strcat(buffer, message);
                  if (send(active_clients[j].des, buffer, strlen(buffer), 0) == -1) {
                    perror("send");

                  }
                  found_client = 1;
                  break;
                }
              }
              bzero(message, sizeof(message));
              bzero(buffer, sizeof(buffer));

              // Notify the sender if the client was not found
              if (!found_client) {
                char not_found_message[] = "Client not found.\n";
                if (send(active_clients[i].des, not_found_message, strlen(not_found_message), 0) == -1) {
                  perror("send");
                }
              }

            } else if (strncmp(buffer, "/activegroup", 12) == 0) {
              for (int k = 0; k < l; k++) {
                for (int p = 0; p < 5; p++) {
                  if (active_clients[i].id == active_groups[k].arr[p]) {
                    sprintf(message, "Group id %d \n", k);
                    strcat(buffer, message);
                    break;
                  }
                }
              }
              if (send(active_clients[i].des, buffer, strlen(buffer), 0) == -1) {
                perror("send");
              }
              bzero(message, sizeof(message));
              bzero(buffer, sizeof(buffer));

            } else if (strncmp(buffer, "/makeadmin", 10) == 0) {
              char grp[10], id[10];
              int t = 1;
              sscanf(buffer, "/makeadmin %s %s", grp, id);
              int a = atoi(grp);
              int b = atoi(id);
              for (int s = 0; s < 5; s++) {
                if (active_groups[a].admin[s] == active_clients[i].id) {
                  while (t < 5) {
                    if (active_groups[a].admin[t] == -1) {
                      active_groups[a].admin[t] = b;
                      bzero(buffer, sizeof(buffer));
                      sprintf(buffer, "Client %d is now admin of group %d.\n", b, a);
                      if (send(active_clients[i].des, buffer, strlen(buffer), 0) == -1) {
                        perror("send");
                      }
                      break;
                    }
                    t++;
                  }
                  break;
                } else {
                  bzero(buffer, sizeof(buffer));
                  sprintf(buffer, "Sorry,You are not admin.\n");
                  if (send(active_clients[i].des, buffer, strlen(buffer), 0) == -1) {
                    perror("send");
                  }
                  break;
                }
              }
              bzero(grp, sizeof(grp));
              bzero(id, sizeof(id));
              bzero(buffer, sizeof(buffer));

            } else if (strncmp(buffer, "/makegroupbroadcast", 19) == 0) {
              strcpy(message, buffer + 20);
              int a = atoi(message);
              for (int s = 0; s < 5; s++) {
                if (active_groups[a].admin[s] == active_clients[i].id) {
                  active_groups[a].broadcast = 1;
                  bzero(buffer, sizeof(buffer));
                  sprintf(buffer, "Group %d is now broadcast only group.\n", a);
                  if (send(active_clients[i].des, buffer, strlen(buffer), 0) == -1) {
                    perror("send");
                  }
                  break;
                } else if (s < 5) {
                  continue;
                } else {
                  bzero(buffer, sizeof(buffer));
                  sprintf(buffer, "Sorry,You are not admin.\n");
                  if (send(active_clients[i].des, buffer, strlen(buffer), 0) == -1) {
                    perror("send");
                  }
                  break;
                }
              }

              bzero(message, sizeof(message));
              bzero(buffer, sizeof(buffer));
            }/*else if (strncmp(buffer, "/makegroupreq", 13) == 0){
            	m=0;
            	token = strtok(buffer + 14, " ");
            	strcpy(message, "want to join in the group?(y/n)\n");
              while (token != NULL && m < 5) {
              	sscanf(token, "%d", &member);
              	for (int p = 0; p < 10; p++) {
                          if (member == active_clients[p].id) {

                            if(send(active_clients[p].des, message, strlen(message), 0)==-1)
                            perror("send");
                            break;
                          }
                        }
              	}
            
            }*/
             else if (strncmp(buffer, "/makegroup", 10) == 0) {
              m = 0;
              active_groups[l].id = l;
              active_groups[l].broadcast = 0;
              for (int s = 0; s < 5; s++) {
                active_groups[l].admin[s] = -1;
                active_groups[l].arr[s] = -1;
              }
              sprintf(message, "You are added in the group number %d.\n", l);
              token = strtok(buffer + 11, " ");
              while (token != NULL && m < 5) {
                if (sscanf(token, "%d", & active_groups[l].arr[m]) == 1) {
                  for (int j = 0; j < MAX_CLIENTS; j++) {
                    if (active_clients[j].status != 0 && active_clients[j].id == active_groups[l].arr[m]) {
                      send(active_clients[j].des, message, strlen(message), 0);
                    }
                  }
                  //printf("%d ", num);
                }
                token = strtok(NULL, " ");
                m++;

              }
              active_groups[l].admin[0] = active_clients[i].id;
              l++;
              bzero(buffer, sizeof(buffer));
              bzero(message, sizeof(message));
            } else if (strncmp(buffer, "/addtogroup", 11) == 0) {
              int id, m = 0;
              //char * token;
              token = strtok(buffer + 12, " ");
              sscanf(token, "%d", & id);
              for (int s = 0; s < 5; s++) {
                if (active_groups[id].admin[s] == active_clients[i].id) {
                  sprintf(message, "You are added in the group number %d.\n", id);
                  token = strtok(NULL, " ");
                  while (token != NULL) {
                  while(m<5){
                    if (active_groups[id].arr[m] == -1) {
                      if (sscanf(token, "%d", & active_groups[id].arr[m]) == 1) {
                        for (int j = 0; j < MAX_CLIENTS; j++) {
                          if (active_clients[j].status != 0 && active_clients[j].id == active_groups[id].arr[m]) {
                            send(active_clients[j].des, message, strlen(message), 0);
                          }
                        }
                        //printf("%d ", num);
                      }
                      break;

                    }
                    m++;
                    }
                    token = strtok(NULL, " ");
                   
                  }
                  break;
                } else if (s < 5) {
                  continue;
                } else {
                  bzero(buffer, sizeof(buffer));
                  sprintf(buffer, "Sorry,You are not admin.\n");
                  if (send(active_clients[i].des, buffer, strlen(buffer), 0) == -1) {
                    perror("send");
                  }
                  break;
                }
              }
              //bzero(token, sizeof(token));
              bzero(buffer, sizeof(buffer));
              bzero(message, sizeof(message));
            } else if (strncmp(buffer, "/removefromgroup", 16) == 0) {
              int id, member;
              token = strtok(buffer + 17, " ");
              sscanf(token, "%d", & id);
              for (int s = 0; s < 5; s++) {
                if (active_groups[id].admin[s] == active_clients[i].id) {
                  sprintf(message, "You are removed from the group number %d.\n", id);
                  token = strtok(NULL, " ");
                  while (token != NULL) {
                    sscanf(token, "%d", & member);
                    for (int k = 0; k < 5; k++) {
                      if (member == active_groups[id].arr[k]) {
                        active_groups[id].arr[k] = -1;
                        for (int p = 0; p < 10; p++) {
                          if (member == active_clients[p].id) {

                            if(send(active_clients[p].des, message, strlen(message), 0)==-1)
                            perror("send");
                            break;
                          }
                        }
                        break;
                      }
                    }
                    token = strtok(NULL, " ");
                  }
                  break;
                } else if (s < 5) {
                  continue;
                } else {
                  bzero(buffer, sizeof(buffer));
                  sprintf(buffer, "Sorry,You are not admin.\n");
                  if (send(active_clients[i].des, buffer, strlen(buffer), 0) == -1) {
                    perror("send");
                  }
                  break;
                }
              }
              //bzero(token, sizeof(token));
              bzero(buffer, sizeof(buffer));
              bzero(message, sizeof(message));
            }
          }
        }
      }
    }
  }

  // Destroy the semaphore
  sem_destroy( & mutex);

  return 0;
}
