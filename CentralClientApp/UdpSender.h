/*
 * UdpSender.h
 *
 *  Created on: Jun 7, 2016
 *      Author: ql
 */

#ifndef UDPSENDER_H_
#define UDPSENDER_H_

#include "CentralDefine.h"

#define CENTRAL_SEVER_PORT 8999
#define PC_UDP_SEVER_PORT 9000
#define CLINET_SEVER_PORT 9001
#define IPAD_UDP_SEVER_PORT 9002
#define IPAD_TCP_SEVER_PORT 9003
#define PLAY_UDP_SEVER_PORT 9004

typedef struct tagSendCmdHdr
{
    CMDHDR cmdHdr;
    long timeDlay;
} SENDCMDHDR, *pSENDCMDHDR;

#include <cctype>
#include <sys/types.h>
#include <deque>
#include <pthread.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string>

static volatile int mErrState;
static volatile int m_bUpdate;

class UdpSender
{
public:
    ~UdpSender();
    static UdpSender* GetMainPtr();
    int SendCommandsById(u_int groupId);
    void SetCallBack(TRANSCALLBACK func)
    {
        m_pfun = func;
    }

    int SetFilePath(char* p, int n);

    int initT();
    int initUpdate();
    int destoryUpdate();

//	static volatile int mErrState;
//	static volatile int m_bUpdate;
protected:

    UdpSender();

    bool findGroupId(int id);

    int _ReadXmlFile(char* szFileName);

    static void SleepMs(unsigned long ms)
    {
        ::usleep(ms * 1000);
    }
    static void* _RecvProc(void * lParam);
    static void* _SendProc(void * lParam);
    static void* _TCPrecv(void * lParam);

    static UdpSender* mCPtr;
    pthread_t m_sendTId;
    pthread_t m_recvTId;
    pthread_t m_TCPrecvTId;
    TRANSCALLBACK m_pfun;
    std::deque<SENDCMDHDR> gCommandQue;
    std::deque<SENDCMDHDR> gSendQue;
    pthread_mutex_t mutex; // = PTHREAD_MUTEX_INITIALIZER;
    sockaddr_in m_addrTo;
    char mfilePath[256];
};

#endif /* UDPSENDER_H_ */
