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
#include "tinyxml.h"
#include "UdpSender.h"

using namespace XML;
using namespace std;

int SetCallBack(TRANSCALLBACK func)
{
	UdpSender* p = UdpSender::GetMainPtr();
	if(p)
	{
		p->SetCallBack(func);
	}
	return 0;
}

int StartTransmission(char* cp, int n)
{
	UdpSender* p = UdpSender::GetMainPtr();
	if(p)
	{
		p->SetFilePath(cp,n);
		return p->initT();
	}
	return UdpSender::mErrState;
}

int StopTransmission()
{
	UdpSender::mErrState = ERR_STOP;
	return 0;
}

int SendCommandsById(u_int groupId)
{
	UdpSender* p = UdpSender::GetMainPtr();
	if(p)
	{
		p->SendCommandsById(groupId);
	}
	return UdpSender::mErrState;
}

int SetUpdate(int val)
{
	UdpSender::m_bUpdate = val;
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
volatile int UdpSender::mErrState = 0;
volatile int UdpSender::m_bUpdate = 0;

UdpSender* UdpSender::GetMainPtr()
{
	if(!UdpSender::mCPtr)
	{
		UdpSender::mCPtr = new UdpSender();
	}
	return UdpSender::mCPtr;
}

int UdpSender::initT()
{
	_ReadXmlFile(mfilePath);
	if(UdpSender::mErrState) return UdpSender::mErrState;
	if (pthread_create(&m_sendTId, NULL, _RecvProc, this))
	{
		std::cout << "Create thread failed!" << std::endl;
		UdpSender::mErrState = ERR_THREAD;
	}
	SleepMs(10);
	if (pthread_create(&m_recvTId, NULL, _SendProc, this))
	{
		std::cout << "Create thread failed!" << std::endl;
		UdpSender::mErrState = ERR_THREAD;
	}
	SleepMs(10);
	if (pthread_create(&m_TCPrecvTId, NULL, _TCPrecv, this))
	{
		std::cout << "Create thread failed!" << std::endl;
		UdpSender::mErrState = ERR_THREAD;
	}
	SleepMs(10);
	return UdpSender::mErrState;
}

UdpSender::UdpSender()
{
	m_sendTId = m_recvTId = m_TCPrecvTId = 0;
	m_pfun = NULL;
	memset(mfilePath,0,sizeof(mfilePath));
	pthread_mutex_init(&mutex,NULL);
}

UdpSender::~UdpSender()
{
	pthread_join(m_sendTId, NULL);
	pthread_join(m_recvTId, NULL);
	pthread_join(m_TCPrecvTId, NULL);
	pthread_mutex_destroy(&mutex);
	if(UdpSender::mCPtr)
	{
		delete UdpSender::mCPtr;
		UdpSender::mCPtr = NULL;
	}
}

int UdpSender::SetFilePath(char* p, int n)
{
	if(n<256)
	{
		memcpy(mfilePath,p,n);
		return 0;
	}
	return -1;
}

bool UdpSender::findGroupId(int id)
{
	std::deque<SENDCMDHDR>::iterator iter = gCommandQue.begin();
	for (;iter != gCommandQue.end(); iter ++)
	{
		if(id == (*iter).cmdHdr.GroupId)
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
	TiXmlDocument *myDocument = new TiXmlDocument(fullPath.c_str());                             //创建一个XML的文档对象。
	if(!myDocument)
	{
		UdpSender::mErrState = ERR_PATH;
		delete myDocument;
		return false;
	}
	myDocument->LoadFile();
	TiXmlElement *RootElement = myDocument->RootElement();
	if(!RootElement)
	{
		UdpSender::mErrState = ERR_XML;
		delete myDocument;
		return false;
	}
	//TRACE("%s\n",RootElement->Value());
	for (TiXmlElement* elem1 = RootElement->FirstChildElement(); elem1 != NULL; elem1 = elem1->NextSiblingElement())
	{
		//TRACE("%s     \n", elem1->Value());                                                                            //commands
		TiXmlAttribute* attributeOfCommands = elem1->FirstAttribute();
		//TRACE("%s     :       %s\n", attributeOfCommands->Name(), attributeOfCommands->Value());                       //id  1
		u_char groupId = atoi(attributeOfCommands->Value());
		if(findGroupId(groupId))
		{
			UdpSender::mErrState = ERR_DUP;
			//CString cstr;
			//cstr.Format(_T("发现重复命令组，组号%d"),(int)groupId);
			//::MessageBox(NULL,cstr,_T("提示"),0);
		}
		SENDCMDHDR tmpCmdHdr = {0};
		tmpCmdHdr.cmdHdr.GroupId     = groupId;
		for (TiXmlElement* elem2 = elem1->FirstChildElement(); elem2 != NULL; elem2 = elem2->NextSiblingElement())
		{
			//TRACE("%s     \n", elem2->Value());
			TiXmlAttribute* attributeOfCommand = elem2->FirstAttribute();                                              //Command
			//TRACE("%s     :       %s\n", attributeOfCommand->Name(), attributeOfCommand->Value());                     //type projector
			string commandtype = attributeOfCommand->Value();
			transform(commandtype.begin(), commandtype.end(), commandtype.begin(), (int(*)(int))tolower);
			u_short commandtypeid = gstringTodefineValue(commandtype);
			for (TiXmlElement* elem3 = elem2->FirstChildElement(); elem3 != NULL; elem3 = elem3->NextSiblingElement())
			{
				string Type  = elem3->Value();
				transform(Type.begin(), Type.end(), Type.begin(), (int(*)(int))tolower);
				string Value = elem3->GetText();
				transform(Value.begin(), Value.end(), Value.begin(), (int(*)(int))tolower);
				//WORD Valueid = gstringTodefineValue(Value);
				//TRACE("%s     :       %s\n", elem3->Value(), elem3->GetText());
				switch (commandtypeid)
				{
				case(KL_Projector) :
						tmpCmdHdr.cmdHdr.DeviceClass = KL_Projector;
				tmpCmdHdr.cmdHdr.addrType    = eComm;
				if(!Type.compare("portnumber"))
				{
					tmpCmdHdr.cmdHdr.address     = atol(Value.c_str());
				}
				else if(!Type.compare("product"))
				{
					memcpy(tmpCmdHdr.cmdHdr.AppType,Value.c_str(),Value.size());
				}
				else if(!Type.compare("order"))
				{
					tmpCmdHdr.cmdHdr.cmdLen		= Value.size();
					memcpy(tmpCmdHdr.cmdHdr.cmdBuf, Value.c_str(), Value.size());
					tmpCmdHdr.cmdHdr.cmdBuf[tmpCmdHdr.cmdHdr.cmdLen] = '\0';
				}
				else if(!Type.compare("timedelay"))
				{
					tmpCmdHdr.timeDlay = atoi(Value.c_str());
				}
				break;
				case(KL_PLC) :
                		tmpCmdHdr.cmdHdr.DeviceClass = KL_PLC;
				tmpCmdHdr.cmdHdr.addrType = eComm;
				if(!Type.compare("portnumber"))
				{
					tmpCmdHdr.cmdHdr.address     = atol(Value.c_str());
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
					tmpCmdHdr.timeDlay = atoi(Value.c_str());
				}
				break;
				case(KL_CircuitFlash) :
                		tmpCmdHdr.cmdHdr.DeviceClass = KL_CircuitFlash;
				tmpCmdHdr.cmdHdr.addrType = eComm;
				if(!Type.compare("portnumber"))
				{
					tmpCmdHdr.cmdHdr.address     = atol(Value.c_str());
				}
				else if(!Type.compare("linenumber"))
				{
					tmpCmdHdr.cmdHdr.subAddress = atol(Value.c_str());
				}
				else if(!Type.compare("timedelay"))
				{
					tmpCmdHdr.timeDlay = atoi(Value.c_str());
				}
				break;
				case(KL_IR) :
                		tmpCmdHdr.cmdHdr.DeviceClass = KL_IR;
				tmpCmdHdr.cmdHdr.addrType = eComm;
				if(!Type.compare("portnumber"))
				{
					tmpCmdHdr.cmdHdr.address     = atol(Value.c_str());
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
					tmpCmdHdr.timeDlay = atoi(Value.c_str());
				}
				break;
				case(KL_USER_com) :
                		tmpCmdHdr.cmdHdr.DeviceClass = KL_USER;
				tmpCmdHdr.cmdHdr.addrType = eComm;
				memcpy(tmpCmdHdr.cmdHdr.AppType,"usrcom",sizeof("usrcom"));
				if(!Type.compare("portnumber"))
				{
					tmpCmdHdr.cmdHdr.address     = atol(Value.c_str());
				}
				else if(!Type.compare("data"))
				{
					tmpCmdHdr.cmdHdr.cmdLen = Value.size();
					memcpy(tmpCmdHdr.cmdHdr.cmdBuf, Value.c_str(), Value.size());
					tmpCmdHdr.cmdHdr.cmdBuf[tmpCmdHdr.cmdHdr.cmdLen] = '\0';
				}
				else if(!Type.compare("timedelay"))
				{
					tmpCmdHdr.timeDlay = atoi(Value.c_str());
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
					tmpCmdHdr.timeDlay = atoi(Value.c_str());
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
					tmpCmdHdr.timeDlay = atoi(Value.c_str());
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
					tmpCmdHdr.timeDlay = atoi(Value.c_str());
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
					tmpCmdHdr.timeDlay = atoi(Value.c_str());
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
	SENDCMDHDR tmpHdr = {0};
	if(!groupId) // 起始start命令，请求状态
	{
		tmpHdr.cmdHdr.DeviceClass=0xFF;
		pthread_mutex_lock(&mutex);
		gSendQue.push_back(tmpHdr);
		pthread_mutex_unlock(&mutex);
		return true;
	}

	bool isFind = false;
	std::deque<SENDCMDHDR>::iterator iter = gCommandQue.begin();
	for (;iter != gCommandQue.end(); iter ++)
	{
		if(groupId == (*iter).cmdHdr.GroupId)
		{
			pthread_mutex_lock(&mutex);
			gSendQue.push_back((*iter));
			pthread_mutex_unlock(&mutex);
			isFind = true;
		}
	}

	if(0==groupId) isFind = true;

	return isFind?0:-1;
}

void* UdpSender::_RecvProc(void * lParam)
{
	//初始化套接字地址和端口
	char cBuf[1400] = { 0 };
	socklen_t len = sizeof(sockaddr);
	sockaddr_in tInAddr={0};
	tInAddr.sin_family = AF_INET;
	tInAddr.sin_port = htons(UDP_RECV_PORT);
	tInAddr.sin_addr.s_addr = INADDR_ANY;
	int tSock = -1;
	if( (tSock = socket(AF_INET, SOCK_DGRAM, 0)) == -1 )
	{
		UdpSender::mErrState = ERR_SOCK;
		return (void*)&UdpSender::mErrState;
	}

	if(::bind(tSock, (sockaddr*)&tInAddr, sizeof(tInAddr)))
	{
		shutdown(tSock,SHUT_RDWR);
		UdpSender::mErrState = ERR_SOCK;
		return (void*)&UdpSender::mErrState;
		//::MessageBox(NULL,_T("端口已被占用\n程序即将退出"),_T("提示"),0);
	}

	((UdpSender*)lParam)->SendCommandsById(0);

	fd_set  fds={0};
	struct timeval tv={0};
	tv.tv_sec = 0;
	tv.tv_usec = 500;
	while(!UdpSender::mErrState)
	{
		if(UdpSender::m_bUpdate)
		{
			::usleep(1000);
			continue;
		}

		FD_ZERO(&fds); //每次循环都要清空集合，否则不能检测描述符变化
		FD_SET(tSock,&fds); //添加描述符
		switch(select(tSock+1, &fds, NULL, NULL, &tv)) //select使用
		{
		case -1:
			UdpSender::mErrState = ERR_SELECT;
			return (void*)&UdpSender::mErrState;
			break; //select错误，退出程序
		case 0: break; //再次轮询
		default:
			if(FD_ISSET(tSock,&fds)) //测试sock是否可读，即是否网络上有数据
			{
				if(-1<recvfrom(tSock, cBuf, 1400, 0, (sockaddr*)&tInAddr, &len))
				{
					if(0xABCDEFFE==*((u_long*)cBuf))
					{
						UdpSender::m_bUpdate = 1;
					}

					if (((UdpSender*)lParam)->m_pfun)
					{
						(*((UdpSender*)lParam)->m_pfun)(((pCMDHDR)cBuf)->cmdLen, ((pCMDHDR)cBuf)->valLen);
					}
				}
			}// end if
			else
			{
				shutdown(tSock,SHUT_RDWR);
			}
			break;
		}// end switch
	}//end while

	return 0;
}

void* UdpSender::_SendProc(void * lParam)
{
	char cBuf[1400]={0};
	sockaddr_in addrTo={0};
	UdpSender* sp = (UdpSender*)lParam;
	//TODO 地址，默认地址需要配置文件保存设置
	memcpy(&addrTo,&sp->m_addrTo,sizeof(sockaddr_in));
	pthread_mutex_t * pMutex = &sp->mutex;
	addrTo.sin_family = AF_INET;
	addrTo.sin_addr.s_addr = inet_addr("192.168.127.1");//server的地址
	addrTo.sin_port = htons(UDP_SEND_PORT);//server的监听端口

	int sendSock = -1;
	if( (sendSock = socket(AF_INET, SOCK_DGRAM, 0)) == -1 )
	{
		UdpSender::mErrState = ERR_SOCK;
		return NULL;
	}

	while(!UdpSender::mErrState)
	{
		if(UdpSender::m_bUpdate)
		{
			::usleep(1000);
			continue;
		}
		pthread_mutex_lock(pMutex);
		int queLen = sp->gSendQue.size();
		pSENDCMDHDR pHdr = NULL;
		for(int i=0;i<queLen;++i)
		{
			pHdr = &sp->gSendQue.front();
			if(pHdr->timeDlay<=0)
			{
				memcpy(cBuf,&pHdr->cmdHdr,sizeof(CMDHDR));
				if(-1==sendto(sendSock, cBuf, sizeof(CMDHDR),0,(sockaddr*)&addrTo, sizeof(sockaddr)))
				{
					UdpSender::mErrState = ERR_SEND;
				}
			}
			else
			{
				--pHdr->timeDlay;
				sp->gSendQue.push_back(*pHdr);
				//--(*iter).timeDlay;
			}
			sp->gSendQue.pop_front();
		}
		pthread_mutex_unlock(pMutex);
		SleepMs(100);
		memset(cBuf,0,sizeof(cBuf));
	}
	return 0;
}

#include <sys/fcntl.h>
#include <fstream>
void* UdpSender::_TCPrecv(void * lParam)
{
	int rcd;
	int new_cli_fd;
	int maxfd;
	unsigned int socklen;
	int server_len;
	int ci;
	int watch_fd_list[2];
	int backlog = 2;

	char filePath[256]={0};
	std::fstream fout;

	memcpy(filePath,((UdpSender*)lParam)->mfilePath,256);
	while(!UdpSender::m_bUpdate) ::usleep(1000);

	for (ci = 0; ci <= backlog; ci++)
		watch_fd_list[ci] = -1;

	int server_sockfd;
	//建立socket，类型为TCP流
	server_sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_sockfd == -1) {
		printf("create server_socket error!\n");
		UdpSender::mErrState = ERR_SOCK;
		return (void*)&UdpSender::mErrState;
	}

	//设为非阻塞
	if (fcntl(server_sockfd, F_SETFL, O_NONBLOCK) == -1) {
		printf("Set server socket nonblock failed\n");
		UdpSender::mErrState = ERR_SOCK;
		return (void*)&UdpSender::mErrState;
	}

	struct sockaddr_in server_sockaddr;
	memset(&server_sockaddr, 0, sizeof(server_sockaddr));
	server_sockaddr.sin_family = AF_INET;
	server_sockaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	//设置监听端口
	server_sockaddr.sin_port = htons(TCP_LIS_PORT);
	server_len = sizeof(server_sockaddr);
	//绑定
	rcd = bind(server_sockfd, (struct sockaddr *) &server_sockaddr, server_len);
	if (rcd == -1) {
		printf("bind port %d error!\n", ntohs(server_sockaddr.sin_port));
		UdpSender::mErrState = ERR_SOCK;
		return (void*)&UdpSender::mErrState;
	}
	//监听
	rcd = listen(server_sockfd, backlog);
	if (rcd == -1) {
		printf("listen error!\n");
		UdpSender::mErrState = ERR_SOCK;
		return (void*)&UdpSender::mErrState;
	}
	printf("Server is  waiting on socket=%d \n", server_sockfd);

	watch_fd_list[0] = server_sockfd;
	maxfd = server_sockfd;

	//初始化监听集合
	fd_set watchset;
	FD_ZERO(&watchset);
	FD_SET(server_sockfd, &watchset);

	struct timeval tv; /* 声明一个时间变量来保存时间 */
	struct sockaddr_in cli_sockaddr;
	tv.tv_sec = 0;
	tv.tv_usec = 500;/* 设置select等待的最大时间*/
	while(!UdpSender::mErrState){
		//每次都要重新设置集合才能激发事件
		FD_ZERO(&watchset);
		FD_SET(server_sockfd, &watchset);
		//对已存在到socket重新设置
		for (ci = 0; ci < backlog; ci++)
			if (watch_fd_list[ci] != -1) {
				FD_SET(watch_fd_list[ci], &watchset);
			}

		rcd = select(maxfd + 1, &watchset, NULL, NULL, &tv);
		switch (rcd) {
		case -1:
			printf("Select error\n");
			UdpSender::mErrState = ERR_SELECT;
			return (void*)&UdpSender::mErrState;
		case 0:
			printf(".");
			//超时则清理掉所有集合元素并关闭所有与客户端的socket
			FD_ZERO(&watchset);
			/*for (ci = 1; ci < backlog; ci++){
				shutdown(watch_fd_list[ci],2);
				watch_fd_list[ci] = -1;
			}*/
			//重新设置监听socket，等待链接
			FD_CLR(server_sockfd, &watchset);
			FD_SET(server_sockfd, &watchset);
			continue;
		default:
			//检测是否有新连接建立
			if (FD_ISSET(server_sockfd, &watchset)) { //new connection
				socklen = sizeof(cli_sockaddr);
				new_cli_fd = accept(server_sockfd,(sockaddr *) &cli_sockaddr, &socklen);
				if (new_cli_fd < 0) {
					printf("Accept error\n");
					UdpSender::mErrState = ERR_SOCK;
					return (void*)&UdpSender::mErrState;
				}
				printf("\nopen communication with  Client %s on socket %d\n",
						inet_ntoa(cli_sockaddr.sin_addr), new_cli_fd);

				for (ci = 1; ci < backlog; ci++) {
					if (watch_fd_list[ci] == -1) {
						watch_fd_list[ci] = new_cli_fd;
						break;
					}
				}

				FD_SET(new_cli_fd, &watchset);
				if (maxfd < new_cli_fd) {
					maxfd = new_cli_fd;
				}

				fout.open(filePath,std::ios_base::out|std::ios_base::binary);
				continue;
			} else {//已有连接的数据通信
				//遍历每个设置过的集合元素
				for (ci = 1; ci < backlog; ci++) { //data
					if (watch_fd_list[ci] == -1)
						continue;
					if (!FD_ISSET(watch_fd_list[ci], &watchset)) {
						continue;
					}
					char buffer[1400];
					//接收
					int len = recv(watch_fd_list[ci], buffer, 1400, 0);
					if (len < 0) {
						printf("Recv error\n");
						UdpSender::mErrState = ERR_SOCK;
						return (void*)&UdpSender::mErrState;
					}
					else if(0==len)
					{
						fout.close();
						//接收到的是关闭命令
						for (ci = 0; ci < backlog; ci++)
							if (watch_fd_list[ci] != -1) {
								shutdown(watch_fd_list[ci],2);
							}
						printf("\nWeb Server Quit!\n");
						UdpSender::mErrState = 0;
						UdpSender::m_bUpdate = 0;
						return (void*)&UdpSender::mErrState;
					}
					buffer[len] = 0;
					printf("\n.%s.\n",buffer);
					if(fout.is_open())
					{
						fout.write(buffer,len);
						fout.flush();
					}
					else
					{
						fout.close();
						UdpSender::mErrState = ERR_FILE;
					}
				}
			}
			break;
		}
	}

	for (ci = 0; ci < backlog; ci++){
		shutdown(watch_fd_list[ci],2);
		watch_fd_list[ci] = -1;
	}
	fout.close();

	UdpSender::m_bUpdate = 0;
	return (void*)&UdpSender::mErrState;
}
