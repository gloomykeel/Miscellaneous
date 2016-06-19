
#ifndef __CENTRALDEF__
#define __CENTRALDEF__

// defines 可采用excel生成
#define KL_STATE    				0xFF
#define KL_Projector				0x01
	#define KL_Projector_epson					0x0101
	#define KL_Projector_benq					0x0102
    #define KL_Projector_sony			     	0x0103
	#define KL_Projector_unknown				0x0104
#define KL_PLC						0x02
#define KL_CircuitFlash				0x03
#define KL_IR						0x04
#define KL_USER						0x05
	#define KL_USER_socket						0x0501
	#define KL_USER_com							0x0502
#define KL_PC						0x06
	#define KLSOFT_video						0x0601
	#define KLPC_PCCommand						0x0602
#define KL_MTX						0x07

enum AddrType { eComm, eSock };
typedef struct tagCmdHdr{
    unsigned char DeviceClass;   // 设备类 projector/IR/PC...
	unsigned char GroupId;       // id
    AddrType addrType;  // 地址类型，com 或者 ip
    unsigned long address;       // 地址，若是IP地址，则为long型，若为com地址，则为主线号
    unsigned long subAddress;    // 端口，IP端口或者com的位置
    char AppType[128];  // 软件类型/或者设备的型号
    unsigned short cmdLen;        // 命令长度
    unsigned char cmdBuf[128];   // 命令字符串
    unsigned short valLen;        // 参数长度
    unsigned char valBuf[128];   // 参数
}CMDHDR,*pCMDHDR;

#define ERR_SOCK 0x01
#define ERR_THREAD 0x02
#define ERR_PATH 0x03
#define ERR_XML 0x04
#define ERR_DUP 0x05
#define ERR_SELECT 0x06
#define ERR_SEND 0x07
#define ERR_FILE 0x08
#define ERR_STOP 0xF0
#define ERR_UNKOWN 0xFF

typedef int (*TRANSCALLBACK)(unsigned int cmdId, unsigned int param);

int StartTransmission();
int StopTransmission();
int SetCallBack(TRANSCALLBACK func);
int SendCommandsById(unsigned int);

#endif
