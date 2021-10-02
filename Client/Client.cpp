// myclient.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#pragma comment(lib,"ws2_32.lib") //Required for WinSock
#include <WinSock2.h> //For win sockets
#include <string> //For std::string
#include <iostream> //For std::cout, std::endl, std::cin.getline
#include <fstream>
using namespace std;
SOCKET Connection;//This client's connection to the server

struct filename {
	string filename;
	int filesize;
	static const int buffersize = 8192;
	int bytescounter;
	int remaining_bytes;
	ifstream  file;
	ofstream outfile;
	char buffer[buffersize];
	int bytesWritten = 0;
}File;

enum Packet
{
	P_ChatMessage,
	P_FileTransferRequest,
	P_FileTransferByteBuffer,
	P_FileTransfer_EndOfFile,
	P_one,
	P_two,
	P_three
};

string filename;
string your_name;
string reqfile;
bool recvall(char* data, int totalbytes)
{
	int bytesreceived = 0; //Holds the total bytes received
	while (bytesreceived < totalbytes) //While we still have more bytes to recv
	{
		int RetnCheck = recv(Connection, data + bytesreceived, totalbytes - bytesreceived, NULL); //Try to recv remaining bytes
		if (RetnCheck == SOCKET_ERROR) //If there is a socket error while trying to recv bytes
			return false; //Return false - failed to recvall
		bytesreceived += RetnCheck; //Add to total bytes received
	}
	return true; //Success!
}

bool sendall(char* data, int totalbytes)
{
	int bytessent = 0; //Holds the total bytes sent
	while (bytessent < totalbytes) //While we still have more bytes to send
	{
		int RetnCheck = send(Connection, data + bytessent, totalbytes - bytessent, NULL); //Try to send remaining bytes
		if (RetnCheck == SOCKET_ERROR) //If there is a socket error while trying to send bytes
			return false; //Return false - failed to sendall
		bytessent += RetnCheck; //Add to total bytes sent
	}
	return true; //Success!
}
bool Sendint32_t(int32_t _int32_t)
{
	_int32_t = htonl(_int32_t); //Convert long from Host Byte Order to Network Byte Order
	if (!sendall((char*)&_int32_t, sizeof(int32_t))) //Try to send int... If int fails to send
		return false; //Return false: int not successfully sent
	return true; //Return true: int successfully sent
}

bool Getint32_t(int32_t& _int32_t)
{
	if (!recvall((char*)&_int32_t, sizeof(int32_t))) //Try to receive int... If int fails to be recv'd
		return false; //Return false: Int not successfully received
	_int32_t = ntohl(_int32_t); //Convert long from Network Byte Order to Host Byte Order
	return true;//Return true if we were successful in retrieving the int
}

bool SendPacketType(Packet _packettype)
{
	if (!Sendint32_t(_packettype)) //Try to send packet type... If packet type fails to send
		return false; //Return false: packet type not successfully sent
	return true; //Return true: packet type successfully sent
}

bool GetPacketType(Packet& _packettype)
{
	int packettype;
	if (!Getint32_t(packettype))//Try to receive packet type... If packet type fails to be recv'd
		return false; //Return false: packet type not successfully received
	_packettype = (Packet)packettype;
	return true;//Return true if we were successful in retrieving the packet type
}

bool SendString(std::string& _string, bool IncludePacketType = true)
{
	if (IncludePacketType)
	{
		if (!SendPacketType(P_ChatMessage)) //Send packet type: Chat Message, If sending packet type fails...
			return false; //Return false: Failed to send string
	}
	int32_t bufferlength = _string.size(); //Find string buffer length
	if (!Sendint32_t(bufferlength)) //Send length of string buffer, If sending buffer length fails...
		return false; //Return false: Failed to send string buffer length
	if (!sendall((char*)_string.c_str(), bufferlength)) //Try to send string buffer... If buffer fails to send,
		return false; //Return false: Failed to send string buffer
	return true; //Return true: string successfully sent
}

bool GetString(std::string& _string)
{
	int32_t bufferlength; //Holds length of the message
	if (!Getint32_t(bufferlength)) //Get length of buffer and store it in variable: bufferlength
		return false; //If get int fails, return false
	char* buffer = new char[bufferlength + 1]; //Allocate buffer
	buffer[bufferlength] = '\0'; //Set last character of buffer to be a null terminator so we aren't printing memory that we shouldn't be looking at
	if (!recvall(buffer, bufferlength)) //receive message and store the message in buffer array. If buffer fails to be received...
	{
		delete[] buffer; //delete buffer to prevent memory leak
		return false; //return false: Fails to receive string buffer
	}
	_string = buffer; //set string to received buffer message
	delete[] buffer; //Deallocate buffer memory (cleanup to prevent memory leak)
	return true;//Return true if we were successful in retrieving the string
}
bool HandleSendFile();

void SendFile(string& filename)
{

	File.file.open(filename, ios::binary | ios::ate);
	if (!File.file.is_open())
	{
		cout << "Cannot open file for reading";
		return;
	}
	if (!SendPacketType(P_FileTransferRequest))
		return;
	if (!SendString(filename, false))
		cout << "cannot send file name";
	File.filename = filename;
	File.filesize = File.file.tellg();
	File.file.seekg(0);
	File.bytescounter = 0;
	return;
}

bool RequestFile(std::string FileName)
{
	File.outfile.open(FileName, std::ios::binary); //open file to write file to
	File.filename = FileName; //save file name
	File.bytesWritten = 0; //reset byteswritten to 0 since we are working with a new file
	if (!File.outfile.is_open()) //if file failed to open...
	{
		std::cout << "ERROR: Function(Client::RequestFile) - Unable to open file: " << FileName << " for writing.\n";
		return false;
	}
	std::cout << "Requesting file from server: " << FileName << std::endl;
	if (!SendPacketType(P_one)) //send file transfer request packet
		return false;
	if (!SendString(FileName, false)) //send file name
		return false;
	return true;
}

bool HandleSendFile()
{
	if (File.bytescounter >= File.filesize) //If end of file reached then return true and skip sending any bytes
		return true;
	if (!SendPacketType(P_FileTransferByteBuffer)) //Send packet type for file transfer byte buffer
		return false;

	File.remaining_bytes = File.filesize - File.bytescounter; //calculate remaining bytes
	if (File.remaining_bytes > File.buffersize) //if remaining bytes > max byte buffer
	{
		File.file.read(File.buffer, File.buffersize); //read in max buffer size bytes
		if (!Sendint32_t(File.buffersize)) //send int of buffer size
			return false;
		if (!sendall(File.buffer, File.buffersize)) //send bytes for buffer
			return false;
		std::cout << "sent byte buffer for  of size: " << File.buffersize << std::endl;
		File.bytescounter += File.buffersize; //increment fileoffset by # of bytes written
	}
	else
	{
		File.file.read(File.buffer, File.remaining_bytes); //read in remaining bytes
		if (!Sendint32_t(File.remaining_bytes)) //send int of buffer size
			return false;
		if (!sendall(File.buffer, File.remaining_bytes)) //send bytes for buffer
			return false;
		std::cout << "sent byte buffer for  of size: " << File.remaining_bytes << std::endl;
		File.bytescounter += File.remaining_bytes; //increment fileoffset by # of bytes written
	}

	if (File.bytescounter == File.filesize) //If we are at end of file
	{
		if (!SendPacketType(P_FileTransfer_EndOfFile)) //Send end of file packet
			return false;
		//Print out data on server details about file that was sent

		std::cout << std::endl << "File sent: " << File.filename << std::endl;
		std::cout << "File size(bytes): " << File.filesize << std::endl << std::endl;
	}
	return true;
}



bool ProcessPacket(Packet _packettype)
{

	switch (_packettype)
	{
	case P_ChatMessage: //If packet is a chat message packet
	{
		std::string Message; //string to store our message we received
		if (!GetString(Message)) //Get the chat message and store it in variable: Message
			return false; //If we do not properly get the chat message, return false
		std::cout << "                         " << Message << std::endl; //Display the message to the user
		break;
	}

	case P_FileTransferByteBuffer:
	{
		if (!HandleSendFile())
			return false;

		break;
	}

	case P_one:
	{
		int32_t buffersize; //buffer to hold size of buffer to write to file
		if (!Getint32_t(buffersize)) //get size of buffer as integer
			return false;
		if (!recvall(File.buffer, buffersize)) //get buffer and store it in file.buffer
		{
			return false;
		}
		File.outfile.write(File.buffer, buffersize); //write buffer from file.buffer to our outfilestream
		File.bytesWritten += buffersize; //increment byteswritten
		std::cout << "Received byte buffer for file transfer of size: " << buffersize << std::endl;
		if (!SendPacketType(P_two)) //send packet type to request next byte buffer (if one exists)
			return false;
		break;
	}
	case P_FileTransfer_EndOfFile:
	{
		std::cout << "File transfer completed. File received." << std::endl;
		std::cout << "File Name: " << File.filename << std::endl;
		std::cout << "File Size(bytes): " << File.bytesWritten << std::endl;
		File.bytesWritten = 0;
		File.outfile.close(); //close file after we are done writing file
		break;
	}

	case P_three:
	{  SendFile(reqfile);
	break;
	}
	default: //If packet type is not accounted for
		std::cout << "Unrecognized packet: " << _packettype << std::endl; //Display that packet was not found
		break;
	}
	return true;
}





void ClientThread()
{
	Packet PacketType;
	while (true)
	{
		if (!GetPacketType(PacketType)) //Get packet type
			break; //If there is an issue getting the packet type, exit this loop
		if (!ProcessPacket(PacketType)) //Process packet (packet type)
			break; //If there is an issue processing the packet, exit this loop
	}
	std::cout << "Lost connection to the server." << std::endl;
	closesocket(Connection);
}

int main()
{
	cout << "PLEASE ENTER YOUR NAME - ";
	getline(cin, your_name);
	string IpAddress;
	cout << "ENTER IP ADDRESS OF SERVER - ";
	getline(cin, IpAddress);
	reqfile = your_name + " CHAT MESSAGE ANALYSIS.txt";
	//Winsock Startup
	WSAData wsaData;
	WORD DllVersion = MAKEWORD(2, 1);
	if (WSAStartup(DllVersion, &wsaData) != 0)
	{
		MessageBoxA(NULL, "Winsock startup failed", "Error", MB_OK | MB_ICONERROR);
		return 0;
	}

	SOCKADDR_IN addr; //Address to be binded to our Connection socket
	int sizeofaddr = sizeof(addr); //Need sizeofaddr for the connect function
	addr.sin_addr.s_addr = inet_addr(IpAddress.c_str()); //Address = localhost (this pc)
	addr.sin_port = htons(1111); //Port = 1111
	addr.sin_family = AF_INET; //IPv4 Socket

	Connection = socket(AF_INET, SOCK_STREAM, NULL); //Set Connection socket
	if (connect(Connection, (SOCKADDR*)&addr, sizeofaddr) != 0) //If we are unable to connect...
	{
		MessageBoxA(NULL, "Failed to Connect", "Error", MB_OK | MB_ICONERROR);
		return 0; //Failed to Connect
	}

	std::cout << "Connected! To Server" << std::endl;
	ofstream file;


	CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)ClientThread, NULL, NULL, NULL); //Create the client thread that will receive any data that the server sends.

	std::string userinput; //holds the user's chat message

	while (true)
	{
		std::getline(std::cin, userinput); //Get line if user presses enter and fill the buffer
		if (strcmp("SEND FILE", userinput.c_str()) == 0)
		{
			cout << "Enter file name  - ";
			getline(cin, filename);

			SendFile(filename);

		}
		else if (strcmp("REQUEST FILE", userinput.c_str()) == 0)
		{
			cout << "Enter file  name - ";
			getline(cin, filename);
			RequestFile(filename);
		}

		else {
			if (!SendString(userinput)) //Send string: userinput, If string fails to send... (Connection issue)
				break;
			Sleep(10);
		}
		file.open(reqfile, ios::app);
		if (!file.is_open())
			cout << "Cannot open file for writng chat messages";
		file << userinput << endl;
		file.close();
	}
	return 0;
}

