
#include "UdpSender.h"

using namespace std;

int DataHandler(u_int cmdId, u_int param) {
	// TODO 指示UI链接成功
	return 0;
}

int main() {
	int x = sizeof(CMDHDR);
	//StartTransmission();
	SetCallBack(DataHandler);
	x = 500;
	StartTransmission("commands.xml", sizeof("commands.xml"));
	SetUpdate(1);
    usleep(1000 * 30000);
    SetUpdate(0);
	while (x) {
		usleep(1000 * 1000);
		SendCommandsById(1);
	}
	return 0;
}
