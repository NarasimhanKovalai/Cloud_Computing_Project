#include <arpa/inet.h> //inet_addr
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>	//printf
#include <stdlib.h> //exit, perror
#include <string.h> //memset
#include <unistd.h>

#include <algorithm>
#include <iostream>
#include <queue>
#include <string>
#include <vector>

#include <cstring>
#include <cstdlib>
#include <cstdio>

#include "chat.h"
using namespace std;
int perm_count = 0;
int reply_received_from_index = 0;
int cont = 0;
int requested_for_cs = 0;
/*7 for broadcast request and 14 for broadcast release */

priority_queue<pair<int, int>> request_queue;
vector<int> permission(3, 0);

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

#define NUM_OF_USERS 10 // total number of users that can be register
#define MAX_USERS 50
static unsigned int peers_count = 0;
static msg_peer_t *listOfPeers[MAX_USERS] = {
	0}; // global array of connected users to the system. notice the
		// {0}initializer it is very important!
static pthread_t listen_tid;
static char userSelection;

static void handler(int signum) { pthread_exit(NULL); }

// Define a structure for the message including timestamp and process ID
struct Message
{
	string content;
	int timestamp;
	int pid;
	int msg_cat; /*Define 0 -Broadcast ,1 -Reply, 2- Release*/

	Message(string content, int timestamp, int pid, int msg_cat)
		: content(content),
		  timestamp(timestamp),
		  pid(pid),
		  msg_cat(msg_cat) {}

	// Overloading the '<' operator for priority queue
	bool operator<(const Message &msg) const
	{
		if (timestamp == msg.timestamp)
		{
			return pid > msg.pid; // If timestamps are
								  // equal, give priority
								  // to the smaller PID
		}
		return timestamp > msg.timestamp;
	}
};

typedef struct arg_handle
{
	struct sockaddr_in addr;
	int client_fd;
} arg_handle;

priority_queue<Message> messageQueue;

/*local logical lamport clock*/
int LampClock;
/*eraseList function*/
void erase_all_users();
/* Add user to userList */
void user_add(msg_peer_t *msg);
/* Delete user from userList */
/*get localIP Address using socket_fd*/
in_addr_t sockfd_to_in_addr_t(int sockfd);
/*function that connects client to the server sends MSG_UP and gets MSG_ACK*/
void connect_server(struct sockaddr_in *server_conn,
					msg_ack_t *server_assigned_port, int *server_fd,
					in_addr_t *localIP, char usr_input[C_BUFF_SIZE]);
/*function that opens a the client for incoming connection(runs in a new
 * thread)*/
void *listenMode(void *args);
/*generate menu for our p2p client*/
// int openChat(int fd); // opens new windows with xterm to chat.
/*When user sends MSG_WHO this method will get all users from our server*/
void getListFromServer(struct sockaddr_in *server_conn);
/*Remove peer from server*/
void removePeerFromServer(struct sockaddr_in *server_conn,
						  msg_ack_t *server_assigned_port);
/*Handle Peer connection*/
void *handlePeerConnection(void *tArgs);
/*the peer choose which peer he wants to connect with*/
void selectPeerToConnect(struct sockaddr_in *out_sock,
						 msg_ack_t *server_assigned_port, in_addr_t *localIP,
						 char usr_input[C_BUFF_SIZE]);
/*Broadcast function to all peers*/
void Broadcast(struct sockaddr_in *out_sock, msg_ack_t *server_assigned_port,
			   in_addr_t *localIP, char usr_input[C_BUFF_SIZE], int broadcast_type);

void generate_menu()
{
	printf("Please select one of the following options:\n");
	printf("[0]\t-\t Send MSG_WHO to server - get list of all connected "
		   "users and choose peer to chat with\n");
	printf("[9]\t-\t Send MSG_DOWN to server - unregister ourselves from "
		   "server\n");
}

int main(int argc, char *argv[])
{
	cout << "Compilation succesfull!\n";
	LampClock = 0;
	/*program Vars*/
	int server_fd = 0;
	msg_ack_t server_assigned_port;
	struct sockaddr_in server_conn, incoming_sck;
	struct sockaddr_in out_sck;
	in_addr_t localIP;			 // our peer IP Address
	char usr_input[C_BUFF_SIZE]; // for user input during the program
	/***************************************/
	signal(SIGUSR1, handler);
	memset(&server_conn, 0, sizeof(struct sockaddr_in));
	memset(&incoming_sck, 0, sizeof(struct sockaddr_in));
	memset(&out_sck, 0, sizeof(struct sockaddr_in));

	cout << "Enter your name :\n";
	string name;
	cin >> name;
	strcpy(usr_input, name.c_str());
	cout << "enter your port number\n";
	cout << "Note: this can be hardcoded to a particular port number and can be run across the machines\n";
	cin >> server_assigned_port.m_port;
	cout << "Congratulations, your port number as assigned by the server is: " << server_assigned_port.m_port;
	cout << "\nenter the randome time you want to start\n";
	cin >> server_assigned_port.starter_time;
	cout << "Time received from server: " << server_assigned_port.starter_time << "\n";

	getListFromServer(&server_conn);
	if (pthread_create(&listen_tid, NULL, listenMode, (void *)&server_assigned_port) != 0)
		perror("could not create thread");

	do
	{

		sleep(1);

		int choice;
		printf("Enter choice of event:\n0 for Internal Event\n1 for requesting CS\n");
		// scanf("%d", &choice);
		cin >> choice;
		fflush(NULL);

		switch (choice)
		{
		case 0:

			// perform internal event
			LampClock++;

			printf("Internal Event : Updated lamport clock "
				   "of %s is %d\n",
				   usr_input, LampClock);

			break;
		case 1:
			// broadcast request
			permission = {0};
			LampClock++;
			int x = 2;
			printf("Broadacasting request\n");

			Broadcast(&out_sck, &server_assigned_port,
					  &localIP, (char *)&usr_input, 7);
			LampClock++;
			printf("Broadcasting release\n");
			if (requested_for_cs == 0)
				Broadcast(&out_sck, &server_assigned_port,
						  &localIP, (char *)&usr_input, 14);
			break;
		}
		// printf(" Press 1 to Continue?");
		// scanf("%d",&cont);
		cont++;
		sleep(1);
	} while (cont <= 10);

	pthread_join(listen_tid, NULL);
	//*The connection is closed by server in each communication!//

	return 0;
}

/*eraseList function*/
void erase_all_users()
{
	for (int i = 0; i < MAX_USERS; i++)
	{
		free(listOfPeers[i]);
		listOfPeers[i] = 0;
	}
}
/* Add user to userList */
void user_add(msg_peer_t *msg)
{
	if (peers_count == MAX_USERS)
	{
		printf("sorry the system is full, please try again later\n");
	}
	int i;
	for (i = 0; i < MAX_USERS; i++)
	{
		if (!listOfPeers[i])
		{
			listOfPeers[i] = msg;
			peers_count++;
			return;
		}
	}
}

/*get localIP Address using socket_fd*/
in_addr_t sockfd_to_in_addr_t(int sockfd)
{
	int s = sockfd;
	struct sockaddr_in sa;
	socklen_t sa_len;
	/* We must put the length in a variable.              */
	sa_len = sizeof(sa);
	/* Ask getsockname to fill in this socket's local     */
	/* address.                                           */
	if (getsockname(s, (struct sockaddr *)&sa, &sa_len) == -1)
	{
		perror("getsockname() failed");
		return -1;
	}
	return sa.sin_addr.s_addr;
}

/*function that connects client to the server sends MSG_UP and gets MSG_ACK*/
void connect_server(struct sockaddr_in *server_conn,
					msg_ack_t *server_assigned_port, int *server_fd,
					in_addr_t *localIP, char usr_input[C_BUFF_SIZE])
{
	/*function VARS*/
	char ip_str[INET_ADDRSTRLEN]; // holds the server IPv4 address in str
								  // form
	/************************************/
	/*The Client is trying to register in our *running* server, if it fails
	 * close client!*/
	printf("Hello dear Client pls write the server IP Address:(by the "
		   "following format xxx.xxx.xxx.xxx)\n");
	printf("Press 'Enter' For Default IP\n");

	fgets(ip_str, INET_ADDRSTRLEN, stdin); // gets input from usr
	if (ip_str[0] == 10)
	{
		strcpy(ip_str, "127.0.0.1");
	}

	// store this IP address,port and IPv4 settings in server_conn:
	inet_pton(AF_INET, ip_str, &server_conn->sin_addr);
	server_conn->sin_family = AF_INET;
	// server_conn.sin_addr.s_addr = inet_addr(ip_str);
	server_conn->sin_port = htons(C_SRV_PORT);
	// if the IP is inValid exit program!
	if ((server_conn->sin_addr.s_addr = inet_addr(ip_str)) == -1)
	{
		perror(strerror(EAFNOSUPPORT));
		exit(1);
	}
	// open socket
	if ((*server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		printf("\n Error : Could not create socket \n");
		exit(1);
	}

	if (connect(*server_fd, (struct sockaddr *)server_conn,
				sizeof(*server_conn)) < 0)
	{
		perror("connect");
		exit(1);
	}

	puts("Connected to server, Please register in order to fetch user "
		 "List\n");
	puts("Please write your name to register(no longer than 24 chars)\n");
	fgets(usr_input, C_BUFF_SIZE, stdin); // gets input from usr
	while (strlen(usr_input) > 24)
	{
		perror("the name you've entered is too big!\n");
		perror("Please try again!\n");
		fgets(usr_input, C_BUFF_SIZE, stdin); // gets input from usr
	}
	// send MSG_UP to our server
	msg_up_t sendToServer;
	sendToServer.m_type = MSG_UP;
	strcpy(sendToServer.m_name, usr_input);
	*localIP = sockfd_to_in_addr_t(*server_fd);
	sendToServer.m_addr = *localIP;

	// Send MSG_UP
	if (send(*server_fd, (void *)&sendToServer, sizeof(sendToServer), 0) <
		0)
	{
		puts("Send failed");
		exit(1);
	}
	puts("MSG_UP Sent to server\n");
	// get MSG_ACK message from server and start listen at the port inside
	// MSG_ACK
	if (recv(*server_fd, server_assigned_port, sizeof(msg_ack_t), 0) ==
		-1)
	{
		perror("read Messege \"MSG_ACK\" fail");
		exit(1);
	}
	puts("Success : got MSG_ACK message");
}

void *listenMode(void *args)
{
	static sigset_t mask;

	sigemptyset(&mask);
	sigaddset(&mask, SIGUSR1);

	/*we have the port num start listen in that port for incoming
	 * connections*/
	/*got it from "main"*/
	/*Define vars for communication*/
	msg_ack_t *actualArgs = (msg_ack_t *)args;
	int socket_fd, client_fd;
	pthread_t t; // thread for the accepted request
	struct sockaddr_in addr;
	int sockfd, ret;
	/*******************************/

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0)
	{
		printf("Error creating socket!\n");
		exit(1);
	}
	printf("Socket created...\n");

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = actualArgs->m_port;

	ret = bind(sockfd, (struct sockaddr *)&addr, sizeof(addr));
	if (ret < 0)
	{
		printf("Error binding!\n");
		exit(1);
	}
	printf("Binding done...\n");

	printf("Waiting for peer connection, Listening on Port:%d\n",
		   addr.sin_port);
	listen(sockfd, 2);
	// getListFromServer(&addr);
	for (int i = 0; i < MAX_USERS; i++)
	{
		if (listOfPeers[i])
		{
			if (listOfPeers[i]->m_port == actualArgs->m_port)
			{
				reply_received_from_index = i;
				break;
			}
		}
	}

	arg_handle arg_handle_peer;
	arg_handle_peer.addr = addr;

	while (1)
	{
		// accept connection from an incoming pers
		client_fd = accept(sockfd, (struct sockaddr *)NULL,
						   NULL); // busy-waiting...

		if (client_fd < 0)
		{
			perror("try to accept incoming connection failed");
		}
		else

		{
			printf("Connection accepted sending MSG_ACK to client");
		}

		/*prompt the user if he want to accept call*/
		/*in a new thread*/
		/*there deal with input*/
		arg_handle_peer.client_fd = client_fd;
		if (pthread_create(&t, NULL, handlePeerConnection,
						   (void *)&arg_handle_peer) != 0)
		{

			perror("could not create thread");
		}

		pthread_join(t, NULL);
		if (close(client_fd) == -1)
		{
			perror("close fail");
		}
		sleep(1);
	}
	if (close(socket_fd) == -1)
	{
		perror("close fail");
		exit(1);
	}
	return 0;
}
/*When user sends MSG_WHO this method will get all users from our server*/
void getListFromServer(struct sockaddr_in *server_conn)
{

	erase_all_users();

	for (int i = 0; i < 3; i++)
	{

		// msg_type_t	m_type;				// = MSG_PEER
		// in_addr_t   m_addr;				// Peer's IP address
		// in_port_t   m_port;				// Peer's port number
		// char        m_name[C_NAME_LEN + 1];		// Peer's display name
		msg_peer_t *PeerMsgFromServer = (msg_peer_t *)malloc(sizeof(msg_peer_t));
		cout << "enter one of the peers name :\n";
		// fflush(stdout);
		// cin.getline(PeerMsgFromServer->m_name, C_NAME_LEN + 1);
		// getline(cin, PeerMsgFromServer->m_name);
		// cin.ignore();
		string temp;
		cin >> temp;
		strcpy(PeerMsgFromServer->m_name, temp.c_str());
		// cin >> PeerMsgFromServer->m_name;
		cout << "enter one of the peers port :\n";
		cin >> PeerMsgFromServer->m_port;
		// cin.get();
		cout << "enter one of the peers IP Address :\n";

		// string temp1;
		// cin >> temp1;
		// strcpy(PeerMsgFromServer->m_addr,temp1.c_str());
		string temp1;
		cin >> temp1;

		PeerMsgFromServer->m_addr = inet_addr(temp1.c_str());
		// cin.get();
		PeerMsgFromServer->m_type = 31; // can be anything for now*******************************************************************
		// cout << m_name << " : " << m_addr
		user_add(PeerMsgFromServer);
	}
}

void *handlePeerConnection(void *tArgs)
{
	arg_handle *arg_handle_peer = (arg_handle *)tArgs;
	int *client_fd = &(arg_handle_peer->client_fd);
	struct sockaddr_in *addr = &(arg_handle_peer->addr);

	char message[50];	// Allocate a buffer to hold the received message
	char pidstring[20]; // Allocate a buffer to hold the converted string
	char lamportclockstring[20];
	int lamportclock_recv;
	char msg_cat_string[5];

	// Receive the message
	if (recv(*client_fd, message, 50, 0) < 0)
	{
		puts("recv failed");
	}

	// Extract Lamport clock value and PID from the received message
	sscanf(message, "%[^,],%[^,],%s", lamportclockstring, pidstring, msg_cat_string);

	lamportclock_recv = atoi(lamportclockstring);
	auto msg_cat_no = (atoi(msg_cat_string));

	// Update the Lamport clock

	// Push the received message with timestamp and PID into the queue
	LampClock = 1 + max(LampClock, lamportclock_recv);

	if (msg_cat_no == 0)
	{
		messageQueue.push(Message(message, lamportclock_recv,
								  atoi(pidstring), msg_cat_no));

		printf("Broadcast request received\n");
		printf("Lamport clock received from sender is %d\n ",
			   lamportclock_recv);
		printf("Debug statement\n");
		printf("Updated Lamport Clock of listener/receiver is %d\n",
			   LampClock);
		printf("Pid received is %d\n", atoi(pidstring));

		priority_queue<Message> tempQueue =
			messageQueue; // Create a temporary queue to keep the
						  // original queue intact
		std::cout << "\nMessages in the queue:\n";
		while (!tempQueue.empty())
		{
			Message msg = tempQueue.top();
			std::cout << "Content: " << msg.content
					  << " Timestamp: " << msg.timestamp
					  << " PID: " << msg.pid << std::endl;
			tempQueue.pop();
		}

		// Replying to the request
		char lampclockstring_reply[20]; // Allocate a buffer to hold
										// the converted string
		char pidstring_reply[20];		// Allocate a buffer to hold the
										// converted string
		char msg_cat_string_reply[5];
		// Convert the integer to a string using sprintf
		sprintf(lampclockstring_reply, "%d", LampClock);
		sprintf(pidstring_reply, "%d", reply_received_from_index);
		sprintf(msg_cat_string_reply, "%d", 1);

		string message_reply = lampclockstring_reply;
		cout << message_reply << endl;
		message_reply += ",";
		cout << "pid string is" << pidstring_reply << endl;
		message_reply += pidstring_reply;
		cout << message_reply << endl;
		message_reply += ",";
		message_reply += msg_cat_string_reply;
		cout << message_reply << endl;
		cout << "Give reply index to broadcaster as  "
			 << reply_received_from_index << endl;

		auto check = send(*client_fd, message_reply.c_str(),
						  message_reply.length() + 1, 0);
		cout << "Client FD of connection between "
			 << reply_received_from_index << "and " << pidstring
			 << "is " << client_fd << endl;
		if (check != -1)
		{
			cout << "Reply successfully executed" << endl;
		}
	}

	else if (msg_cat_no == 2)
	{
		priority_queue<Message> temp;
		while (!messageQueue.empty())
		{
			if (messageQueue.top().pid == atoi(pidstring))
			{
				messageQueue.pop(); // Pop the element
				break;
			}
			else
			{
				temp.push(messageQueue.top());
				messageQueue.pop();
			}
		}

		// Transfer the messages back to the original queue
		while (!temp.empty())
		{
			messageQueue.push(temp.top());
			temp.pop();
		}
		priority_queue<Message> tempQueue =
			messageQueue; // Create a temporary queue to keep the
						  // original queue intact
		std::cout << "\nMessages in the queue after release/popping:\n";
		while (!tempQueue.empty())
		{
			Message msg = tempQueue.top();
			std::cout << "Content: " << msg.content
					  << " Timestamp: " << msg.timestamp
					  << " PID: " << msg.pid << std::endl;
			tempQueue.pop();
		}
	}

	pthread_detach(pthread_self());
	return 0;
}

void Broadcast(struct sockaddr_in *out_sock, msg_ack_t *server_assigned_port,
			   in_addr_t *localIP, char usr_input[C_BUFF_SIZE], int broadcast_type)
{
	/*Function VARS*/

	int broadcaster_index = 0;
	int equlsPeerFD =
		-1; // used to open a new chat windows using FD Number
	msg_peer_t peerSelection;
	int userSelection = 0;
	/****************/
	/*print all connected Peers*/
	for (int i = 0; i < MAX_USERS; i++)
	{

		/*Send MSG_PEER messeges*/
		if (listOfPeers[i])
		{
			/*print connected users*/
			printf("\n[%d]\t-\t Username : %s\n", i,
				   listOfPeers[i]->m_name);
			printf("[%d]\t-\t IP : %s\n", i,
				   inet_ntoa(*(struct in_addr *)&listOfPeers[i]
								  ->m_addr));
			printf("[%d]\t-\t Port : %d\n\n", i,
				   listOfPeers[i]->m_port);
		}
	}

	for (int i = 0; i < MAX_USERS; i++)
	{
		if (listOfPeers[i])
		{
			if (strcmp(listOfPeers[i]->m_name, usr_input) == 0)
			{
				broadcaster_index = i;
			}
		}
	}

	// /*Let us randomly choose to which peer he want's to connect*/
	// printf("Choose peer number to send to \n");
	// // printf("Choosing a random client to send to : \n");
	// scanf("%d", &userSelection);
	char lampclockstring[20]; // Allocate a buffer to hold the converted
							  // string
	char pidstring[20];		  // Allocate a buffer to hold the converted string
	char msg_cat_string[5];
	// Convert the integer to a string using sprintf
	sprintf(lampclockstring, "%d", LampClock);
	sprintf(pidstring, "%d", broadcaster_index);
	if (broadcast_type == 7)
		sprintf(msg_cat_string, "%d", 0);
	else
		sprintf(msg_cat_string, "%d", 2);

	string message = lampclockstring;
	message += ",";
	message += pidstring;
	message += ",";
	message += msg_cat_string;
	if (broadcast_type == 7)
		messageQueue.push(Message(message, LampClock, broadcaster_index, 0));
	else
		/*In the broadcaster's side, his entry will always be on top of queue*/
		messageQueue.pop();

	while (userSelection <= 2)
	{

		if (strcmp(listOfPeers[userSelection]->m_name, usr_input) !=
			0)
		{

			printf("Send Event : %s is sending to %s\n ", usr_input,
				   listOfPeers[userSelection]->m_name);
			printf("Updated Lamport Clock of Sender is %d\n",
				   LampClock);
			msg_conn_t sendToPeer;
			sendToPeer.m_type = MSG_CONN;
			strcpy(sendToPeer.m_name, usr_input);
			sendToPeer.m_addr = *localIP;

			/*Fetch Peer data by user choice*/

			/*******************************/
			if (listOfPeers[userSelection] != 0)
			{
				peerSelection.m_addr =
					listOfPeers[userSelection]->m_addr;
				peerSelection.m_port =
					listOfPeers[userSelection]->m_port;
				peerSelection.m_type =
					listOfPeers[userSelection]->m_type;
				strcpy(peerSelection.m_name,
					   listOfPeers[userSelection]->m_name);

				printf("\nYou choose: %s\n",
					   peerSelection.m_name);
				printf("The IP:port of Requested peers is : "
					   "%s:%d\n",
					   inet_ntoa(
						   *(struct in_addr *)&peerSelection
								.m_addr),
					   peerSelection.m_port);

				/*open socket, connect to other peer, get FD,
				 * send msg_conn_t and wait for RESPONSE from
				 * other PEER */
				out_sock->sin_family = AF_INET;
				out_sock->sin_addr.s_addr =
					peerSelection.m_addr;
				out_sock->sin_port = peerSelection.m_port;
				// open socket
				if ((equlsPeerFD = socket(AF_INET, SOCK_STREAM,
										  0)) < 0)
				{
					printf("\n Error : Could not create "
						   "socket \n");
					exit(1);
				}

				if (connect(equlsPeerFD,
							(struct sockaddr *)out_sock,
							sizeof(*out_sock)) < 0)
				{
					perror("connect");
					exit(1);
				}

				// cout<<"The port of broadcaster is
				// :"<<listOfPeers[broadcaster_index]->m_port;
				printf("Sending to  -%s-  Sending MSG_CONN and "
					   "Waiting for Response...\n\n",
					   peerSelection.m_name);
				cout << broadcaster_index << "is sending to "
					 << userSelection << "on fd "
					 << equlsPeerFD;
				send(equlsPeerFD, message.c_str(),
					 message.length() + 1, 0);

				// Receive reply
				if (broadcast_type == 7)
				{
					char reply[1024];
					if (recv(equlsPeerFD, reply, 1024, 0) == -1)
					{
						perror("Reply for brodcast request not "
							   "received!");
						exit(1);
					}
					puts("Success :Reply received\n");
					cout << "Reply received is: " << reply << "\n";

					char lamportclockstring[10];
					char pidstring[10];
					char msg_cat_string[10];
					sscanf(reply, "%[^,],%[^,],%s", lamportclockstring,
						   pidstring, msg_cat_string);

					int lamportclock_recv =
						atoi(lamportclockstring);
					auto msg_cat_no = (atoi(msg_cat_string));
					LampClock =
						1 + max(LampClock, lamportclock_recv);
					int flag = 0;

					cout << "Recevied permission from " << pidstring << endl;
					perm_count++;
					permission[atoi(pidstring)] = 1;
					for (auto x : permission)
						cout << "Permission i is " << x << endl;
					if (perm_count != 2)
					{
						cout << "Not yet receieved all "
								"permissions"
							 << endl;
					}
					else
					{
						cout << "Received all permissions\n"
							 << endl;

						requested_for_cs = 1;
						priority_queue<Message> tempQueue =
							messageQueue; // Create a temporary queue to keep the original
										  // queue intact
						Message msg_top = tempQueue.top();
						while (requested_for_cs == 1)
						{
							if (messageQueue.top().pid == broadcaster_index)
							{
								requested_for_cs = 0;
								cout << "Peer " << broadcaster_index << "is allowed to access the critical section !" << endl;
								cout << "ENTERING THE CRITICAL SECTION!!!" << endl;
								sleep(5);
								cout<<"LEAVING THE CRITICAL SECTION!!!"<<endl;
								cout << "Peer " << broadcaster_index << " is about to broadcast a release" << endl;
								/*Release Critical Section now by popping the broadcaster's entry from each queue*/
								char lampclockstring[20]; // Allocate a buffer to hold the converted
														  // string
								char pidstring[20];		  // Allocate a buffer to hold the converted string
								char msg_cat_string[5];
								// Convert the integer to a string using sprintf
								sprintf(lampclockstring, "%d", LampClock);
								sprintf(pidstring, "%d", broadcaster_index);
								sprintf(msg_cat_string, "%d", 0);
								string message = lampclockstring;
								message += ",";
								message += pidstring;
								message += ",";
								message += msg_cat_string;
							}
							else
							{
								cout << "Peer " << broadcaster_index << "is denied access to the critical section due to queue rule !" << endl;
							}
						}
					}
				}
				std::cout << "\nMessages in sender's queue:\n";
				priority_queue<Message> tempQueue =
					messageQueue;
				while (!tempQueue.empty())
				{
					Message msg = tempQueue.top();
					std::cout << "Content: " << msg.content
							  << " Timestamp: " << msg.timestamp
							  << " PID: " << msg.pid << std::endl;
					tempQueue.pop();
				}
			}
		}
		userSelection++;
	}
}