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

int StartTransmission()
{
	UdpSender::GetMainPtr();
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

UdpSender* UdpSender::GetMainPtr()
{
	if(!UdpSender::mCPtr)
	{
		UdpSender::mCPtr = new UdpSender();
	}
	return UdpSender::mCPtr;
}

UdpSender::UdpSender()
{
	m_pfun = NULL;
	pthread_mutex_init(&mutex,NULL);
	// TODO 读取文件路径输入问题
	_ReadXmlFile("commands.xml");
	if(UdpSender::mErrState) return;
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
}

UdpSender::~UdpSender()
{
	pthread_join(m_sendTId, NULL);
	pthread_join(m_recvTId, NULL);
	pthread_mutex_destroy(&mutex);
	if(UdpSender::mCPtr)
	{
		delete UdpSender::mCPtr;
		UdpSender::mCPtr = NULL;
	}
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
	//TODO 地址和端口，默认地址需要配置文件保存设置
	memcpy(&addrTo,&sp->m_addrTo,sizeof(sockaddr_in));
	pthread_mutex_t * pMutex = &sp->mutex;
	addrTo.sin_family = AF_INET;
	addrTo.sin_addr.s_addr = inet_addr("127.0.0.1");//server的地址
	addrTo.sin_port = htons(UDP_SEND_PORT);//server的监听端口

	int sendSock = -1;
	if( (sendSock = socket(AF_INET, SOCK_DGRAM, 0)) == -1 )
	{
		UdpSender::mErrState = ERR_SOCK;
		return NULL;
	}

	while(!UdpSender::mErrState)
	{
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
