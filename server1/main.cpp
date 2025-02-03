#include<iostream>
#include<winsock2.h>
#include<WS2tcpip.h>
#include<tchar.h>
#include<thread>
#include<vector>
#include <queue>               // For client socket queue
#include <mutex>               // For thread synchronization
#include <condition_variable>  // For notifying worker threads

// THREADPOOL-> extra 
// queue: Stores client sockets to be processed by worker threads. Joh thread khaali hogi vo isme se client lelegi
// mutex : Ensures safe access to the shared queue.
// condition_variable : Notifies worker threads when a new client is added

using namespace std;

/*
* steps needed to make websocket application
* 
* 1) intialize the socket library -> since we are using winsock library for it 
* 
* 2) create the socket 
* 
* 3) get ip and port
* 
* 4) bind the ip and port with the socket -> the server needs to bind the socket with a specific IP address 
    and port number, telling the operating system where to listen for incoming connections.

  5) Listen on the socket -> makes the socket wait for incoming connections.

  6) Accept  -> If no client is trying to connect, the accept() function waits (blocks) until a connection request is received.
    Once a client connects, it returns a new socket (clientSocket), which is used for communication.

  7) recieve and send -> data transfer 

  8) close the socket 

  9) cleanup winsock 
*/


// so code ko batao ki usko socket library use krna hai 
#pragma comment(lib,"ws2_32.lib")
// This pragma directive tells the linker to automatically link the Winsock 2 library(ws2_32.lib) to the program when compiling on Windows


const int MAX_THREADS = 16;    // Number of worker threads
queue<SOCKET> clientQueue;    // Queue to store client sockets
mutex queueMutex;             // Mutex for synchronizing access to the queue
condition_variable queueCondition;  // Condition variable for notifying threads
bool serverRunning = true;    // Server status flag


// vector -> to store all the clients that are interacting with the server
vector<SOCKET>clients;

bool Initialize() {
	WSADATA data;
	//since we are using the 2.2 version we will do 2,2
	return WSAStartup(MAKEWORD(2, 2), &data) == 0;
	// if it is returning 0 means it has been initilaized correctly 
}

void InteractWithClient(SOCKET clientsocket, vector<SOCKET>& clients) {
	cout << "Client connected" << endl;

	// 7) recieve and send data to the client

	// now to recieve the data we have to create a buffer jaha pe voh client message recieve hoga
	char buffer[4096];
	// since multiple messages recv and send ho sakte hai so therefore while loop
	while (1) {

		// this recv is a blocking call jabtak data nahi ayega it will wait here and not execute further
		// acc to this API -> if it returns 0 -> client has disconnected no more messages 
		// -> if it gives <= 0 then it means there is some connection fault or abrubtly disconnected
		int bytesrecvd = recv(clientsocket, buffer, sizeof(buffer), 0);
		
		// ab agar no bytes recieved so 
		if (bytesrecvd <= 0) {
			cout << "client disconnected" << endl;
			break;
		}
		string message(buffer, bytesrecvd);
		cout << "message from client : " << message << endl;

		// now we have to send this message to all the clients also connected to server
		for (auto c : clients) {
			// joh original client hai usko nahi bhejna uske alawa sabko bhejna hai
			if (c != clientsocket) {
				send(c, message.c_str(), message.length(), 0);
			}
		}

		// clear the buffer 
		memset(buffer, 0, sizeof(buffer));
	}


	// so ab bytesrec <= 0 -> socket close karna pdega hogaya interaction uss client ka 
	// so remove them from vector also
	auto it = find(clients.begin(), clients.end(), clientsocket);
	if (it != clients.end()) {
		clients.erase(it);
	}


	closesocket(clientsocket);

}

void WorkerThread() {
	while (serverRunning) {
		SOCKET clientSocket;

		// Lock queue and wait for a new client
		{
			unique_lock<mutex> lock(queueMutex);

			/*The main purpose of a condition variable is to allow a thread to wait for a condition
			(like a new client arriving) while temporarily releasing the lock so that other threads 
			can access the shared resource(like adding a new client to the queue) during that waiting period.*/

			//this will wait until client ki queue empty nahi hai or server ruk nahi gaya that is until we've closed the server
			queueCondition.wait(lock, [] { return !clientQueue.empty() || !serverRunning; });

			if (!serverRunning) break; // Exit if server is shutting down

			// Get a client from the queue
			clientSocket = clientQueue.front();
			clientQueue.pop();
		} // <- ye {} hai scope of mutex lock , after this scope lock automatically unlocks

		// Process client
		// yeh interaction with client will be outside mutex because it can take too long 
		// mutex sirf shared resource ke hi liye use 
		InteractWithClient(clientSocket,clients);
	}
}




int main() {
	// we have to check if it has initialized correctly otherwise stop
	if (!Initialize()) {
		cout << "winsock initialization failed" << endl;
	}
	cout << "server program" << endl;

	//actual server code 

	// 2) create the socket
	// SOCKET is just another datatype like int and socket functions returns that 
	// the socket function takes input (address family -> ipv4  , socket type -> we used TCP , and protocol)
	// we sent 0 in protocol so that it identifies and chooses the best protocol present on its own

	SOCKET listensocket = socket(AF_INET, SOCK_STREAM, 0);

	// *** jab bhi aap koi networking ya socket programming ka code likho toh uske API ki return value ko
	// check zaroor karna hai agar galat value aagayi and check nahi kia so dikkat hojaegi

	if (listensocket == INVALID_SOCKET) {
		cout << "Socket creation failed" << endl;

		// and since socket creation failed no point moving forward so cleanup	
		WSACleanup();
		return 1;
	}

	// 3) Get ip and port

	// create address structure 

	// The sockaddr_in structure is part of the Winsock library on Windows and contains fields for the address family, port number, and IP address.
	sockaddr_in serveraddr;
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_port = htons(12345);
	// so humne port 1 2 3 4 and 5 mention kardiye
	// htons() is a function(api) that converts the port number from host byte order(native endianness of the machine) 
	// to network byte order(big - endian), which is the standard used for transmission over networks.

	// we have to convert the ipaddress to binary format to put inside sin_family
	// that will happen with INETPton API
	// InetPton() returns 1 on success, 0 if the address string is invalid, and SOCKET_ERROR if there is an error.
	// since we are using out own computer as host computer therefore the IP address is 0.0.0.0
	if (InetPton(AF_INET, _T("0.0.0.0"), &serveraddr.sin_addr) != 1) {
		cout << "setting address structure failed" << endl;

		closesocket(listensocket);
		WSACleanup();
		return 1;
	}

	// 4) bind the ip and port with the socket
	// uske parameters hai api ke vo vo bhrdo harek me
	if (bind(listensocket, reinterpret_cast<sockaddr*>(&serveraddr), sizeof(serveraddr)) == SOCKET_ERROR){
		cout << "Bind failed" << endl;

		closesocket(listensocket);
		WSACleanup();
		return 1;

	}

	// 5) listen on the socket 
	// we have to pass the maximum number of connections the socket will queue up before listening to it
	// we can give any number but this is set up 
	// SOCKET_ERROR is a common error which occur everytime something in socket fails
	if (listen(listensocket, SOMAXCONN) == SOCKET_ERROR) {
		cout << "Listen failed" << endl;

		closesocket(listensocket);
		WSACleanup();
		return 1;
	}

	// since port is in network format so to represent in host format we have to bring it back in 
	cout << "Server started listening on port : " << ntohs(serveraddr.sin_port) << endl;

	// initialize all the threads 
	vector<thread> threadPool;
	for (int i = 0; i < MAX_THREADS; i++) {
		// emplace_back() constructs the thread directly inside the vector, avoiding any copy - related issues.
		// Threads cant be copied 
		threadPool.emplace_back(WorkerThread);
	}


	

	while (serverRunning) {
		// 6) Accept -> start accepting the connection from the client
		SOCKET clientsocket = accept(listensocket, nullptr, nullptr);
		// second and third parameters pe we have client address but here we dont have it so we have done it null

		// It returns a new socket (clientsocket) that can be used for communication with that specific client.
		// The original socket(listensocket) remains available for accepting additional client connections.

		// If no client connected, continue
		if (clientsocket == INVALID_SOCKET) {
			this_thread::sleep_for(chrono::milliseconds(100)); // Prevent busy loop
			continue;
		}

		cout << "New client connected" << endl;

		// Add client to the queue
		{
			lock_guard<mutex> lock(queueMutex);
			clientQueue.push(clientsocket);
		}

		clients.push_back(clientsocket);

		// Notify a worker thread that a new client is available
		queueCondition.notify_one();



		// OLD 

		//// ab iss clientsocket ko bhejdo on a thread and joh poora buffer me recv and send vala
		//// interaction hoga that is in function InteractWithClient 
		//// threads me by refrence ese hi pass hota hai 
		//thread t1(InteractWithClient, clientsocket, std::ref(clients));
		//// make the thread run independently in the background 
		//t1.detach(); 
		//// ab koi naya client ayega so it will react interact using another thread with the help of loop


		// this application of server will keep going on in this state until we manually close it down
	}


	// To finally end the server we do serverRunning = false and notify all threads
	serverRunning = false;
	queueCondition.notify_all();  // Wake up all threads so they can exit

	// Ensure that all threads finish there task before ending the main thread , so we join all threads
	for (auto& t : threadPool) {
		t.join();  // Ensure all threads finish before exiting
	}
	

    

	
	closesocket(listensocket);

		

	// finalize
	WSACleanup();
	return 0;
}