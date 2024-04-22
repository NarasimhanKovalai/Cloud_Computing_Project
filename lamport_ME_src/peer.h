
#include <netinet/in.h>

/*
 *  buffer size used while send() and recv()
 */
#define C_BUFF_SIZE 1024

/*name of the registered node should have maximum 10 char*/

#define C_NAME_LEN 10

#define MSG_ACK 11
#define MSG_PEER 12
#define MSG_CONN 13

typedef int msg_type_t;

// It contains the node's port number as provided in the config text files.

typedef struct msg_ack
{
    msg_type_t m_type;
    in_port_t m_port;
    int starter_time;
} msg_ack_t;

/*
 *  message formats: MSG_PEER
 *  Used by the server to convey the details of a single peer.
 */
typedef struct msg_peer
{
    msg_type_t m_type;
    in_addr_t m_addr;            // Peer's IP address
    in_port_t m_port;            // Peer's port number
    char m_name[C_NAME_LEN + 1]; // Peer's display name
} msg_peer_t;

//  Used by a node to request a connection:

typedef struct msg_conn
{
    msg_type_t m_type;
    in_addr_t m_addr;            // Sender's IP address
    in_port_t m_port;            // Sender's port number
    char m_name[C_NAME_LEN + 1]; // Sender's display name
} msg_conn_t;
