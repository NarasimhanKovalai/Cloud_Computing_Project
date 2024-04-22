# Rules used to update the Lamport Clock for events.
* a. newClock=oldClock+1
* b. newClock=max(newClock,oldClock) + 1
## Sender Side
1. Whenever a process requests a critical section, it sends a broadcast request to other 2 processes. The Lamport Clock is updated before sending the broadcast request (rule a).

2. Whenever a process sends a reply to the broadcast request, it updated its Lamport Clock before sending it to the broadcaster (rule a).

3. Whenever a process sends a release message after accessing critical section, its Lamport Clock is updated (rule a).

## Receiver Side
1. Whenever process receives request to access critical section from broadcaster, it updates it Lamport clock according to rule b.

2. Whenever process receives a critical section release message, it updates its Lamport Clock according to rule b.

# Framework Used

We employ direct connection between nodes, **without** the requirement for a central server for coordination.

* We use TCP network protocol for establishing connection between nodes. 
* Each node spawns a thread for entering into the listening mode.

