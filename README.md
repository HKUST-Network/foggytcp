# ELEC 3120 Course Project: TCP in the Wild

Welcome to ELEC3120 course project! In this project, you will implement a TCP socket yourself in C++. This is a semester-long project with four checkpoints, and we will guide you through the whole process.

Why so? Because we want you to understand how the most important protocol on the Internet, TCP, works, and the best way to understand it is to implement it yourself. And also, for many ELEC students, this will be the only programming project in your undergraduate study. For future engineers, it is important to know how to make plans for a project.

## Checkpoints
### Checkpoint 1: Kickoff
- How to use Git, GitHub, and Copilot
- How to set up the environment using VirtualBox and vagrant
- How to send and receive data using sockets
- Understand the transmission delay and propagation delay through measurements

### Checkpoint 2: Basic TCP
- Sequence number and acknowledgement number
- Sliding window

### Checkpoint 3: TCP Functionalities
- Fast retransmission
- Congestion control

### Checkpoint 4: Advanced TCP
- Implement (or design) a new congestion control
- Evaluate under different network conditions
- (Bonus) compete with other teams

## Files
The following files have been provided for you:

* `Vagrantfile`: Defines the structure, IP addresses, and dependencies in the containers. Feel free to modify this file to add any additional testing tools as you see fit.

* `foggy_backend.cc`: This file contains the backend code that will run in a separate thread from the application. This is where most of your logic should go. The backend should deal with most of the TCP functionality, including the state machine, timeouts, retransmissions, buffering, congestion control, etc.

* `foggy_tcp.cc`: This contains the main socket functions required of your TCP socket including reading, writing, opening and closing. Since TCP needs to works asynchronously with the application, these functions are relatively simple and interact with the backend running in a separate thread.

* `server.cc`: An application using the server side of your transport protocol. We will test your code using a different server program, so do not keep any variables or functions here that your protocol uses.

* `client.cc`: An application using the client side of your transport protocol. We will test your code using a different client application, so do not keep any variables or functions here that your protocol uses.

* `foggy_packet.h`: This file describes the basic packet format and header and provides helper functions that will help you handle packets. You are not allowed to modify this file! The scripts that we provide to help you graph your packet traces rely on this file being unchanged. All the communication between your server and client will use UDP as the underlying protocol. All packets will begin with the common header described as follows:

    * Course Number 		    [4 bytes]
    * Source Port 			    [2 bytes]
    * Destination Port 		    [2 bytes]
    * Sequence Number 		    [4 bytes]
    * Acknowledgement Number 	[4 bytes]
    * Header Length		        [2 bytes]
    * Packet Length			    [2 bytes]
    * Flags				        [1 byte]
    * Advertised Window		    [2 bytes]
    * Extension length		    [2 bytes]
    * Extension Data		    [You may use it when designing your own congestion control algorithm]

* `grading.h`: These are variables that we will use to test your implementation. We will be replacing this file when running tests, and hence you should test your implementation with different values.

* `capture_packets.sh`: Captures packets from the server and client containers and saves them to a PCAP file.

* `tcp.lua`: A Lua plugin that allows Wireshark to decode foggy-TCP headers. Copy the file to the directory described in <https://www.wireshark.org/docs/wsug_html_chunked/ChPluginFolders.html> to use the plugin.

## Single-session lifecycle repair

This branch repairs peer pinning and FIN/EOF handling without changing the four
public function declarations.
