#include "tftp.h"

int main()
{
	cout << "***tftp client Version1.0***" << endl;

    tftp active;
    
	//初始化Socket
    if (active.init() != 1)return -1;

	//命令行界面
    cout << "supporting command : upload download exit" << endl;
	cout << "\nExamples:\nupload [filename]\ndownload [remotefilename] [filename]\nexit\n" << endl;
	char* arg1, *arg2, *arg3;

	//命令提示符
    while (1) {
        cout << "tftp>";
        char input[MAX_BUF_LEN];
        gets_s(input);

		//截取arg1指令，分类进入函数
		arg1 = strtok(input, " ");
		if (arg1 == NULL) {
			cout << "please input correct command" << endl;
			continue;
		}
		if (!strcmp(arg1, "upload")) {
			arg2 = strtok(NULL, " ");
			if (arg2 != NULL) {
				active.upload(arg2);
			}
			else {
				cout << "command upload needs an arg:filename that you want to upload" << endl;
				continue;
			}
		}
		else if (!strcmp(arg1, "download")) {
			arg2 = strtok(NULL, " ");
			arg3 = strtok(NULL, " ");
			if (arg2 == NULL || arg3 == NULL) {
				cout << "command download needs two args:remote_filename and local_filename" << endl;
			}
			else {
				active.download(arg2, arg3);
			}
		}
		else if (!strcmp(arg1, "exit")) {
			cout << "bye!" << endl;
			break;
		}
		else {
			cout << "please input correct command" << endl;
		}
    }
	return 0;
}
