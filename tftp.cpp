#include "tftp.h"

int tftp::init() {
	//启用WSA
	if (WSAStartup(0x0101, &wsadata)) {
		cout << "WSAStartup error!";
		return -1;
	}
	if (wsadata.wVersion != 0x0101) {
		cout << "Version error!";
		return -1;
	}

	//接收服务器ip
	char c_ip[20]="10.12.173.15", s_ip[20]="10.12.173.15";
	cout << "Input Client IP : ";
	cin >> c_ip;
	cout << "Input Server IP : ";
	cin >> s_ip;
	getchar();

	//初始化socketaddr_in结构体
	client_ip.sin_family = AF_INET;
	client_ip.sin_addr.S_un.S_addr = inet_addr(c_ip);
	client_ip.sin_port = htons(5000);
	server_ip.sin_family = AF_INET;
	server_ip.sin_addr.S_un.S_addr = inet_addr(s_ip);
	server_ip.sin_port = htons(69);

	//错误处理
	client_socket = socket(AF_INET, SOCK_DGRAM, 0);
	if (client_socket == INVALID_SOCKET) {
		cout << "create socket failed!"<<endl;
		WSACleanup();
		return -2;
	}
	cout << "client created socket" << endl;

	//设置为非阻塞模式
	ioctlsocket(client_socket, FIONBIO, &Opt);

	/*if (bind(client_socket, (struct sockaddr*)&client_ip, sizeof(client_ip)) == SOCKET_ERROR) {
		cout << "bind socket failed!" << endl;
		WSACleanup();
		return -3;
	}
	cout << "client bound socket" << endl;*/

	//打开日志
	if (!(log = fopen("tftp.txt", "w+"))) {
		cout << "cannot create log file" << endl;
		return -4;
	};

	//记录日志
	GetLocalTime(&st);
	sprintf(log_buf, "%d-%d-%d %02d:%02d:%02d:%03d\tCreate socket connect to %s\n", \
		st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,\
		s_ip);
	write_log();

	return 1;
}

int tftp::upload(char* filename) {
	//传输模式选择
	cout << "please select transfer mode:1.netascii 2.octet" << endl;
	char buf[MAX_BUF_LEN];
	gets_s(buf, sizeof(buf));
	FILE* sendfile;

	//日志记录
	GetLocalTime(&st);
	sprintf(log_buf, "%d-%d-%d %02d:%02d:%02d:%03d\tupload file %s", \
		st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, \
		filename);
	if (buf[0] == '1') {
		transfer_mode=1;
		sendfile = fopen(filename, "r");
		strcat(log_buf, " transfer mode: netascii\n");
	}
	else if (buf[0] == '2') {
		transfer_mode=2;
		sendfile = fopen(filename, "rb");
		strcat(log_buf, " transfer mode: octet\n");
	}
	else {
		cout << "invalid expression" << endl;
		return -1;
	}
	if (!sendfile) {
		cout << "cannot open this file:" << filename << endl;
		return -2;
	}
	write_log();

	//WRQ构建
	packetlen = 0;
	memset(packet, 0, sizeof(packet));
	packet[1] = 2;
	packetlen += 2;
	char* name = filename;

	//文件名处理
	for (int i = 0; filename[i] != '\0'; i++) {
		if (filename[i] == '\\')name = filename + i + 1;
	}

	strcpy(packet + packetlen, name);
	packetlen += strlen(name) + 1;

	if (transfer_mode == 1) {
		strcpy(packet + packetlen, "netascii");
		packetlen += strlen("netascii") + 1;
	}
	else if (transfer_mode == 2) {
		strcpy(packet + packetlen, "octet");
		packetlen += strlen("octet") + 1;
	}
	server_ip.sin_port = htons(69);
	//发送报文
	sendto(client_socket, packet, packetlen, 0, (struct sockaddr*)&server_ip, sizeof(sockaddr_in));
	
	//ACK判断
	sockaddr_in server_back;
	int sizeof_server_back= sizeof(server_back);
	int waitingtimes = 0, size = 0, lost_pkt = 0;
	unsigned short* temp;
	for (waitingtimes = 0,size = 0 ; waitingtimes < PKT_TIMEOUT; waitingtimes++){
		size = recvfrom(client_socket, buf, sizeof(buf), 0, (struct sockaddr*)&server_back, &sizeof_server_back);
		temp = (unsigned short*)(&buf[2]);
		if (size >= 4 && buf[1] == 4 && *temp == htons(0)) {
			//收到即结束循环
			break;
		}
		if (waitingtimes % SEND_AGAIN == SEND_AGAIN-1) {
			//到达预设次数，尝试重传并记录
			sendto(client_socket, packet, packetlen, 0, (struct sockaddr*)&server_ip, sizeof(sockaddr_in));
			GetLocalTime(&st);
			sprintf(log_buf, "%d-%d-%d %02d:%02d:%02d:%03d\tTIMEOUT send first WRQ packet again!\n", \
				st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
			write_log();
		}
		Sleep(WAITING_TIME);
	}
	if (waitingtimes >= PKT_TIMEOUT) {
		//超时处理
		cout << "could not recieve from server!" << endl;
		GetLocalTime(&st);
		sprintf(log_buf, "%d-%d-%d %02d:%02d:%02d:%03d\tupload %s failed!\tCannot recieve from server\n", \
			st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, \
			filename);
		write_log();
		return -3;
	}

	//发送数据包
	short block_num = 1;
	int read_size = 0;
	int flag=0;
	transByte = 0;
	start = clock();
	do {
		//构造本轮发送的数据包
		memset(packet, 0, sizeof(packet));
		packetlen = 0;
		packet[1] = 3;

		temp = (unsigned short*) & packet[2];
		*temp = htons(block_num);
		packetlen += 4;
		read_size = fread(&packet[4], 1, MAX_PKT_LEN, sendfile);//读入
		transByte += read_size;//记录传输的总数据量
		packetlen += read_size;//记录本次传输的数据量
		//发送数据包
		sendto(client_socket, packet, packetlen, 0, (struct sockaddr*)&server_back, sizeof(sockaddr_in));

		for (waitingtimes = 0, size = 0; waitingtimes < PKT_TIMEOUT; waitingtimes++) {
			size = recvfrom(client_socket, buf, sizeof(buf), 0, (struct sockaddr*)&server_back, &sizeof_server_back);
			temp = (unsigned short*)(&buf[2]);
			if (size >= 4 && buf[1] == 4 && *temp == htons(block_num)) {
				//收到预期ACK跳出循环
				cout << "recieve " << block_num << " packet!" << endl;
				break;
			}
			if (size >= 4 && buf[1] == 5) {
				//收到错误的报文，处理之
				error_ack(buf);
				return -4;
			}
			if (waitingtimes % SEND_AGAIN == SEND_AGAIN - 1) {
				//到达预设次数，尝试重传并记录
				sendto(client_socket, packet, packetlen, 0, (struct sockaddr*)&server_back, sizeof(sockaddr_in));
				GetLocalTime(&st);
				sprintf(log_buf, "%d-%d-%d %02d:%02d:%02d:%03d\tTIMEOUT send %d data packet again!\n", \
					st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, \
					block_num);
				write_log();
			}
			Sleep(WAITING_TIME);
		}

		if (waitingtimes >= PKT_TIMEOUT) {
			//仍然超时进入最后的尝试
			while (!flag && lost_pkt <= MAX_LOST_PKT) {
				lost_pkt++;
				for (waitingtimes = 0, size = 0; waitingtimes < PKT_TIMEOUT; waitingtimes++) {
					size = recvfrom(client_socket, buf, sizeof(buf), 0, (struct sockaddr*)&server_back, &sizeof_server_back);
					temp = (unsigned short*)(&buf[2]);
					if (size >= 4 && buf[1] == 4 && *temp == htons(block_num)) {
						cout << "recieve " << block_num << " ack packet!" << endl;
						flag = 1;
						break;
					}
					if (size >= 4 && buf[1] == 5) {
						//错误处理
						error_ack(buf);
						return -4;
					}
					if (waitingtimes % SEND_AGAIN == SEND_AGAIN-1) {
						//重传
						sendto(client_socket, packet, packetlen, 0, (struct sockaddr*)&server_back, sizeof(sockaddr_in));
						GetLocalTime(&st);
						sprintf(log_buf, "%d-%d-%d %02d:%02d:%02d:%03d\tTIMEOUT send %d data packet again!\n", \
							st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, \
							block_num);
						write_log();
					}
					Sleep(WAITING_TIME);
				}
			}
			//超时，结束，记录日志
			if (lost_pkt > MAX_LOST_PKT) {
				cout << "could not recieve from server!" << endl;
				GetLocalTime(&st);
				sprintf(log_buf, "%d-%d-%d %02d:%02d:%02d:%03d\tupload %s failed!\tCannot recieve from server\n", \
					st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, \
					filename);
				write_log();
				return -3;
			}
		}

		block_num++;//计数器自增
		lost_pkt = 0;
		flag = 0;
	} while (read_size == MAX_PKT_LEN);//如果不相等，则到达了最后的报文
	end = clock();
	//计算传输时间、传输速率并记录到日志
	consumeTime = ((double)(end - start)) / CLK_TCK;
	cout << "upload successfully!" << endl;
	cout << "file size: " << transByte << " Bytes" << " time: " << consumeTime << " s" << endl;
	cout << "upload speed:" << transByte / consumeTime / 1024 << " kB/s" << endl;
	fclose(sendfile);
	GetLocalTime(&st);
	sprintf(log_buf, "%d-%d-%d %02d:%02d:%02d:%03d\tupload %s complete!\tsize:%.0lf\tBytes time:%.2lfs\tupload speed:%.2lfkB/s\n", \
		st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, \
		filename, transByte, consumeTime, transByte / consumeTime / 1024);
	write_log();
	return 1;
}

int tftp::download(char* remotefilename, char* filename) {
	//选择传输模式
	cout << "please select transfer mode:1.netascii 2.octet" << endl;
	char buf[MAX_BUF_LEN];
	gets_s(buf, sizeof(buf));

	//日志记录
	GetLocalTime(&st);
	sprintf(log_buf, "%d-%d-%d %02d:%02d:%02d:%03d\tdownload file %s to local %s", \
		st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, \
		remotefilename, filename);
	FILE* sendfile;
	if (buf[0] == '1') {
		transfer_mode = 1;
		sendfile = fopen(filename, "w");
		strcat(log_buf, " transfer mode: netascii\n");
	}
	else if (buf[0] == '2') {
		transfer_mode = 2;
		sendfile = fopen(filename, "wb");
		strcat(log_buf, " transfer mode: octet\n");
	}
	else {
		cout << "invalid expression" << endl;
		return -1;
	}
	if (!sendfile) {
		cout << "cannot create this file:" << filename << endl;
		return -2;
	}
	write_log();

	//RRQ构造发送
	packetlen = 0;
	memset(packet, 0, sizeof(packet));
	packet[1] = 1;
	packetlen += 2;
	char* name = remotefilename;
	for (int i = 0; remotefilename[i] != '\0'; i++) {
		if (remotefilename[i] == '\\')name = remotefilename + i + 1;
	}

	strcpy(packet + packetlen, name);
	packetlen += strlen(name) + 1;

	if (transfer_mode == 1) {
		strcpy(packet + packetlen, "netascii");
		packetlen += strlen("netascii") + 1;
	}
	else if (transfer_mode == 2) {
		strcpy(packet + packetlen, "octet");
		packetlen += strlen("octet") + 1;
	}
	server_ip.sin_port = htons(69);
	//发送下载请求
	sendto(client_socket, packet, packetlen, 0, (struct sockaddr*)&server_ip, sizeof(sockaddr_in));

	//收包
	//初始化
	sockaddr_in server_back=server_ip;
	int sizeof_server_back = sizeof(server_back);
	int waitingtimes = 0, size = 0;
	unsigned short* temp;

	short block_num = 1;
	int read_size = 0;
	transByte = 0;
	start = clock();
	while (1) {
		for (waitingtimes = 0, size = 0; waitingtimes < PKT_TIMEOUT; waitingtimes++) {
			read_size = recvfrom(client_socket, buf, sizeof(buf), 0, (struct sockaddr*)&server_back, &sizeof_server_back);
			read_size -= 4;
			transByte += read_size;
			temp = (unsigned short*)(&buf[2]);
			if (read_size >= 0 && buf[1] == 3 && *temp == htons(block_num)) {
				//收到预期报文
				cout << "recieve " << block_num << " packet!" << endl;
				break;
			}
			if (read_size >= 0 && buf[1] == 5) {
				//收到错误信息报文
				error_ack(buf);
				return -4;
			}
			if (waitingtimes % SEND_AGAIN == SEND_AGAIN-1) {
				//到达预设次数，尝试重传并记录
				sendto(client_socket, packet, packetlen, 0, (struct sockaddr*)&server_back, sizeof(sockaddr_in));
				GetLocalTime(&st);
				if(block_num==1)
					sprintf(log_buf, "%d-%d-%d %02d:%02d:%02d:%03d\tTIMEOUT send RRQ packet again!\n", \
						st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, \
						block_num);
				else
					sprintf(log_buf, "%d-%d-%d %02d:%02d:%02d:%03d\tTIMEOUT send %d ACK packet again!\n", \
						st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, \
						block_num);
				write_log();
			}
			Sleep(WAITING_TIME);
		}
		if (waitingtimes >= PKT_TIMEOUT) {
			//超时，对本次活动进行记录
			cout << "could not recieve from server!" << endl;
			GetLocalTime(&st);
			sprintf(log_buf, "%d-%d-%d %02d:%02d:%02d:%03d\tdownload %s failed!Cannot recieve from server\n", \
				st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, \
				filename);
			write_log();
			return -3;
		}
		memset(packet, 0, sizeof(packet));
		packet[1] = 4;
		packetlen = 4;
		fwrite(&buf[4], 1, read_size, sendfile);
		packet[2] = buf[2];
		packet[3] = buf[3];
		sendto(client_socket, packet, packetlen, 0, (struct sockaddr*)&server_back, sizeof(sockaddr_in));//发送ACk给服务器
		if (read_size != MAX_PKT_LEN)break;
		block_num++;//计数器自增
	}
	end = clock();
	//计算传输速率、传输时间
	consumeTime = ((double)(end - start)) / CLK_TCK;
	cout << "download successfully!" << endl;
	cout << "file size: " << transByte << " Bytes" << " time: " << consumeTime << " s" << endl;
	cout << "upload speed:" << transByte / consumeTime / 1024 << " kB/s" << endl;
	fclose(sendfile);
	GetLocalTime(&st);
	sprintf(log_buf, "%d-%d-%d %02d:%02d:%02d:%03d\tdownload %s complete!\tsize:%.0lf\tBytes time:%.2lfs\tupload speed:%.2lfkB/s\n", \
		st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, \
		filename, transByte, consumeTime, transByte / consumeTime / 1024);
	write_log();
	return 1;
}

void tftp::write_log() {
	//向日志文件输出日志
	fprintf(log,"%s",log_buf);
	return;
}

void tftp::error_ack(char* error_buf) {
	//读取错误报文信息并输出到日志中
	GetLocalTime(&st);
	sprintf(log_buf, "%d-%d-%d %02d:%02d:%02d:%03d\terrorcode:%c\terror_msg:%s\n", \
		st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, \
		error_buf[3]+'0', &error_buf[4]);
	write_log();
	printf("error:%s\n", &error_buf[4]);
	return;
}