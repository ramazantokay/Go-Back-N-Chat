# Go-Back-N Protocol for Chat on Command Line

This project implements a Go-Back-N protocol for a chat scenario on the command line. It consists of two binaries: a client and a server. The client and server communicate with each other, allowing two participants to chat with each other. The implementation ensures reliable transfer of messages even in the presence of a bad network that can reorder, drop, delay, or duplicate packets. The communication is done using UDP packets with a payload size limit of 16 bytes.

## Features
- Chat between two participants: The client and server binaries facilitate a chat session between two users on the command line.

- Reliability using Go-Back-N protocol: The implementation employs the Go-Back-N protocol to ensure reliable transfer of messages over an unreliable network.

- Support for packet reordering, dropping, and duplication: The protocol handles various network conditions such as packet reordering, dropping, and duplication.

- Timer mechanism: Each sent packet has a timer associated with it, allowing the detection of lost packets through timeouts.

- User-friendly termination: The chat session can be terminated gracefully by entering three consecutive newline characters (two empty lines) in the user input.

- Threading for concurrent sending and receiving: Threading is used to listen for incoming ACK packets and data packets concurrently on both the client and server endpoints.

## Prerequisites
To build and run this project, you'll need the following:

- C programming language compiler (e.g., gcc)
- Linux environment

## Getting Started
1. Clone the repository:
```
git clone https://github.com/ramazantokay/Go-Back-N-Chat.git
```
2. Change into the project directory:
```
cd Go-Back-N-Chat
```
3. Build the binaries:
```
make all
```
4. Start the server:
```
./server <client_ip_address> <server_port_number>
```
5. Start the client:
```
./client <server_ip_address> <server_port_number>
```

6. Start chatting between the client and server.

7. Terminate the chat session by entering three consecutive newline characters (two empty lines) in the user input.

<br>
<br>

## Acknowledgements
This project is based on the Go-Back-N protocol described in the textbook "Computer Networking: A Top-Down Approach" by James F. Kurose and Keith W. Ross.