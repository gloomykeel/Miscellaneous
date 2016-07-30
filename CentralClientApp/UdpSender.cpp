/*
 * UdpSender.cpp
 *
 *  Created on: Jun 7, 2016
 *      Author: ql
 */
#include <sys/socket.h>
#include <netinet/in.h>
#include <string>
#include <algorithm>
#include <errno.h>
#include <signal.h>
#include "tinyxml.h"
#include "UdpSender.h"

using namespace XML;
using namespace std;

std::string g_ip = "192.168.1.101";

int SetCallBack(TRANSCALLBACK func)
{
    UdpSender* p = UdpSender::GetMainPtr();
    if (p)
    {
        p->SetCallBack(func);
    }
    return 0;
}

int StartTransmission(const char* cp, int n)
{
    UdpSender* p = UdpSender::GetMainPtr();
    if (p)
    {
        mErrState = 0;
        p->SetFilePath((char*) cp, n);
        return p->initT();
    }
    return mErrState;
}

int StopTransmission()
{
    mErrState = ERR_STOP;
    return 0;
}

int SendCommandsById(u_int groupId)
{
    UdpSender* p = UdpSender::GetMainPtr();
    if (p)
    {
        p->SendCommandsById(groupId);
    }
    return mErrState;
}

int SetUpdate(int val)
{
    m_bUpdate = val;

    UdpSender* p = UdpSender::GetMainPtr();
    if (!p) return -1;
    if(val)
    {
        p->initUpdate();
    }
    else
    {
        p->destoryUpdate();
        mErrState = 0;
        p->initT();
    }
    return 0;
}

u_short gstringTodefineValue(std::string & str)
{
    if (!string("projector").compare(str))
    {
        return KL_Projector;
    }
    else if (!string("plc").compare(str))
    {
        return KL_PLC;
    }
    else if (!string("circuitflash").compare(str))
    {
        return KL_CircuitFlash;
    }
    else if (!string("ir").compare(str))
    {
        return KL_IR;
    }
    else if (!string("com").compare(str))
    {
        return KL_USER_com;
    }
    else if (!string("socket").compare(str))
    {
        return KL_USER_socket;
    }
    else if (!string("pccommand").compare(str))
    {
        return KLPC_PCCommand;
    }
    else if (!string("video").compare(str))
    {
        return KLSOFT_video;
    }
    else if (!string("epson").compare(str))
    {
        return KL_Projector_epson;
    }
    else if (!string("benq").compare(str))
    {
        return KL_Projector_benq;
    }
    else if (!string("unknown").compare(str))
    {
        return KL_Projector_unknown;
    }
    else if (!string("matrix").compare(str))
    {
        return KL_MTX;
    }
    else
    {
        return 0;
    }
}

UdpSender* UdpSender::mCPtr = NULL;

UdpSender* UdpSender::GetMainPtr()
{
    if (!UdpSender::mCPtr)
    {
        UdpSender::mCPtr = new UdpSender();
    }
    return UdpSender::mCPtr;
}

int UdpSender::initUpdate()
{
    if (pthread_create(&m_TCPrecvTId, NULL, _TCPrecv, this))
    {
        std::cout << "Create thread failed!" << std::endl;
        mErrState = ERR_THREAD;
    }
    SleepMs(100);
    return mErrState;
}

int UdpSender::destoryUpdate()
{
    if(m_TCPrecvTId) pthread_join(m_TCPrecvTId, NULL);
    return mErrState;
}

int UdpSender::initT()
{
    pthread_mutex_lock(&mutex);
    gSendQue.clear();
    pthread_mutex_unlock(&mutex);

    int pthread_kill_err = m_sendTId ? pthread_kill(m_sendTId,0):ESRCH;

    if (pthread_kill_err == ESRCH)
    {
        if (pthread_create(&m_sendTId, NULL, _RecvProc, this))
        {
            std::cout << "Create thread failed!" << std::endl;
            mErrState = ERR_THREAD;
        }
        SleepMs(10);
    }

    pthread_kill_err = m_recvTId ? pthread_kill(m_recvTId,0):ESRCH;
    if (pthread_kill_err == ESRCH)
    {
        if (pthread_create(&m_recvTId, NULL, _SendProc, this))
        {
            std::cout << "Create thread failed!" << std::endl;
            mErrState = ERR_THREAD;
        }
        SleepMs(10);
    }
    _ReadXmlFile(mfilePath);
    return mErrState;
}

UdpSender::UdpSender()
{
    m_sendTId = m_recvTId = m_TCPrecvTId = 0;
    m_pfun = NULL;
    memset(mfilePath, 0, sizeof(mfilePath));
    pthread_mutex_init(&mutex, NULL);
}

UdpSender::~UdpSender()
{
    if(m_sendTId) pthread_join(m_sendTId, NULL);
    if(m_recvTId) pthread_join(m_recvTId, NULL);
    if(m_TCPrecvTId) pthread_join(m_TCPrecvTId, NULL);
    pthread_mutex_destroy(&mutex);
    if (UdpSender::mCPtr)
    {
        delete UdpSender::mCPtr;
        UdpSender::mCPtr = NULL;
    }
}

int UdpSender::SetFilePath(char* p, int n)
{
    if (n < 256)
    {
        memcpy(mfilePath, p, n);
        return 0;
    }
    return -1;
}

bool UdpSender::findGroupId(int id)
{
    std::deque<SENDCMDHDR>::iterator iter = gCommandQue.begin();
    for (; iter != gCommandQue.end(); iter++)
    {
        if (id == (*iter).cmdHdr.GroupId)
        {
            return true;
        }
    }
    return false;
}

int UdpSender::_ReadXmlFile(char* szFileName)
{
    //读取Xml文件，并遍历
    string fullPath = szFileName;
    TiXmlDocument *myDocument = new TiXmlDocument(fullPath.c_str()); //创建一个XML的文档对象。
    if (!myDocument)
    {
        mErrState = ERR_PATH;
        delete myDocument;
        return false;
    }
    myDocument->LoadFile();
    TiXmlElement *RootElement = myDocument->RootElement();
    if (!RootElement)
    {
        mErrState = ERR_XML;
        delete myDocument;
        return false;
    }
    gCommandQue.clear();// we clear the queue first
    for (TiXmlElement* elem1 = RootElement->FirstChildElement(); elem1 != NULL;
            elem1 = elem1->NextSiblingElement())
    {
        TiXmlAttribute* attributeOfCommands = elem1->FirstAttribute();
        u_char groupId = atoi(attributeOfCommands->Value());
        if (findGroupId(groupId))
        {
            mErrState = ERR_DUP;
            //CString cstr;
            //cstr.Format(_T("发现重复命令组，组号%d"),(int)groupId);
            //::MessageBox(NULL,cstr,_T("提示"),0);
        }
        SENDCMDHDR tmpCmdHdr =  { 0 };
        tmpCmdHdr.cmdHdr.GroupId = groupId;
        for (TiXmlElement* elem2 = elem1->FirstChildElement(); elem2 != NULL;
                elem2 = elem2->NextSiblingElement())
        {
            TiXmlAttribute* attributeOfCommand = elem2->FirstAttribute(); //Command
            string commandtype = attributeOfCommand->Value();
            transform(commandtype.begin(), commandtype.end(),
                    commandtype.begin(), (int (*)(int))tolower);
            u_short commandtypeid = gstringTodefineValue(commandtype);
            for (TiXmlElement* elem3 = elem2->FirstChildElement();
                    elem3 != NULL; elem3 = elem3->NextSiblingElement())
            {
                string Type = elem3->Value();
                transform(Type.begin(), Type.end(), Type.begin(),
                        (int (*)(int))tolower);string
                Value = elem3->GetText();
                transform(Value.begin(), Value.end(), Value.begin(),
                        (int (*)(int))tolower);
                switch( commandtypeid)
                {
                    case(KL_Projector) :
                    tmpCmdHdr.cmdHdr.DeviceClass = KL_Projector;
                    tmpCmdHdr.cmdHdr.addrType = eComm;
                    if(!Type.compare("portnumber"))
                    {
                        tmpCmdHdr.cmdHdr.address = atol(Value.c_str());
                    }
                    else if(!Type.compare("product"))
                    {
                        memcpy(tmpCmdHdr.cmdHdr.AppType,Value.c_str(),Value.size());
                    }
                    else if(!Type.compare("order"))
                    {
                        tmpCmdHdr.cmdHdr.cmdLen = Value.size();
                        memcpy(tmpCmdHdr.cmdHdr.cmdBuf, Value.c_str(), Value.size());
                        tmpCmdHdr.cmdHdr.cmdBuf[tmpCmdHdr.cmdHdr.cmdLen] = '\0';
                    }
                    else if(!Type.compare("timedelay"))
                    {
                        tmpCmdHdr.timeDlay = 10*atoi(Value.c_str());
                    }
                    else if(!Type.compare("subportnumber"))
                    {
                        tmpCmdHdr.cmdHdr.subportnumber = atoi(Value.c_str());
                    }
                    break;
                    case(KL_PLC) :
                    tmpCmdHdr.cmdHdr.DeviceClass = KL_PLC;
                    tmpCmdHdr.cmdHdr.addrType = eComm;
                    if(!Type.compare("portnumber"))
                    {
                        tmpCmdHdr.cmdHdr.address = atol(Value.c_str());
                    }
                    else if(!Type.compare("linenumber"))
                    {
                        tmpCmdHdr.cmdHdr.subAddress = atol(Value.c_str());
                    }
                    else if(!Type.compare("state"))
                    {
                        tmpCmdHdr.cmdHdr.cmdLen = Value.size();
                        memcpy(tmpCmdHdr.cmdHdr.cmdBuf, Value.c_str(), Value.size());
                        tmpCmdHdr.cmdHdr.cmdBuf[tmpCmdHdr.cmdHdr.cmdLen] = '\0';
                    }
                    else if(!Type.compare("timedelay"))
                    {
                        tmpCmdHdr.timeDlay = 10*atoi(Value.c_str());
                    }
                    else if(!Type.compare("subportnumber"))
                    {
                        tmpCmdHdr.cmdHdr.subportnumber = atoi(Value.c_str());
                    }
                    break;
                    case(KL_CircuitFlash) :
                    tmpCmdHdr.cmdHdr.DeviceClass = KL_CircuitFlash;
                    tmpCmdHdr.cmdHdr.addrType = eComm;
                    if(!Type.compare("portnumber"))
                    {
                        tmpCmdHdr.cmdHdr.address = atol(Value.c_str());
                    }
                    else if(!Type.compare("linenumber"))
                    {
                        tmpCmdHdr.cmdHdr.subAddress = atol(Value.c_str());
                    }
                    else if(!Type.compare("timedelay"))
                    {
                        tmpCmdHdr.timeDlay = 10*atoi(Value.c_str());
                    }
                    else if(!Type.compare("subportnumber"))
                    {
                        tmpCmdHdr.cmdHdr.subportnumber = atoi(Value.c_str());
                    }
                    break;
                    case(KL_IR) :
                    tmpCmdHdr.cmdHdr.DeviceClass = KL_IR;
                    tmpCmdHdr.cmdHdr.addrType = eComm;
                    if(!Type.compare("portnumber"))
                    {
                        tmpCmdHdr.cmdHdr.address = atol(Value.c_str());
                    }
                    else if(!Type.compare("channel"))
                    {
                        tmpCmdHdr.cmdHdr.cmdLen = atol(Value.c_str());
                    }
                    else if(!Type.compare("button"))
                    {
                        tmpCmdHdr.cmdHdr.valLen = atol(Value.c_str());
                    }
                    else if(!Type.compare("timedelay"))
                    {
                        tmpCmdHdr.timeDlay = 10*atoi(Value.c_str());
                    }
                    else if(!Type.compare("subportnumber"))
                    {
                        tmpCmdHdr.cmdHdr.subportnumber = atoi(Value.c_str());
                    }
                    break;
                    case(KL_USER_com) :
                    tmpCmdHdr.cmdHdr.DeviceClass = KL_USER;
                    tmpCmdHdr.cmdHdr.addrType = eComm;
                    memcpy(tmpCmdHdr.cmdHdr.AppType,"usrcom",sizeof("usrcom"));
                    if(!Type.compare("portnumber"))
                    {
                        tmpCmdHdr.cmdHdr.address = atol(Value.c_str());
                    }
                    else if(!Type.compare("data"))
                    {
                        tmpCmdHdr.cmdHdr.cmdLen = Value.size();
                        memcpy(tmpCmdHdr.cmdHdr.cmdBuf, Value.c_str(), Value.size());
                        tmpCmdHdr.cmdHdr.cmdBuf[tmpCmdHdr.cmdHdr.cmdLen] = '\0';
                    }
                    else if(!Type.compare("timedelay"))
                    {
                        tmpCmdHdr.timeDlay = 10*atoi(Value.c_str());
                    }
                    else if(!Type.compare("subportnumber"))
                    {
                        tmpCmdHdr.cmdHdr.subportnumber = atoi(Value.c_str());
                    }
                    break;
                    case(KL_USER_socket) :
                    tmpCmdHdr.cmdHdr.DeviceClass = KL_USER;
                    tmpCmdHdr.cmdHdr.addrType = eSock;
                    memcpy(tmpCmdHdr.cmdHdr.AppType,"usrsock",sizeof("usrsock"));
                    if(!Type.compare("ip"))
                    {
                        tmpCmdHdr.cmdHdr.address = inet_addr(Value.c_str());
                    }
                    else if(!Type.compare("port"))
                    {
                        tmpCmdHdr.cmdHdr.subAddress = atol(Value.c_str());
                    }
                    else if(!Type.compare("data"))
                    {
                        tmpCmdHdr.cmdHdr.cmdLen = Value.size();
                        memcpy(tmpCmdHdr.cmdHdr.cmdBuf, Value.c_str(), Value.size());
                        tmpCmdHdr.cmdHdr.cmdBuf[tmpCmdHdr.cmdHdr.cmdLen] = '\0';
                    }
                    else if(!Type.compare("timedelay"))
                    {
                        tmpCmdHdr.timeDlay = 10*atoi(Value.c_str());
                    }
                    else if(!Type.compare("subportnumber"))
                    {
                        tmpCmdHdr.cmdHdr.subportnumber = atoi(Value.c_str());
                    }
                    break;
                    case(KLPC_PCCommand) :
                    tmpCmdHdr.cmdHdr.DeviceClass = KL_PC;
                    tmpCmdHdr.cmdHdr.addrType = eSock;
                    tmpCmdHdr.cmdHdr.subAddress = 62537;
                    memcpy(tmpCmdHdr.cmdHdr.AppType,"pccmd",sizeof("pccmd"));
                    if(!Type.compare("ip"))
                    {
                        tmpCmdHdr.cmdHdr.address = inet_addr(Value.c_str());
                    }
                    else if(!Type.compare("order"))
                    {
                        tmpCmdHdr.cmdHdr.cmdLen = Value.size();
                        memcpy(tmpCmdHdr.cmdHdr.cmdBuf, Value.c_str(), Value.size());
                        tmpCmdHdr.cmdHdr.cmdBuf[tmpCmdHdr.cmdHdr.cmdLen] = '\0';
                    }
                    else if(!Type.compare("value"))
                    {
                        tmpCmdHdr.cmdHdr.valLen = Value.size();
                        memcpy(tmpCmdHdr.cmdHdr.valBuf, Value.c_str(), Value.size());
                        tmpCmdHdr.cmdHdr.valBuf[tmpCmdHdr.cmdHdr.valLen] = '\0';
                    }
                    else if(!Type.compare("timedelay"))
                    {
                        tmpCmdHdr.timeDlay = 10*atoi(Value.c_str());
                    }
                    else if(!Type.compare("subportnumber"))
                    {
                        tmpCmdHdr.cmdHdr.subportnumber = atoi(Value.c_str());
                    }
                    break;
                    case(KLSOFT_video) :
                    tmpCmdHdr.cmdHdr.DeviceClass = KL_PC;
                    tmpCmdHdr.cmdHdr.addrType = eSock;
                    tmpCmdHdr.cmdHdr.subAddress = 62537;
                    memcpy(tmpCmdHdr.cmdHdr.AppType,"softvideo",sizeof("softvideo"));
                    if(!Type.compare("ip"))
                    {
                        tmpCmdHdr.cmdHdr.address = inet_addr(Value.c_str());
                    }
                    else if(!Type.compare("order"))
                    {
                        tmpCmdHdr.cmdHdr.cmdLen = Value.size();
                        memcpy(tmpCmdHdr.cmdHdr.cmdBuf, Value.c_str(), Value.size());
                        tmpCmdHdr.cmdHdr.cmdBuf[tmpCmdHdr.cmdHdr.cmdLen] = '\0';
                    }
                    else if(!Type.compare("value"))
                    {
                        tmpCmdHdr.cmdHdr.valLen = Value.size();
                        memcpy(tmpCmdHdr.cmdHdr.valBuf, Value.c_str(), Value.size());
                        tmpCmdHdr.cmdHdr.valBuf[tmpCmdHdr.cmdHdr.valLen] = '\0';
                    }
                    else if(!Type.compare("timedelay"))
                    {
                        tmpCmdHdr.timeDlay = 10*atoi(Value.c_str());
                    }
                    else if(!Type.compare("subportnumber"))
                    {
                        tmpCmdHdr.cmdHdr.subportnumber = atoi(Value.c_str());
                    }
                    break;
                    case(KL_MTX) :
                    tmpCmdHdr.cmdHdr.DeviceClass = KL_MTX;
                    tmpCmdHdr.cmdHdr.addrType = eComm;
                    if(!Type.compare("portnumber"))
                    {
                        tmpCmdHdr.cmdHdr.address = atol(Value.c_str());
                    }
                    else if(!Type.compare("order"))
                    {
                        tmpCmdHdr.cmdHdr.cmdLen = Value.size();
                        memcpy(tmpCmdHdr.cmdHdr.cmdBuf, Value.c_str(), Value.size());
                        tmpCmdHdr.cmdHdr.cmdBuf[tmpCmdHdr.cmdHdr.cmdLen] = '\0';
                    }
                    else if(!Type.compare("param"))
                    {
                        tmpCmdHdr.cmdHdr.valLen = Value.size();
                        memcpy(tmpCmdHdr.cmdHdr.valBuf, Value.c_str(), Value.size());
                        tmpCmdHdr.cmdHdr.valBuf[tmpCmdHdr.cmdHdr.valLen] = '\0';
                    }
                    else if(!Type.compare("timedelay"))
                    {
                        tmpCmdHdr.timeDlay = 10*atoi(Value.c_str());
                    }
                    else if(!Type.compare("subportnumber"))
                    {
                        tmpCmdHdr.cmdHdr.subportnumber = atoi(Value.c_str());
                    }
                    break;
                    default:
                    break;
                }
            }
            //加入deque
            gCommandQue.push_back(tmpCmdHdr);
        }
    }
    delete myDocument;
    return true;
}

int UdpSender::SendCommandsById(u_int groupId)
{
    SENDCMDHDR tmpHdr =  { 0 };
    if (!groupId) // 起始start命令，请求状态
    {
        tmpHdr.cmdHdr.DeviceClass = 0xFF;
        pthread_mutex_lock(&mutex);
        gSendQue.push_back(tmpHdr);
        pthread_mutex_unlock(&mutex);
        return true;
    }

    bool isFind = false;
    std::deque<SENDCMDHDR>::iterator iter = gCommandQue.begin();
    for (; iter != gCommandQue.end(); iter++)
    {
        if (groupId == (*iter).cmdHdr.GroupId)
        {
            pthread_mutex_lock(&mutex);
            //?? ::usleep(300);
            gSendQue.push_back((*iter));
            pthread_mutex_unlock(&mutex);
            isFind = true;
        }
    }

    return isFind ? 0 : -1;
}

void* UdpSender::_RecvProc(void * lParam)
{
    //初始化套接字地址和端口
    char cBuf[1400] =  { 0 };
    socklen_t len = sizeof(sockaddr);
    sockaddr_in tInAddr =  { 0 };
    tInAddr.sin_family = AF_INET;
    tInAddr.sin_port = htons(IPAD_UDP_SEVER_PORT);
    tInAddr.sin_addr.s_addr = INADDR_ANY;
    int tSock = -1;
    if ((tSock = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
    {
        mErrState = ERR_SOCK;
        return (void*) &mErrState;
    }

    if (::bind(tSock, (sockaddr*) &tInAddr, sizeof(tInAddr)))
    {
        close(tSock);
        mErrState = ERR_SOCK;
        return (void*) &mErrState;
        //::MessageBox(NULL,_T("端口已被占用\n程序即将退出"),_T("提示"),0);
    }

    ((UdpSender*) lParam)->SendCommandsById(0);

    fd_set fds = { 0 };
    struct timeval tv = { 0 };
    tv.tv_sec = 0;
    tv.tv_usec = 500;
    while (!mErrState)
    {
        if (m_bUpdate)
        {
            ::usleep(1000);
            continue;
        }

        FD_ZERO(&fds); //每次循环都要清空集合，否则不能检测描述符变化
        FD_SET(tSock, &fds); //添加描述符
        switch (select(tSock + 1, &fds, NULL, NULL, &tv)) //select使用
        {
        case -1:
            close(tSock);
            mErrState = ERR_SELECT;
            break; //select错误，退出程序
        case 0:
            break; //再次轮询
        default:
            if (FD_ISSET(tSock, &fds)) //测试sock是否可读，即是否网络上有数据
            {
                if (-1 < recvfrom(tSock, cBuf, 1400, 0, (sockaddr*) &tInAddr, &len))
                {
                    if (0xABCDEFFE == *((u_long*) cBuf))
                    {
                        m_bUpdate = 1;
                    }

                    if (((UdpSender*) lParam)->m_pfun)
                    {
                        (*((UdpSender*) lParam)->m_pfun)(
                                ((pCMDHDR) cBuf)->cmdLen,
                                ((pCMDHDR) cBuf)->valLen);
                    }
                }
                else
                {
                    close(tSock);
                }
            } // end if
            break;
        } // end switch
    } //end while
    if(-1 != tSock) close(tSock);
    return 0;
}

void* UdpSender::_SendProc(void * lParam)
{
    char cBuf[1400] = { 0 };
    sockaddr_in addrTo = { 0 };
    UdpSender* sp = (UdpSender*) lParam;
    //TODO 地址，默认地址需要配置文件保存设置
    memcpy(&addrTo, &sp->m_addrTo, sizeof(sockaddr_in));
    pthread_mutex_t * pMutex = &sp->mutex;
    addrTo.sin_family = AF_INET;
    addrTo.sin_addr.s_addr = inet_addr(g_ip.c_str()); //server的地址
    addrTo.sin_port = htons(CENTRAL_SEVER_PORT); //server的监听端口

    int sendSock = -1;
    if ((sendSock = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
    {
        mErrState = ERR_SOCK;
        return NULL;
    }

    while (!mErrState)
    {
        if (m_bUpdate)
        {
            ::usleep(1000);
            continue;
        }
        pthread_mutex_lock(pMutex);
        int queLen = sp->gSendQue.size();
        pSENDCMDHDR pHdr = NULL;
        for (int i = 0; i < queLen; ++i)
        {
            pHdr = &sp->gSendQue.front();

            //??::usleep((useconds_t) pHdr->timeDlay * 1000); //等待执行时间
            if(pHdr->timeDlay>0)
            {
                pHdr->timeDlay -= 1;
                sp->gSendQue.push_back(*pHdr);
            }
            else
            {
                memcpy(cBuf, &pHdr->cmdHdr, sizeof(CMDHDR));
                if (-1 == sendto(sendSock, cBuf, sizeof(CMDHDR), 0,
                                (sockaddr*) &addrTo, sizeof(sockaddr)))
                {
                    close(sendSock);
                    mErrState = ERR_SEND;
                    break;
                }
            }

            sp->gSendQue.pop_front(); //删除第一个
            //--queLen; //删除后总长度减1

        }
        pthread_mutex_unlock(pMutex);
        SleepMs(100);
        memset(cBuf, 0, sizeof(cBuf));
    }
    return 0;
}

#include <sys/fcntl.h>
#include <fstream>

TRANSCALLBACK gUpdatePfunc = NULL;

int SetUpdateCallBack(TRANSCALLBACK func)
{
    gUpdatePfunc = func;
    return 0;
}

void* UdpSender::_TCPrecv(void * lParam)
{
    int server_sockfd;
    int new_cli_fd;
    int maxfd;
    unsigned int socklen;
    int server_len;
    int watch_fd_list[2];

    char filePath[256] = { 0 };
    std::fstream fout;

    memcpy(filePath, ((UdpSender*) lParam)->mfilePath, 256);
    while (!m_bUpdate)  ::usleep(1000);

    watch_fd_list[0] =  watch_fd_list[1] = -1;

    //建立socket，类型为TCP流
    server_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sockfd == -1)
    {
        printf("create server_socket error!\n");
        mErrState = ERR_SOCK;
        return (void*) &mErrState;
    }

    //设为非阻塞
    if (fcntl(server_sockfd, F_SETFL, O_NONBLOCK) == -1)
    {
        close(server_sockfd);
        printf("Set server socket nonblock failed\n");
        mErrState = ERR_SOCK;
        return (void*) &mErrState;
    }

    struct sockaddr_in server_sockaddr;
    memset(&server_sockaddr, 0, sizeof(server_sockaddr));
    server_sockaddr.sin_family = AF_INET;
    server_sockaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    //设置监听端口
    server_sockaddr.sin_port = htons(IPAD_TCP_SEVER_PORT);
    server_len = sizeof(server_sockaddr);
    //绑定
    if (-1==bind(server_sockfd, (struct sockaddr *) &server_sockaddr, server_len))
    {
        close(server_sockfd);
        printf("bind port %d error!\n", ntohs(server_sockaddr.sin_port));
        mErrState = ERR_SOCK;
        return (void*) &mErrState;
    }
    //监听
    if (-1==listen(server_sockfd, 2))
    {
        close(server_sockfd);
        printf("listen error!\n");
        mErrState = ERR_SOCK;
        return (void*) &mErrState;
    }
    printf("Server is  waiting on socket=%d \n", server_sockfd);

    //初始化监听集合
    fd_set watchset;
    watch_fd_list[0] = server_sockfd;
    maxfd = server_sockfd;

    struct timeval tv; /* 声明一个时间变量来保存时间 */
    struct sockaddr_in cli_sockaddr;
    tv.tv_sec = 0;
    tv.tv_usec = 500;/* 设置select等待的最大时间*/
    while (m_bUpdate && (!mErrState || ERR_XML == mErrState))
    {
        //每次都要重新设置集合才能激发事件
        FD_ZERO(&watchset);
        //FD_SET(server_sockfd, &watchset);
        //对已存在到socket重新设置
        if (watch_fd_list[0] != -1) FD_SET(watch_fd_list[0], &watchset);
        if (watch_fd_list[1] != -1) FD_SET(watch_fd_list[1], &watchset);

        switch (select(maxfd + 1, &watchset, NULL, NULL, &tv))
        {
        case -1:
            printf("Select error\n");
            mErrState = ERR_SELECT;
            break;
        case 0:
            //printf(".");
            continue;
        default:
            //检测是否有新连接建立
            if (FD_ISSET(server_sockfd, &watchset))
            { //new connection
                socklen = sizeof(cli_sockaddr);
                new_cli_fd = accept(server_sockfd, (sockaddr *) &cli_sockaddr,
                        &socklen);
                if (new_cli_fd < 0)
                {
                    printf("Accept error\n");
                    mErrState = ERR_SOCK;
                    break;
                }
                printf("\nopen communication with  Client %s on socket %d\n",
                        inet_ntoa(cli_sockaddr.sin_addr), new_cli_fd);

                watch_fd_list[1] = new_cli_fd;

                //FD_SET(new_cli_fd, &watchset);
                maxfd = maxfd > new_cli_fd ? maxfd : new_cli_fd;

                fout.open(filePath, std::ios_base::out | std::ios_base::binary);
                continue;
            }
            else
            { //已有连接的数据通信
              //data
                if (watch_fd_list[1] == -1||
                !FD_ISSET(watch_fd_list[1], &watchset))
                {
                    continue;
                }

                char buffer[1400];
                //接收
                int len = recv(watch_fd_list[1], buffer, 1400, 0);
                if (len < 0)
                {
                    printf("Recv error\n");
                    mErrState = ERR_SOCK;
                    return NULL;
                }
                else if (0 == len)
                {
                    fout.close();
                    //接收到的是关闭命令
                    printf("\nWeb Server Quit!\n");

                    if (gUpdatePfunc)
                    {
                        gUpdatePfunc(0, 0);
                    }

                    mErrState = 0;
                    m_bUpdate = 0;
                    break;
                }
                buffer[len] = 0;
                //printf("\n.%s.\n",buffer);
                if (fout.is_open())
                {
                    fout.write(buffer, len);
                    fout.flush();
                }
                else
                {
                    fout.close();
                    mErrState = ERR_FILE;
                }
            }
            break;
        }
    }

    if(fout.is_open()) fout.close();
    if (watch_fd_list[0] != -1) close(watch_fd_list[0]);
    if (watch_fd_list[1] != -1) close(watch_fd_list[1]);
    watch_fd_list[0] =  watch_fd_list[1] = -1;

    m_bUpdate = 0;
    return NULL;
}
