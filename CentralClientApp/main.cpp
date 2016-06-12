

#include "UdpSender.h"

using namespace std;

int DataHandler(u_int cmdId, u_int param)
{
	// TODO 指示UI链接成功
    return 0;
}

int main()
{
	StartTransmission();
	SetCallBack(DataHandler);
	SendCommandsById(1);
	while(1)
	{
		usleep(1000);
	}
	return 0;
}
