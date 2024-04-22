#include <arpa/inet.h> 
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
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
vector<int> permission(3, 0);
#define NUM_OF_USERS 3 // total number of users that can be register
#define MAX_USERS 3
static unsigned int peers_count = 0;
static msg_peer_t *listOfPeers[MAX_USERS] = {0}; // Array of Peers, initialised by null pointer
static pthread_t listen_tid;
static char userSelection;

static void handler(int signum) { pthread_exit(NULL); }

// Define a structure for the message including timestamp and process ID
struct Message
{
	string content;
	int timestamp;
	int pid;
	int msg_cat; /*Define 0 -broadcast ,1 -Reply, 2- Release*/

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
int lamportClock;
/*eraseList function*/
void initialisePeerList();
/* Add user to userList */
void addPeer(msg_peer_t *msg);
/* Delete user from userList */
/*get localIP Address using socket_fd*/
in_addr_t sockfd_to_in_addr_t(int sockfd);
/*function that opens a the client for incoming connection(runs in a new
 * thread)*/
void *listenMode(void *args);
/*generate menu for our p2p client*/
// int openChat(int fd); // opens new windows with xterm to chat.
/*When user sends MSG_WHO this method will get all users from our server*/
void getPeerList();
/*Remove peer from server*/
void removePeerFromServer(struct sockaddr_in *server_conn,
						  msg_ack_t *peerPort);
/*Handle Peer connection*/
void *handlePeerConnection(void *tArgs);

/*broadcast function to all peers*/
void broadcast(struct sockaddr_in *out_sock, msg_ack_t *peerPort,
			   in_addr_t *localIP, char usr_input[C_BUFF_SIZE], int broadcast_type);


int main(int argc, char *argv[])
{
	lamportClock = 0;
	/*program Vars*/
	int server_fd = 0;
	msg_ack_t peerPort;
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
	cout << "Enter your port number\n";
	cin >> peerPort.m_port;
	cout << "Congratulations, your port number is: " << peerPort.m_port;
	cout << "\nEnter the random time you want to start\n";

	getPeerList(); /*Get List of other peers in the network, apart from yourself*/

	if (pthread_create(&listen_tid, NULL, listenMode, (void *)&peerPort) != 0)
		perror("could not create thread");

	do
	{

		sleep(1);

		int choice; /*Choose which type of event the device wants to perform*/
		cout << "Enter choice of event:\n0 for Internal Event\n1 for requesting Critical Section\n2 for exiting the program" << endl;
		cin >> choice;
		fflush(NULL);

		switch (choice)
		{
		case 0:

			// perform internal event
			lamportClock++;

			cout << "Internal Event : Updated lamport clock of " << usr_input << " is " << lamportClock << endl;

			break;
		case 1:

			// broadcast request
			permission = {0}; /*Reset permission vector to 0 for each iteration*/
			lamportClock++;	  /*Increment Lamport Clock before broadcasting request*/
			cout << "Broadcasting request" << endl;

			broadcast(&out_sck, &peerPort, &localIP, (char *)&usr_input, 7);
			lamportClock++;
			cout << "Broadcasting release" << endl;
			if (requested_for_cs == 0)
				broadcast(&out_sck, &peerPort, &localIP, (char *)&usr_input, 14);
			break;

		case 2:
			exit(1);
		}
		cont++;
		sleep(1);
	} while (cont <= 10);

	pthread_join(listen_tid, NULL);
	//*The connection is closed by server in each communication!//

	return 0;
}

/*eraseList function*/
void initialisePeerList()
{
	for (int i = 0; i < MAX_USERS; i++)
	{
		free(listOfPeers[i]);
		listOfPeers[i] = 0;
	}
}
/* Add user to userList */
void addPeer(msg_peer_t *msg)
{
	if (peers_count == MAX_USERS)
	{
		cout<<"MAX-USERS exceeded\n";
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
		cout<<"Error creating socket!\n";
		exit(1);
	}
	cout<<"Socket created...\n";

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = actualArgs->m_port;

	ret = bind(sockfd, (struct sockaddr *)&addr, sizeof(addr));
	if (ret < 0)
	{
		cout<<"Error binding!\n";
		exit(1);
	}
	cout<<"Binding done...\n";

	cout<<"Waiting for peer connection, Listening on Port: <<"<< addr.sin_port<<"\n";
	listen(sockfd, 2);
	// getPeerList(&addr);
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
		// accept connection from an incoming peers
		client_fd = accept(sockfd, (struct sockaddr *)NULL, NULL); // busy-waiting for incoming connections

		if (client_fd < 0)
		{
			perror("try to accept incoming connection failed");
		}
		else

		{
			cout << "Connection accepted sending MSG_ACK to client\n";
		}

		/*prompt the user if he want to accept call*/
		/*in a new thread*/
		/*there deal with input*/
		arg_handle_peer.client_fd = client_fd;
		if (pthread_create(&t, NULL, handlePeerConnection, (void *)&arg_handle_peer) != 0)
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

// takes the input of all the three peers, note that the order of the peers input data should be same for all devices
void getPeerList()
{

	initialisePeerList();

	for (int i = 0; i < 3; i++)
	{

		msg_peer_t *peer = (msg_peer_t *)malloc(sizeof(msg_peer_t));
		cout << "enter one of the peers name :\n";
		string temp;
		cin >> temp;
		strcpy(peer->m_name, temp.c_str());
		cout << "enter one of the peers port :\n";
		cin >> peer->m_port;
		cout << "enter one of the peers IP Address :\n";
		string temp1;
		cin >> temp1;
		peer->m_addr = inet_addr(temp1.c_str());
		peer->m_type = 31;
		addPeer(peer);
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
	lamportClock = 1 + max(lamportClock, lamportclock_recv);

	if (msg_cat_no == 0)
	{
		messageQueue.push(Message(message, lamportclock_recv, atoi(pidstring), msg_cat_no));

		cout << "broadcast request received\n";
		cout << "Lamport clock received from sender is " << lamportclock_recv << "\n";
		cout << "Updated Lamport Clock of listener/receiver is " << lamportClock << "\n";
		cout << "Pid received is " << atoi(pidstring) << "\n";

		priority_queue<Message> tempQueue = messageQueue; // Create a temporary queue to keep the original queue intact
		cout << "\nMessages in the queue:\n";
		while (!tempQueue.empty())
		{
			Message msg = tempQueue.top();
			cout << "Content: " << msg.content
				 << " Timestamp: " << msg.timestamp
				 << " PID: " << msg.pid << std::endl;
			tempQueue.pop();
		}

		// Replying to the request
		char lampclockstring_reply[20]; // Allocate a buffer to hold
										// the converted string
		char pidstring_reply[20];
		char msg_cat_string_reply[5];
		// Convert the integer to a string using sprintf
		sprintf(lampclockstring_reply, "%d", lamportClock);
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
		cout << "Give reply index to broadcaster as " << reply_received_from_index << endl;

		auto check = send(*client_fd, message_reply.c_str(), message_reply.length() + 1, 0);
		cout << "Client FD of connection between " << reply_received_from_index << "and " << pidstring << "is " << client_fd << endl;
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
		cout << "\nMessages in the queue after release/popping:\n";
		while (!tempQueue.empty())
		{
			Message msg = tempQueue.top();
			cout << "Content: " << msg.content
				 << " Timestamp: " << msg.timestamp
				 << " PID: " << msg.pid << endl;
			tempQueue.pop();
		}
	}

	pthread_detach(pthread_self());
	return 0;
}

void broadcast(struct sockaddr_in *out_sock, msg_ack_t *peerPort, in_addr_t *localIP, char usr_input[C_BUFF_SIZE], int broadcast_type)
{
	/*Function VARS*/

	int broadcaster_index = 0;
	int equlsPeerFD = -1; // used to open a new chat windows using FD Number
	msg_peer_t peerSelection;
	int userSelection = 0;
	/****************/
	/*print all connected Peers*/
	for (int i = 0; i < MAX_USERS; i++)
	{

		if (listOfPeers[i])
		{

			cout << "[" << i << "]\t-\t Username : " << listOfPeers[i]->m_name << "\n";
			cout << "[" << i << "]\t-\t IP : " << inet_ntoa(*(struct in_addr *)&listOfPeers[i]->m_addr) << "\n";
			cout << "[" << i << "]\t-\t Port : " << listOfPeers[i]->m_port << "\n\n";
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

	char lampclockstring[20]; // Allocate a buffer to hold the converted string
	char pidstring[20];
	char msg_cat_string[5];
	// Convert the integer to a string using sprintf
	sprintf(lampclockstring, "%d", lamportClock);
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
		messageQueue.push(Message(message, lamportClock, broadcaster_index, 0));
	else
		/*In the broadcaster's side, his entry will always be on top of queue*/
		messageQueue.pop();

	while (userSelection <= 2)
	{

		if (strcmp(listOfPeers[userSelection]->m_name, usr_input) !=
			0)
		{

			cout << "Send Event : " << usr_input << " is sending to " << listOfPeers[userSelection]->m_name << "\n";
			cout << "Updated Lamport Clock of Sender is " << lamportClock << "\n";
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

				cout << "\nYou choose: " << peerSelection.m_name << "\n";
				cout << "The IP:port of Requested peers is: " << inet_ntoa(*(struct in_addr *)&peerSelection.m_addr) << ":" << peerSelection.m_port << "\n";

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
					cout<<"\n Error : Could not create "
						   "socket \n";
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
				cout<<"Sending to " << peerSelection.m_name << " Sending MSG_CONN and Waiting for Response...\n\n";
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
					lamportClock =
						1 + max(lamportClock, lamportclock_recv);
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
								cout << "LEAVING THE CRITICAL SECTION!!!" << endl;
								cout << "Peer " << broadcaster_index << " is about to broadcast a release" << endl;
								/*Release Critical Section now by popping the broadcaster's entry from each queue*/
								char lampclockstring[20]; // Allocate a buffer to hold the converted
														  // string
								char pidstring[20];		  // Allocate a buffer to hold the converted string
								char msg_cat_string[5];
								// Convert the integer to a string using sprintf
								sprintf(lampclockstring, "%d", lamportClock);
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