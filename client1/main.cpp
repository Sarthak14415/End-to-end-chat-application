#include<iostream>
using namespace std;
#include<winsock2.h>
#include<ws2tcpip.h>
#include<thread>
#include<string>
/*
* Steps for client 
* 1) intialize winsock
* 2) create socket 
* 3) connect to server 
* 4) send/recv
* 5) close the socket
*/

#pragma comment(lib, "ws2_32.lib")

bool Initialize() {
	WSADATA data;
	return WSAStartup(MAKEWORD(2, 2), &data) == 0;
}

void SendMsg(SOCKET s) {
	// first chat name enter karvao usse -> ki kon message bhej rha hai
	cout << "Enter your chat name" << endl;
	string name;
	getline(cin, name);

	// now yeh bhej sakta hai message jabtak its not disconnected , since humne isko ek alag thread pe 
	// rakha hai therefore jabtak yeh thread hai tabtak yeh bhej sakta hai , receiving uska alag thread pe hoga
	while (1) {
		cout << "Enter your message" << endl;
		string message;
		getline(cin,message);
		string msg = name + " : " + message;

		int bytesent = send(s, msg.c_str(), msg.length(), 0);

		if (bytesent == SOCKET_ERROR){
			cout << "Error sending message" << endl;
			break;
		}

		if (message == "quit") {
			cout << "Application closed " << endl;
			break;
		}

	}

	// bahar agaya matlab ab send aur nahi karega hogaya client ka 
	closesocket(s);
	WSACleanup();
	// jab yeh hoga toh server me isse recieve hona band hojaega so vo isko nikal dega clients list se
}

void RecvMsg(SOCKET s) {
	char buffer[4096];
	while (1) {
		int bytesrecv = recv(s, buffer, sizeof(buffer), 0);
		if (bytesrecv <= 0) {
			cout << "Disconnected from server" << endl;
			break;
		}
		string message(buffer, bytesrecv);

		cout << message << endl;

		// reset the buffer
		memset(buffer, 0, sizeof(buffer));
	}
	
	// now im out of this means this clientsocket is disconnected from server
	closesocket(s);
	WSACleanup();
}

int main() {
	cout << "client program started" << endl;

	if (!Initialize()) {
		cout << "Initialization winsock failed" << endl;
		return 1;
	}	

	// 2) Create socket
	SOCKET s;	
	s = socket(AF_INET, SOCK_STREAM, 0);
	if (s == INVALID_SOCKET) {
		cout << "Invalid socket created" << endl;
		WSACleanup();
		return 1;
	}

	// 127.0.0.1 is the loopback address, also known as localhost
	string serveraddress = "127.0.0.1";
	sockaddr_in serveraddr;
	serveraddr.sin_family = PF_INET;
	serveraddr.sin_port = htons(12345);
	// ip address to binary
	inet_pton(AF_INET, serveraddress.c_str(), &serveraddr.sin_addr);


	// 3) connect to server 
	if (connect(s, reinterpret_cast<sockaddr*>(&serveraddr), sizeof(serveraddr)) == SOCKET_ERROR) {
		cout << "Not able to connect to server" << endl;

		closesocket(s);
		WSACleanup();
		return 1;
	}

	cout << "Successfully connected to server " << endl;

	
	// recv / send

	thread senderthread(SendMsg, s);
	thread recieverthread(RecvMsg, s);


	// yeh humne join kia hai so that the main thread will only end once these two threads will end
	senderthread.join();
	recieverthread.join();


	return 0;
}