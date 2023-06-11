#pragma once
#include<string.h>
#include<iostream>
#include<winsock2.h>
#include<stdlib.h>
#include<stdio.h>
#include <sys/types.h>
#include <winsock.h>

#pragma comment(lib,"ws2_32.lib")  

using namespace std;

#define MAX_BUF_LEN 1024	//最长缓冲区长度
#define MAX_PKT_LEN 512		//最长报文长度
#define MAX_LOST_PKT 5		//最大丢包数
#define WAITING_TIME 10		//Sleep等待的毫秒数
#define PKT_TIMEOUT 600		//用于重复等待计数器最大数值
#define SEND_AGAIN 200		//重传计数器重传间隔

class tftp
{
public:
	int init();//初始化函数
	int download(char* remotefilename,char* filename);//下载调用
	int upload(char* filename);//上载调用

	void write_log();//记录日志

	void error_ack(char* error_buf);//错误包处理
private:
	WSADATA wsadata;//wsa数据结构
	SOCKADDR_IN client_ip, server_ip;//ip地址结构
	SOCKET client_socket, server_socket;//Socket结构
	unsigned long Opt;//非阻塞模式设置使用
	int transfer_mode;//标记传输模式

	char packet[MAX_BUF_LEN];//数据包缓冲区
	int packetlen = 0;//数据包长度计数器

	FILE* log;//日志文件指针
	char log_buf[MAX_BUF_LEN];//日志条目缓冲区
	SYSTEMTIME st;//系统时间结构体
	clock_t start, end;//开始结束时间记录
	double transByte, consumeTime;//传输数据量、时间记录
};
