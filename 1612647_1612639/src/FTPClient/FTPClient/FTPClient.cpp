
#include "stdafx.h"
#include "FTPClient.h"

#pragma comment(lib, "Ws2_32.lib")
#define MY_ERROR(s) printf(s); system("PAUSE"); exit(1);

#define MAX_SIZE 4096
//#define MY_IP "127.0.0.1"
void errexit(const char *, ...);

void replylogcode(int code)
{
	switch (code) {
	case 200:
		printf("Command okay");
		break;
	case 500:
		printf("Syntax error, command unrecognized.");
		printf("This may include errors such as command line too long.");
		break;
	case 501:
		printf("Syntax error in parameters or arguments.");
		break;
	case 202:
		printf("Command not implemented, superfluous at this site.");
		break;
	case 502:
		printf("Command not implemented.");
		break;
	case 503:
		printf("Bad sequence of commands.");
		break;
	case 530:
		printf("Not logged in.");
		break;
	}
	printf("\n");
}

//Lấy các giá trị p1, p2 từ port
void GetActiveMode(int port, int &p1, int &p2)
{
	p1 = port / 256;
	p2 = port - (p1 * 256);
}
//Tìm giá trị port
int GetPortPassiveMode(char recvBuffer[200])
{
	//227 entering passive mode (ip1,ip2,ip3,ip4,p1,p2)
	char* address;
	char* ptr;
	int result[6];
	int port;
	address = &recvBuffer[27];
	ptr = strtok(address, ",)");

	for (int i = 0; i < 6; i++)
	{
		result[i] = atoi(ptr);
		ptr = strtok(NULL, ",)");
	}
	port = result[4] * 256 + result[5];
	// dataport = p1*256 + p2
	return port;
}

bool EstablishDataChannel(SOCKET* ConnectSocket, SOCKET dataSocket, int mode) //mode 1: active, mode 2: passive
{
	char Command[50] = "\0";
	char recvBuffer[200];
	int recvBytes;
	int dataPort;
	HOSTENT *pHostEnt;

	if (mode == 1) //active
	{
		struct sockaddr_in sin;
		int len = sizeof(sin);
		getsockname(dataSocket, (struct sockaddr *)&sin, &len);
		dataPort = ntohs(sin.sin_port);
		int p1, p2;
		GetActiveMode(dataPort, p1, p2);



		/*	char Name[] = "localhost";
		pHostEnt = gethostbyname(Name);*/
		sprintf(Command, "PORT %s.%d.%d\n", "127.0.0.1", p1, p2);

		send(*ConnectSocket, Command, strlen(Command), 0);
		recvBytes = recv(*ConnectSocket, recvBuffer, 200, 0);
		recvBuffer[recvBytes] = '\0';
		cout << recvBuffer;

		if (strncmp(recvBuffer, "421", 3) == 0)
		{
			printf("Server cannot create socket error 421\n");
			return false;
		}

		struct sockaddr_in serverAddress;//dia chi server voi
		memset(&serverAddress, 0, sizeof(serverAddress));

		serverAddress.sin_family = AF_INET;
		serverAddress.sin_port = htons(20);
		//serverAddress.sin_addr.s_addr = inet_addr("199.71.215.197");

		pHostEnt = gethostbyname("localhost");
		memcpy(&sin.sin_addr, pHostEnt->h_addr_list[0], pHostEnt->h_length);


		//printf("connect()\n");
		//connect(dataSocket, (struct sockaddr *) &serverAddress, sizeof(serverAddress));
		if (connect(dataSocket, (SOCKADDR*)&serverAddress, sizeof(serverAddress)))
		{
			printf("Cannot establish data channel, error : %d\n", GetLastError());
			return false;
		}
	}
	else if (mode == 2)	//passive
	{
		strcpy(Command, "PASV\n");
		send(*ConnectSocket, Command, 5, 0);
		recvBytes = recv(*ConnectSocket, recvBuffer, 200, 0);
		recvBuffer[recvBytes] = '\0';
		

		dataPort = GetPortPassiveMode(recvBuffer);

		if (strncmp(recvBuffer, "421", 3) == 0)
		{
			printf("Server cannot create socket error 421\n");
			return false;
		}

		SOCKADDR_IN serverAddress;//dia chi server voi dataport
		memset(&serverAddress, 0, sizeof(serverAddress));

		serverAddress.sin_family = AF_INET;
		serverAddress.sin_port = htons(dataPort);
		//serverAddress.sin_addr.s_addr = inet_addr(MY_IP);
		pHostEnt = gethostbyname("localhost");
		memcpy(&serverAddress.sin_addr, pHostEnt->h_addr_list[0], pHostEnt->h_length);



		if (connect(dataSocket, (SOCKADDR*)&serverAddress, sizeof(serverAddress)))
		{
			printf("Cannot establish data channel, error : %d\n", GetLastError());
			return false;
		}
	}
	return true;
}

void dir(SOCKET* ConnectSocket, int mode)
{
	char pasvCommand[5];
	char listCommand[10];
	char recvBuffer[BUFSIZ + 1];
	char dataBuffer[MAX_SIZE + 1];
	int recvBytes;

	SOCKET dataSocket;
	dataSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	//
	//Mo kenh data 
	if (!EstablishDataChannel(ConnectSocket, dataSocket, mode))
	{
		return;
	}
	strcpy(listCommand, "LIST\n");
	send(*ConnectSocket, listCommand, strlen(listCommand), 0);

	memset(&recvBuffer, 0, sizeof(recvBuffer));
	recvBytes = recv(*ConnectSocket, recvBuffer, BUFSIZ, 0);
	recvBuffer[recvBytes] = '\0';
	cout << recvBuffer;

	memset(&recvBuffer, 0, sizeof(recvBuffer));
	recvBytes = recv(*ConnectSocket, recvBuffer, BUFSIZ, 0);
	recvBuffer[recvBytes] = '\0';
	cout << recvBuffer;

	memset(&recvBuffer, 0, sizeof(recvBuffer));
	recvBytes = recv(dataSocket, dataBuffer, MAX_SIZE, 0);
	dataBuffer[recvBytes] = '\0';
	cout << dataBuffer;

	closesocket(dataSocket);
}
void put(SOCKET* ConnectSocket, char filename[], int mode)
{

	char storCommand[100];
	char sendBuffer[MAX_SIZE + 1];
	char recvBuffer[MAX_SIZE + 1];
	int recvBytes = 0;
	int sentBytes = 0;
	int filesize;
	FILE* uploadFile;

	//dir(ConnectSocket);
	sprintf(filename, "%s", filename);
	uploadFile = fopen(filename, "rb");
	if (uploadFile == NULL)
	{
		printf("Cannot find the file path\n");
		return;
	}

	SOCKET dataSocket;
	dataSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (!EstablishDataChannel(ConnectSocket, dataSocket, mode))
	{
		return;
	}

	//tim filesize
	fseek(uploadFile, 0L, SEEK_END);
	filesize = ftell(uploadFile);
	rewind(uploadFile);

	//STOR <filename>
	sprintf(storCommand, "STOR %s\n", filename);

	send(*ConnectSocket, storCommand, strlen(storCommand), 0);
	////
	memset(&recvBuffer, 0, sizeof(recvBuffer));
	recvBytes = recv(*ConnectSocket, recvBuffer, BUFSIZ, 0);
	recvBuffer[recvBytes] = '\0';
	cout << recvBuffer;

	recvBytes = 0;
	uploadFile = fopen(filename, "rb");
	memcpy(recvBuffer, "", MAX_SIZE);


	while (sentBytes < filesize)
	{
		int read = fread(sendBuffer, 1, MAX_SIZE, uploadFile);
		if (read < 0)
		{
			break;
		}
		sendBuffer[read] = '\0';
		send(dataSocket, sendBuffer, read, 0);
		sentBytes += MAX_SIZE;
	}

	
	
	fclose(uploadFile);
	closesocket(dataSocket);
	memset(&recvBuffer, 0, sizeof(recvBuffer));
	recvBytes = recv(*ConnectSocket, recvBuffer, BUFSIZ, 0);
	recvBuffer[recvBytes] = '\0';
	cout << recvBuffer;
}
void mput(SOCKET* ConnectSocket, int mode)
{
	char filenames[512];
	cout << ">> Remote files  ";	//Remote files 
	int N;
	scanf("%d *[^\n]", &N);
	gets_s(filenames);


	//Lấy tên từng file nhập vào
	char token[100];
	int len = strlen(filenames);
	/* duyet qua cac file con lai */
	int i = 0, j = 0;
	while (i < len)
	{
		if (filenames[i] == ' ')
		{
			token[j] = NULL;
			if (strcmp(token, ""))
				put(ConnectSocket, token, mode);
			j = 0;
			i++;
			continue;
		}
		token[j] = filenames[i];
		j++;
		i++;
	}
	token[j] = NULL;

	if (strcmp(token, ""))
	{
		put(ConnectSocket, token, mode);
	}
}
void get(SOCKET* ConnectSocket, char filename[], int mode)
{
	char retrCommand[100];
	char sizeCommand[100];
	char recvBuffer[MAX_SIZE + 1];
	int recvBytes;
	int ret;
	int filesize;
	FILE* file;


	//dir(ConnectSocket);

	sprintf(sizeCommand, "SIZE %s\n", filename);
	send(*ConnectSocket, sizeCommand, strlen(sizeCommand), 0);

	recvBytes = recv(*ConnectSocket, recvBuffer, MAX_SIZE, 0);
	recvBuffer[recvBytes] = '\0';

	sscanf(recvBuffer, "%d %d", &ret, &filesize);

	SOCKET dataSocket;
	dataSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (!EstablishDataChannel(ConnectSocket, dataSocket, mode))
	{
		return;
	}

	//RETR <filename>
	sprintf(retrCommand, "RETR %s\n", filename);
	send(*ConnectSocket, retrCommand, strlen(retrCommand), 0);

	memset(&recvBuffer, 0, sizeof(recvBuffer));
	recvBytes = recv(*ConnectSocket, recvBuffer, BUFSIZ, 0);
	recvBuffer[recvBytes] = '\0';
	cout << recvBuffer;

	recvBytes = 0;
	FILE* downloadFile;
	downloadFile = fopen(filename, "wb");
	memcpy(recvBuffer, "", MAX_SIZE);

	while (recvBytes < filesize)
	{
		int write = recv(dataSocket, recvBuffer, MAX_SIZE, 0);
		recvBuffer[write] = '\0';
		fwrite(recvBuffer, 1, write, downloadFile);
		recvBytes += MAX_SIZE;
	}

	memset(&recvBuffer, 0, sizeof(recvBuffer));
	recvBytes = recv(*ConnectSocket, recvBuffer, BUFSIZ, 0);
	recvBuffer[recvBytes] = '\0';
	cout << recvBuffer;

	fclose(downloadFile);
	closesocket(dataSocket);
}

void mget(SOCKET* ConnectSocket, int mode)

{
	char filenames[512];
	cout << ">> Remote files  ";	//Remote files 
	int N;
	scanf("%d *[^\n]", &N);
	gets_s(filenames);


	//Lấy tên từng file nhập vào
	char token[100];
	int len = strlen(filenames);
	/* duyet qua cac file con lai */
	int i = 0, j = 0;
	while (i < len)
	{
		if (filenames[i] == ' ')
		{
			token[j] = NULL;
			if (strcmp(token, ""))
				get(ConnectSocket, token, mode);
			j = 0;
			i++;
			continue;
		}
		token[j] = filenames[i];
		j++;
		i++;
	}
	token[j] = NULL;

	if (strcmp(token, ""))
	{
		get(ConnectSocket, token, mode);
	}
}
void cd(SOCKET* ConnectSocket, char directoryname[])
{
	char cdCommand[200];

	char recvBuffer[512];
	int recvBytes;

	//DELE <filename>
	sprintf(cdCommand, "CWD %s\n", directoryname);

	send(*ConnectSocket, cdCommand, strlen(cdCommand), 0);
	recvBytes = recv(*ConnectSocket, recvBuffer, 512, 0);
	recvBuffer[recvBytes] = '\0';
	//nhan ket qua tu server 

	cout << recvBuffer;
}
void lcd()
{
	char path[100];
	cout << "Directory Remote ";
	cin >> path;
	int rc = chdir(path);
	if (rc < 0) {
		// TODO: handle error. use errno to determine problem
		cout << "Not found\n";
	}
	else {
		cout << "Local directory now ";
		system("cd");
	}
}
void deletef(SOCKET* ConnectSocket, char filename[])
{
	char deleteCommand[200];

	char recvBuffer[512];
	int recvBytes;

	//DELE <filename>
	sprintf(deleteCommand, "DELE %s\n", filename);

	send(*ConnectSocket, deleteCommand, strlen(deleteCommand), 0);
	recvBytes = recv(*ConnectSocket, recvBuffer, 512, 0);
	recvBuffer[recvBytes] = '\0';
	//nhan ket qua tu server 
	cout << recvBuffer;
}
void mdeletef(SOCKET* ConnectSocket)
{
	char filenames[512];
	cout << ">> Remote files  ";	//Remote files 
	int N;
	scanf("%d *[^\n]", &N);
	gets_s(filenames);


	//Lấy tên từng file nhập vào
	char token[100];
	int len = strlen(filenames);
	/* duyet qua cac file con lai */
	int i = 0, j = 0;
	while (i < len)
	{
		if (filenames[i] == ' ')
		{
			token[j] = NULL;
			if (strcmp(token, ""))
				deletef(ConnectSocket, token);
			j = 0;
			i++;
			continue;
		}
		token[j] = filenames[i];
		j++;
		i++;
	}
	token[j] = NULL;

	if (strcmp(token, ""))
	{
		deletef(ConnectSocket, token);
	}
}
void mkdir(SOCKET* ConnectSocket, char filename[])
{
	char mkdirCommand[200];

	char recvBuffer[512];
	int recvBytes;

	//DELE <filename>
	sprintf(mkdirCommand, "MKD %s\n", filename);

	send(*ConnectSocket, mkdirCommand, strlen(mkdirCommand), 0);
	recvBytes = recv(*ConnectSocket, recvBuffer, 512, 0);
	recvBuffer[recvBytes] = '\0';
	//nhan ket qua tu server 
	cout << recvBuffer;
}
void rmdir(SOCKET* ConnectSocket, char directoryname[])
{
	char deleteCommand[20];

	char recvBuffer[512];
	int recvBytes;

	//rmdir <directoryname>
	sprintf(deleteCommand, "XRMD %s\n", directoryname);

	send(*ConnectSocket, deleteCommand, strlen(deleteCommand), 0);
	recvBytes = recv(*ConnectSocket, recvBuffer, 512, 0);
	recvBuffer[recvBytes] = '\0';
	//nhan ket qua tu server 
	cout << recvBuffer;
}
void pwd(SOCKET* ConnectSocket)
{
	char mkdirCommand[] = "PWD\n";

	char recvBuffer[512];
	int recvBytes;


	send(*ConnectSocket, mkdirCommand, strlen(mkdirCommand), 0);
	recvBytes = recv(*ConnectSocket, recvBuffer, MAX_SIZE, 0);
	recvBuffer[recvBytes] = '\0';
	cout << recvBuffer;
}

void quit(SOCKET* ConnectSocket)
{
	char mkdirCommand[] = "QUIT\n";

	char recvBuffer[512];
	int recvBytes;

	send(*ConnectSocket, mkdirCommand, strlen(mkdirCommand), 0);
	recvBytes = recv(*ConnectSocket, recvBuffer, MAX_SIZE, 0);
	recvBuffer[recvBytes] = '\0';
	cout << recvBuffer << endl;
}

int _tmain(int argc, char* argv[])
{
Main_Begin:
	WORD wVersionRequested;
	WSADATA wsaData;
	int retcode;

	SOCKET socket_connect;
	int mode = 2; //Mode 1: active, 2: passive

	char ServerName[64];
	HOSTENT *pHostEnt;
	struct sockaddr_in sin;	// serverAddress


	char Buffer[MAX_SIZE]; //buf
	int length; //bytesRead


				//printf("WSAStartup()\n");

	wVersionRequested = MAKEWORD(2, 2);	// Use MAKEWORD(1,1) if you're at WinSock 1.1
	retcode = WSAStartup(wVersionRequested, &wsaData);
	if (retcode != 0)
		errexit("Startup failed: %d\n", retcode);



	if (LOBYTE(wsaData.wVersion) != LOBYTE(wVersionRequested) ||
		HIBYTE(wsaData.wVersion) != HIBYTE(wVersionRequested))
	{
		printf("Supported version is too low\n");
		WSACleanup();
		return 0;
	}


	//printf("socket()\n");

	socket_connect = socket(PF_INET, SOCK_STREAM, 0);
	if (socket_connect == INVALID_SOCKET)
		errexit("Socket creation failed: %d\n", WSAGetLastError());


	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(21);

	if (argc > 1) strcpy(ServerName, argv[1]);
	else strcpy(ServerName, "localhost");


	//printf("gethostbyname(\"%s\")\n", ServerName);
	if (pHostEnt = gethostbyname(ServerName)) {
		memcpy(&sin.sin_addr, pHostEnt->h_addr_list[0], pHostEnt->h_length);
	}
	else errexit("Can't get %s\" host entry: %d\n", ServerName, WSAGetLastError());


	//printf("connect()\n");

	retcode = connect(socket_connect, (struct sockaddr *) &sin, sizeof(sin));
	if (retcode == SOCKET_ERROR)
		errexit("Connect: (%d) Connection refused \n", WSAGetLastError());





	char buf[BUFSIZ + 1];
	int tmpres, size, status;

	char * str;
	int codeftp;
	printf("Connection established, waiting for welcome message...\n");
	//How to know the end of welcome message: http://stackoverflow.com/questions/13082538/how-to-know-the-end-of-ftp-welcome-message
	memset(buf, 0, sizeof buf);
	while ((tmpres = recv(socket_connect, buf, BUFSIZ, 0)) > 0) {
		sscanf(buf, "%d", &codeftp);
		printf("%s", buf);
		if (codeftp != 220) //120, 240, 421: something wrong
		{
			replylogcode(codeftp);
			exit(1);
		}

		str = strstr(buf, "220");//Why ???
		if (str != NULL) {
			break;
		}
		memset(buf, 0, tmpres);
	}

	while (true)
	{
		//Send Username
		char info[50];//
		printf("User (%s): ", inet_ntoa(sin.sin_addr));
		memset(buf, 0, sizeof buf);
		scanf("%s", info);

		sprintf(buf, "USER %s\r\n", info);
		tmpres = send(socket_connect, buf, strlen(buf), 0);

		memset(buf, 0, sizeof buf);
		tmpres = recv(socket_connect, buf, BUFSIZ, 0);

		sscanf(buf, "%d", &codeftp);
		if (codeftp != 331)
		{
			replylogcode(codeftp);
			exit(1);
		}
		printf("%s", buf);

		//Send Password
		memset(info, 0, sizeof info);
		printf("Password: ");
		memset(buf, 0, sizeof buf);
		scanf("%s", info);

		sprintf(buf, "PASS %s\r\n", info);
		tmpres = send(socket_connect, buf, strlen(buf), 0);

		memset(buf, 0, sizeof buf);
		tmpres = recv(socket_connect, buf, BUFSIZ, 0);

		printf("%s", buf);
		sscanf(buf, "%d", &codeftp);
		if (codeftp != 230)
		{
			replylogcode(codeftp);
			continue;
		}


		//command
		//Cac lenh ftp
		char filename[100];
		int choice;
		while (true)
		{
			cout << endl << endl << "\t- - - + - + - + - - - MENU - - - + - + - + - - -" << endl << endl;
			cout << setiosflags(ios::left)
				<< setw(30) << "\t\t1.ls, dir"
				<< setw(30) << "8.delete" << endl
				<< setw(30) << "\t\t2.put"
				<< setw(30) << "9.mdelete" << endl
				<< setw(30) << "\t\t3.get"
				<< setw(30) << "10.mkdir" << endl
				<< setw(30) << "\t\t4.mput"
				<< setw(30) << "11.rmkdir" << endl
				<< setw(30) << "\t\t5.mget"
				<< setw(30) << "12.pwd" << endl
				<< setw(30) << "\t\t6.cd";
			cout << setw(30) << ((mode == 1) ? "13.passive" : "13.active") << endl;
			cout << setiosflags(ios::left)
				<< setw(30) << "\t\t7.lcd"
				<< setw(30) << "14.Quit" << endl;

			cout << endl << ">> Choose: ";
			cin >> choice;
			switch (choice)
			{
			case 1:
			{
				dir(&socket_connect, mode);
			}
			break;
			case 2:
			{
				cout << ">> File remote ";
				scanf("%s", filename);
				put(&socket_connect, filename, mode);
			}
			break;
			case 3:
			{
				cout << ">> File remote ";
				scanf("%s", filename);
				get(&socket_connect, filename, mode);
			}
			break;
			case 4:
				mput(&socket_connect, mode);
				break;
			case 5:
				mget(&socket_connect, mode);
				break;
			case 6:
			{
				char directoryname[100];
				cout << ">> Remote directory: ";
				cin >> directoryname;
				cd(&socket_connect, directoryname);
			}
			break;
			case 7:
				lcd();
				break;
			case 8:
			{
				char filename[100];
				cout << ">> Remote file: ";
				cin >> filename;
				deletef(&socket_connect, filename);
			}
			break;
			case 9:
				mdeletef(&socket_connect);
				break;
			case 10:
			{
				char filename[100];
				cout << ">> Directory name: ";
				cin >> filename;
				mkdir(&socket_connect, filename);
			}
			break;
			case 11:
			{
				char filename[100];
				cout << ">> Directory name: ";
				cin >> filename;
				rmdir(&socket_connect, filename);
			}

			break;
			case 12:
				pwd(&socket_connect);
				break;
			case 13:
				//Change mode 
			{
				if (mode == 1)
					mode = 2;
				else if (mode == 2)
					mode = 1;
				if (mode == 1)
					cout << "Mode: Active";
				else if (mode == 2)
					cout << "Mode: Passive";

			}
				break;
			case 14:
			{
				quit(&socket_connect);
				goto Main_Begin;
			}
			}

		}
	}


	//printf("closesocket()\n");

	retcode = closesocket(socket_connect);
	if (retcode == SOCKET_ERROR)
		errexit("Close socket failed: %d\n", WSAGetLastError());



	//printf("WSACleanup()\n");

	retcode = WSACleanup();
	if (retcode == SOCKET_ERROR)
		errexit("Cleanup failed: %d\n", WSAGetLastError());


	return 0;
}



void errexit(const char *format, ...)
{
	va_list	args;

	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
	WSACleanup();
	exit(1);
}




