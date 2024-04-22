#!/bin/bash

# Compile the C++ program
g++ -o peer_node chatc.cpp -pthread -std=c++11
rm -rf adi.txt nar.txt yas.txt

# # Check if compilation was successful
# if [ $? -eq 0 ]; then

#     # Run the executable, including prompt statements and user inputs in the log file
#     {
#         # Read name input
#         read -p "Enter your name: " name
#         echo "$name"

#         # Read port number input
#         read -p "Enter your port number: " port_number
#         echo "$port_number"

#         # Read IP address input
#         read -p "Enter your IP address: " ip_address
#         echo "$ip_address"

#         # Read peers' details input (3 times)
#         for ((i=1; i<=3; i++)); do
#             read -p "Enter peer $i name: " peer_name
#             read -p "Enter peer $i port: " peer_port
#             read -p "Enter peer $i IP address: " peer_ip
#             echo "Peer $i name: $peer_name, Port: $peer_port, IP address: $peer_ip"
#         done

#         # Choose an event (0 for Internal Event)
#         echo "Enter choice of event:"
#         echo "0 for Internal Event"
#         echo "1 for requesting CS"
#         read event_choice
#         echo "$event_choice"
#     } | ./peer_node 2>&1
    
#     echo "Execution complete. Check log.txt for details."
# else
#     echo "Compilation failed!"
# fi
