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

#define MAX_BUF_LEN 1024	//�����������
#define MAX_PKT_LEN 512		//����ĳ���
#define MAX_LOST_PKT 5		//��󶪰���
#define WAITING_TIME 10		//Sleep�ȴ��ĺ�����
#define PKT_TIMEOUT 600		//�����ظ��ȴ������������ֵ
#define SEND_AGAIN 200		//�ش��������ش����

class tftp
{
public:
	int init();//��ʼ������
	int download(char* remotefilename,char* filename);//���ص���
	int upload(char* filename);//���ص���

	void write_log();//��¼��־

	void error_ack(char* error_buf);//���������
private:
	WSADATA wsadata;//wsa���ݽṹ
	SOCKADDR_IN client_ip, server_ip;//ip��ַ�ṹ
	SOCKET client_socket, server_socket;//Socket�ṹ
	unsigned long Opt;//������ģʽ����ʹ��
	int transfer_mode;//��Ǵ���ģʽ

	char packet[MAX_BUF_LEN];//���ݰ�������
	int packetlen = 0;//���ݰ����ȼ�����

	FILE* log;//��־�ļ�ָ��
	char log_buf[MAX_BUF_LEN];//��־��Ŀ������
	SYSTEMTIME st;//ϵͳʱ��ṹ��
	clock_t start, end;//��ʼ����ʱ���¼
	double transByte, consumeTime;//������������ʱ���¼
};
