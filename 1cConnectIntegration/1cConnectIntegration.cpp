// 1cConnectIntegration.cpp: определяет экспортированные функции для приложения DLL.
//

#include "stdafx.h"

#if defined( __linux__ ) || defined(__APPLE__)
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <iconv.h>
#include <sys/time.h>
#endif

#include <wchar.h>
#include <string>
#include <thread>
#include <fstream>
#include <ctime>
#include "1cConnectIntegration.h"
#pragma warning(disable : 4996)

#define TIME_LEN 65

#define BASE_ERRNO 7

static const wchar_t *a_AttribNames[] = { L"ID", L"Time", L"Mode", L"Object", L"Initiator" };
static const wchar_t *a_TagNames[] = { L"ClientID", L"UserID", L"ServiceID", L"ColleagueID", L"ConferenceRoomID", L"Members", L"Status", L"MessageID", //state
L"AuthorID", L"MessageBody", L"Sended", L"SpecialistID", L"CallID", L"CallFrom", L"CallTo", L"StartTime", L"AcceptTime", L"EndTime",
L"Duration", L"BillSec", L"CallResult", L"CallRecordLink", L"SessionId", L"HostCompName", L"AdminCompName", L"HostIP", L"AdminIP",
L"FileTransferUUID", L"FileName", L"FileSize", L"TransferDuration", L"TreatmentID", L"AppointedSpecialistID", L"WaitTime",
L"TreatmentDuration", L"Assessment", L"Comments" };

static const wchar_t *a_PropNames[] = { L"Login" };
static const wchar_t *a_MethodNames[] = { L"Enable", L"Disable", L"ShowMessageBox", L"ConnectToAgent", L"CloseConnectToAgent",
L"SubscribeOnColleaguesMessage", L"SubscribeToEvents_Softphone", L"UnSubscribeFromEvents_Softphone", L"Call_Softphone", L"ChangeOnlineStatus",
L"SubscribeToEvents_OnlineStatus", L"UnSubscribeFromEvents_OnlineStatus", L"SubscribeToEvents_ColleaguesCall", L"UnSubscribeFromEvents_ColleaguesCall",
L"SubscribeToEvents_ClientsCall", L"UnSubscribeFromEvents_ClientsCall", L"SubscribeToEvents_AllCall", L"UnSubscribeFromEvents_AllCall"};

static const wchar_t *a_PropNamesRu[] = { L"Логин" };
static const wchar_t *a_MethodNamesRu[] = { L"Включить", L"Выключить", L"ПоказатьСообщение", L"ПодключитьсяКАгенту", L"ЗакрытьПодключение",
L"ПодписатьсяНаСообщенияКоллег", L"ПодписатьсяНаСобытия_Софтфон", L"ОтписатьсяОтСобытий_Софтфон", L"ПозвонитьВ_Софтфон", L"УстановитьОнлайнСтатус", 
L"ПодписатьсяНаСобытия_ПолучениеСтатуса", L"ОтписатьсяОтСобытий_ПолучениеСтатуса", L"ПодписатьсяНаСобытия_ЗвонокВКоллегах", L"ОтписатьсяОтСобытий_ЗвонокВКоллегах",
L"ПодписатьсяНаСобытия_ЗвонокВКлиентах", L"ОтписатьсяОтСобытий_ЗвонокВКЛиентах", L"ПодписатьсяНаСобытия_Звонок", L"ОтписатьсяОтСобытий_Звонок"};
static const wchar_t a_kClassNames[] = L"C1cConnectIntegration"; 
static IAddInDefBase *pAsyncEvent = NULL;

uint32_t convToShortWchar(WCHAR_T** Dest, const wchar_t* Source, uint32_t len = 0);
uint32_t convFromShortWchar(wchar_t** Dest, const WCHAR_T* Source, uint32_t len = 0);
uint32_t getLenShortWcharStr(const WCHAR_T* Source);
void connectToNamedPipe(std::wstring namepipe);
void writeToNamedPipe(std::wstring stringwrite);
void readFromNamedPipe();
std::wstring getUUID();
std::ofstream fout;
HANDLE hPipe = NULL, hEventWrt, hEventRd;
OVERLAPPED OverLapWrt, OverLapRd;
static AppCapabilities a_capabilities = eAppCapabilitiesInvalid;
static WcharWrapper s_names(a_kClassNames);
//---------------------------------------------------------------------------//
long GetClassObject(const WCHAR_T* wsName, IComponentBase** pInterface)
{
	if (!*pInterface)
	{
		*pInterface = new C1cConnectIntegration;
		return (long)*pInterface;
	}
	return 0;
}
//---------------------------------------------------------------------------//
AppCapabilities SetPlatformCapabilities(const AppCapabilities capabilities)
{
	a_capabilities = capabilities;
	return eAppCapabilitiesLast;
}
//---------------------------------------------------------------------------//
long DestroyObject(IComponentBase** pIntf)
{
	if (!*pIntf)
		return -1;

	delete *pIntf;
	*pIntf = 0;
	return 0;
}
//---------------------------------------------------------------------------//
const WCHAR_T* GetClassNames()
{
	return s_names;
}
//---------------------------------------------------------------------------//
// C1cConnectIntegration
//---------------------------------------------------------------------------//
C1cConnectIntegration::C1cConnectIntegration()
{
	m_iMemory = 0;
	m_iConnect = 0;
#if !defined( __linux__ ) && !defined(__APPLE__)
	m_hTimerQueue = 0;
#endif //__linux__
}
//---------------------------------------------------------------------------//
C1cConnectIntegration::~C1cConnectIntegration()
{
}
//---------------------------------------------------------------------------//
bool C1cConnectIntegration::Init(void* pConnection)
{
	m_iConnect = (IAddInDefBase*)pConnection;
	return m_iConnect != NULL;
}
//---------------------------------------------------------------------------//
long C1cConnectIntegration::GetInfo()
{
	// Component should put supported component technology version 
	// This component supports 2.0 version
	return 2000;
}
//---------------------------------------------------------------------------//
void C1cConnectIntegration::Done()
{
#if !defined( __linux__ ) && !defined(__APPLE__)
	if (m_hTimerQueue)
	{
		DeleteTimerQueue(m_hTimerQueue);
		m_hTimerQueue = 0;
	}
#endif //__linux__
}
/////////////////////////////////////////////////////////////////////////////
// ILanguageExtenderBase
//---------------------------------------------------------------------------//
bool C1cConnectIntegration::RegisterExtensionAs(WCHAR_T** wsExtensionName)
{
	const wchar_t *wsExtension = L"AddInNativeExtension";
	int iActualSize = ::wcslen(wsExtension) + 1;
	WCHAR_T* dest = 0;

	if (m_iMemory)
	{
		if (m_iMemory->AllocMemory((void**)wsExtensionName, iActualSize * sizeof(WCHAR_T)))
			::convToShortWchar(wsExtensionName, wsExtension, iActualSize);
		return true;
	}

	return false;
}
//---------------------------------------------------------------------------//
long C1cConnectIntegration::GetNProps()
{
	return ePropLast;
}
//---------------------------------------------------------------------------//
//поиск св-ва
long C1cConnectIntegration::FindProp(const WCHAR_T* wsPropName)
{
	long plPropNum = -1;
	wchar_t* propName = 0;

	::convFromShortWchar(&propName, wsPropName);
	plPropNum = findName(a_PropNames, propName, ePropLast);

	if (plPropNum == -1)
		plPropNum = findName(a_PropNamesRu, propName, ePropLast);

	delete[] propName;

	return plPropNum;
}
//---------------------------------------------------------------------------//
//возвращаем имя св-ва
const WCHAR_T* C1cConnectIntegration::GetPropName(long lPropNum, long lPropAlias)
{
	if (lPropNum >= ePropLast)
		return NULL;

	wchar_t *wsCurrentName = NULL;
	WCHAR_T *wsPropName = NULL;
	int iActualSize = 0;

	switch (lPropAlias)
	{
	case 0: // First language
		wsCurrentName = (wchar_t*)a_PropNames[lPropNum];
		break;
	case 1: // Second language
		wsCurrentName = (wchar_t*)a_PropNamesRu[lPropNum];
		break;
	default:
		return 0;
	}

	iActualSize = wcslen(wsCurrentName) + 1;

	if (m_iMemory && wsCurrentName)
	{
		if (m_iMemory->AllocMemory((void**)&wsPropName, iActualSize * sizeof(WCHAR_T)))
			::convToShortWchar(&wsPropName, wsCurrentName, iActualSize);
	}

	return wsPropName;
}
//---------------------------------------------------------------------------//
//Получаем значение св-ва
bool C1cConnectIntegration::GetPropVal(const long lPropNum, tVariant* pvarPropVal)
{
	switch (lPropNum)
	{
	case ePropLogin:
		wstring_to_p(login, pvarPropVal);
		break;
	default:
		return false;
	}

	return true;
}
//---------------------------------------------------------------------------//
//Задаем значение св-ва
bool C1cConnectIntegration::SetPropVal(const long lPropNum, tVariant *varPropVal)
{
//	switch (lPropNum)
//	{
//	}

	return true;
}
//---------------------------------------------------------------------------//
//Задаем доступность св-ва для четния
bool C1cConnectIntegration::IsPropReadable(const long lPropNum)
{
	switch (lPropNum)
	{
	case ePropLogin:
		return true;
	default:
		return false;
	}

	return false;
}
//---------------------------------------------------------------------------//
//Задаем доступность св-ва для записи
bool C1cConnectIntegration::IsPropWritable(const long lPropNum)
{
	switch (lPropNum)
	{
	case ePropLogin:
		return false;
	default:
		return false;
	}

	return false;
}
//---------------------------------------------------------------------------//
//Возвращаем кол-во методов
long C1cConnectIntegration::GetNMethods()
{
	return eMethLast;
}
//---------------------------------------------------------------------------//
//поиск метода
long C1cConnectIntegration::FindMethod(const WCHAR_T* wsMethodName)
{
	long plMethodNum = -1;
	wchar_t* name = 0;

	::convFromShortWchar(&name, wsMethodName);

	plMethodNum = findName(a_MethodNames, name, eMethLast);

	if (plMethodNum == -1)
		plMethodNum = findName(a_MethodNamesRu, name, eMethLast);

	delete[] name;

	return plMethodNum;
}
//---------------------------------------------------------------------------//
//возвращаем име метода
const WCHAR_T* C1cConnectIntegration::GetMethodName(const long lMethodNum, const long lMethodAlias)
{
	if (lMethodNum >= eMethLast)
		return NULL;

	wchar_t *wsCurrentName = NULL;
	WCHAR_T *wsMethodName = NULL;
	int iActualSize = 0;

	switch (lMethodAlias)
	{
	case 0: // First language
		wsCurrentName = (wchar_t*)a_MethodNames[lMethodNum];
		break;
	case 1: // Second language
		wsCurrentName = (wchar_t*)a_MethodNamesRu[lMethodNum];
		break;
	default:
		return 0;
	}

	iActualSize = wcslen(wsCurrentName) + 1;

	if (m_iMemory && wsCurrentName)
	{
		if (m_iMemory->AllocMemory((void**)&wsMethodName, iActualSize * sizeof(WCHAR_T)))
			::convToShortWchar(&wsMethodName, wsCurrentName, iActualSize);
	}

	return wsMethodName;
}
//---------------------------------------------------------------------------//
//Задаем количество параметров у метода
long C1cConnectIntegration::GetNParams(const long lMethodNum)
{
	switch (lMethodNum)
	{
	case eMethConnectToAgent:
		return 1;
	case eMethCallSoftphone:
		return 2;
	case eMethChangeStatus:
		return 2;
	default:
		return 0;
	}

	return 0;
}
//---------------------------------------------------------------------------//
//Значение параметров метода по умолчанию
bool C1cConnectIntegration::GetParamDefValue(const long lMethodNum, const long lParamNum,
	tVariant *pvarParamDefValue)
{
	TV_VT(pvarParamDefValue) = VTYPE_EMPTY;

	switch (lMethodNum)
	{
	case eMethEnable:
	case eMethDisable:
	case eMethShowMsgBox:
	case eMethConnectToAgent:
	case eMethCloseConnect:
	case eSubColleaguesMessage:
	case eMethSubSoftphone:
	case eMethUnSubSoftphone:
	case eMethCallSoftphone:
	case eMethSubStatus:
	case eMethUnSubStatus:
	case eMethSubColleaguesCall:
	case eMethUnSubColleaguesCall:
	case eMethSubClientsCall:
	case eMethUnSubClientsCall:
	case eMethSubCall:
	case eMethUnSubCall:
		// There are no parameter values by default 
		break;
	default:
		return false;
	}

	return false;
}
//---------------------------------------------------------------------------//
//указываем функция или процедура
bool C1cConnectIntegration::HasRetVal(const long lMethodNum)
{
//	switch (lMethodNum)
//	{
//	}

	return false;
}
//---------------------------------------------------------------------------//
//Описание методов-процедур
bool C1cConnectIntegration::CallAsProc(const long lMethodNum,
	tVariant* paParams, const long lSizeArray)
{
	pAsyncEvent = m_iConnect;
	pAsyncEvent->SetEventBufferDepth(1000);

	WCHAR_T *name = 0;
	WCHAR_T *type = 0;
	WCHAR_T *message = 0;

	switch (lMethodNum)
	{
	case eMethEnable:
		m_boolEnabled = true;
		break;
	case eMethDisable:
		m_boolEnabled = false;
		break;
	case eMethShowMsgBox:
	{
		if (eAppCapabilities1 <= a_capabilities)
		{
			IAddInDefBaseEx* cnn = (IAddInDefBaseEx*)m_iConnect;
			IMsgBox* imsgbox = (IMsgBox*)cnn->GetInterface(eIMsgBox);
			if (imsgbox)
			{
				IPlatformInfo* info = (IPlatformInfo*)cnn->GetInterface(eIPlatformInfo);
				assert(info);
				const IPlatformInfo::AppInfo* plt = info->GetPlatformInfo();
				if (!plt)
					break;
				tVariant retVal;
				tVarInit(&retVal);
				if (imsgbox->Confirm(plt->AppVersion, &retVal))
				{
					bool succeed = TV_BOOL(&retVal);
					WCHAR_T* result = 0;

					if (succeed)
						::convToShortWchar(&result, L"OK");
					else
						::convToShortWchar(&result, L"Cancel");

					imsgbox->Alert(result);
					delete[] result;

				}
			}
		}
	}
	break;
	case eMethConnectToAgent:
		try
		{
			char* temp_path;
			size_t len;
			_dupenv_s(&temp_path, &len, "TEMP");
			if (TV_VT(paParams) != VTYPE_PWSTR)
			{
				::convToShortWchar(&name, L"ДрайверВзаимодействияАгент1СКоннект");
				::convToShortWchar(&type, L"Ошибка подключения");
				::convToShortWchar(&message, L"Неверный тип данных, ID агента указывается в кавычках");
				pAsyncEvent->ExternalEvent(name, type, message);
			}
			else
			{
				login = (paParams)->pwstrVal;
				connectToNamedPipe(login);
				std::thread thr(&readFromNamedPipe);
				thr.detach();
			}
		}
		catch (const std::exception&)
		{

		}
		break;
	case eMethCloseConnect:
		if (fout)
			fout << "Отключение от канала" << '\n';
		try
		{
			fout.close();
		}
		catch (const std::exception&)
		{

		}
		login = L"";
		CloseHandle(hPipe);
		break;
	case eSubColleaguesMessage:
//		writeToNamedPipe(L"<Command Action=\"EventSubscribe\" ID=\"550e8400-e29b-41d4-a716-446655440000\" Mode=\"Colleagues\" Object=\"Message\" Initiator=\"Self\"></Command>");
		break;
	case eMethSubSoftphone:
		if (fout)
			fout << "Подписаться на исх звронки софтфона" << '\n';
		//			commandid = getUUID();
		writeToNamedPipe(L"<Command Action=\"EventSubscribe\" ID=\"SubscribeSelfCall\" Mode=\"Softphone\" Object=\"Call\" Initiator=\"Self\"></Command>");
		if (fout)
			fout << "Подписаться на вх звронки софтфона" << '\n';
		//			commandid = getUUID();
		writeToNamedPipe(L"<Command Action=\"EventSubscribe\" ID=\"SubscribeIncomCall\" Mode=\"Softphone\" Object=\"Call\" Initiator=\"Incoming\"></Command>");
		break;
	case eMethUnSubSoftphone:
		if (fout)
			fout << "Отписаться от исх звронки софтфона" << '\n';
		//		commandid = getUUID();
		writeToNamedPipe(L"<Command Action=\"EventUnSubscribe\" ID=\"UnSubscribeSelfCall\" Mode=\"Softphone\" Object=\"Call\" Initiator=\"Self\"></Command>");
		if (fout)
			fout << "Отписаться от вх звронки софтфона" << '\n';
		//		commandid = getUUID();
		writeToNamedPipe(L"<Command Action=\"EventUnSubscribe\" ID=\"UnSubscribeIncomCall\" Mode=\"Softphone\" Object=\"Call\" Initiator=\"Incoming\"></Command>");
		break;
	case eMethCallSoftphone:
		if (fout)
			fout << "Звонок в софтфон" << '\n';
		commandid = (paParams)->pwstrVal;
		phonenumber = (paParams + 1)->pwstrVal;
		writeToNamedPipe(L"<Command Action=\"CallTo\" ID=\"" + commandid + L"\" Mode=\"Softphone\"><CallTo>" + phonenumber + L"</CallTo></Command>");
		break;
	case eMethChangeStatus:
		if (fout)
			fout << "Изменить статус" << '\n';
		commandid = (paParams)->pwstrVal;
		status = (paParams + 1)->pwstrVal;
		writeToNamedPipe(L"<Command Action=\"ChangeMyOnlineStatus\" ID=\"" + commandid + L"\"><Status>" + status + L"</Status></Command>");
		break;
	case eMethSubStatus:
		if (fout)
			fout << "Подписаться на изменение статуса" << '\n';
		writeToNamedPipe(L"<Command Action=\"EventSubscribe\" ID=\"SubscribeOnlineStatus\" Mode=\"ServicesClients\" Object=\"AgentOnlineStatus\" Initiator=\"Self\"></Command>");
		break;
	case eMethUnSubStatus:
		if (fout)
			fout << "Отписаться от изменения статуса" << '\n';

		writeToNamedPipe(L"<Command Action=\"EventUnSubscribe\" ID=\"UnSubscribeOnlineStatus\" Mode=\"ServicesClients\" Object=\"AgentOnlineStatus\" Initiator=\"Self\"></Command>");
		break;
	case eMethSubColleaguesCall:
		if (fout)
			fout << "Подписаться на исх звронки коллеги" << '\n';
		writeToNamedPipe(L"<Command Action=\"EventSubscribe\" ID=\"SubscribeSelfColleaguesCall\" Mode=\"Colleagues\" Object=\"Call\" Initiator=\"Self\"></Command>");
		if (fout)
			fout << "Подписаться на вх звронки коллеги" << '\n';
		writeToNamedPipe(L"<Command Action=\"EventSubscribe\" ID=\"SubscribeIncomColleaguesCall\" Mode=\"Colleagues\" Object=\"Call\" Initiator=\"Incoming\"></Command>");
		break;
	case eMethUnSubColleaguesCall:
		if (fout)
			fout << "Отписаться от исх звронки коллеги" << '\n';
		writeToNamedPipe(L"<Command Action=\"EventUnSubscribe\" ID=\"UnSubscribeSelfColleaguesCall\" Mode=\"Colleagues\" Object=\"Call\" Initiator=\"Self\"></Command>");
		if (fout)
			fout << "Отписаться от вх звронки коллеги" << '\n';
		writeToNamedPipe(L"<Command Action=\"EventUnSubscribe\" ID=\"UnSubscribeIncomColleaguesCall\" Mode=\"Colleagues\" Object=\"Call\" Initiator=\"Incoming\"></Command>");
		break;
	case eMethSubClientsCall:
		if (fout)
			fout << "Подписаться на исх звронки клиенты" << '\n';
		writeToNamedPipe(L"<Command Action=\"EventSubscribe\" ID=\"SubscribeSelfClientsCall\" Mode=\"ServicesClients\" Object=\"Call\" Initiator=\"Self\"></Command>");
		if (fout)
			fout << "Подписаться на вх звронки клиенты" << '\n';
		writeToNamedPipe(L"<Command Action=\"EventSubscribe\" ID=\"SubscribeIncomClientsCall\" Mode=\"ServicesClients\" Object=\"Call\" Initiator=\"Incoming\"></Command>");
		break;
	case eMethUnSubClientsCall:
		if (fout)
			fout << "Отписаться от исх звронки клиенты" << '\n';
		writeToNamedPipe(L"<Command Action=\"EventUnSubscribe\" ID=\"UnSubscribeSelfClientsCall\" Mode=\"ServicesClients\" Object=\"Call\" Initiator=\"Self\"></Command>");
		if (fout)
			fout << "Отписаться от вх звронки клиенты" << '\n';
		writeToNamedPipe(L"<Command Action=\"EventUnSubscribe\" ID=\"UnSubscribeIncomClientsCall\" Mode=\"ServicesClients\" Object=\"Call\" Initiator=\"Incoming\"></Command>");
		break;
	case eMethSubCall:
		if (fout)
			fout << "Подписаться на исх звронки коллеги" << '\n';
		writeToNamedPipe(L"<Command Action=\"EventSubscribe\" ID=\"SubscribeSelfColleaguesCall\" Mode=\"Colleagues\" Object=\"Call\" Initiator=\"Self\"></Command>");
		if (fout)
			fout << "Подписаться на вх звронки коллеги" << '\n';
		writeToNamedPipe(L"<Command Action=\"EventSubscribe\" ID=\"SubscribeIncomColleaguesCall\" Mode=\"Colleagues\" Object=\"Call\" Initiator=\"Incoming\"></Command>");
		if (fout)
			fout << "Подписаться на исх звронки клиенты" << '\n';
		writeToNamedPipe(L"<Command Action=\"EventSubscribe\" ID=\"SubscribeSelfClientsCall\" Mode=\"ServicesClients\" Object=\"Call\" Initiator=\"Self\"></Command>");
		if (fout)
			fout << "Подписаться на вх звронки клиенты" << '\n';
		writeToNamedPipe(L"<Command Action=\"EventSubscribe\" ID=\"SubscribeIncomClientsCall\" Mode=\"ServicesClients\" Object=\"Call\" Initiator=\"Incoming\"></Command>");
		if (fout)
			fout << "Подписаться на исх звронки софтфона" << '\n';
		writeToNamedPipe(L"<Command Action=\"EventSubscribe\" ID=\"SubscribeSelfCall\" Mode=\"Softphone\" Object=\"Call\" Initiator=\"Self\"></Command>");
		if (fout)
			fout << "Подписаться на вх звронки софтфона" << '\n';
		writeToNamedPipe(L"<Command Action=\"EventSubscribe\" ID=\"SubscribeIncomCall\" Mode=\"Softphone\" Object=\"Call\" Initiator=\"Incoming\"></Command>");
		break;
	case eMethUnSubCall:
		if (fout)
			fout << "Отписаться от исх звронки коллеги" << '\n';
		writeToNamedPipe(L"<Command Action=\"EventUnSubscribe\" ID=\"UnSubscribeSelfColleaguesCall\" Mode=\"Colleagues\" Object=\"Call\" Initiator=\"Self\"></Command>");
		if (fout)
			fout << "Отписаться от вх звронки коллеги" << '\n';
		writeToNamedPipe(L"<Command Action=\"EventUnSubscribe\" ID=\"UnSubscribeIncomColleaguesCall\" Mode=\"Colleagues\" Object=\"Call\" Initiator=\"Incoming\"></Command>");
		if (fout)
			fout << "Отписаться от исх звронки клиенты" << '\n';
		writeToNamedPipe(L"<Command Action=\"EventUnSubscribe\" ID=\"UnSubscribeSelfClientsCall\" Mode=\"ServicesClients\" Object=\"Call\" Initiator=\"Self\"></Command>");
		if (fout)
			fout << "Отписаться от вх звронки клиенты" << '\n';
		writeToNamedPipe(L"<Command Action=\"EventUnSubscribe\" ID=\"UnSubscribeIncomClientsCall\" Mode=\"ServicesClients\" Object=\"Call\" Initiator=\"Incoming\"></Command>");
		if (fout)
			fout << "Отписаться от исх звронки софтфон" << '\n';
		writeToNamedPipe(L"<Command Action=\"EventUnSubscribe\" ID=\"UnSubscribeSelfCall\" Mode=\"Softphone\" Object=\"Call\" Initiator=\"Self\"></Command>");
		if (fout)
			fout << "Отписаться от вх звронки софтфон" << '\n';
		writeToNamedPipe(L"<Command Action=\"EventUnSubscribe\" ID=\"UnSubscribeIncomCall\" Mode=\"Softphone\" Object=\"Call\" Initiator=\"Incoming\"></Command>");
		break;
	default:
		return false;
	}

	//delete[] name;
	//delete[] type;
	//delete[] message;

	return true;
}
//---------------------------------------------------------------------------//
//Описание методов-функций
bool C1cConnectIntegration::CallAsFunc(const long lMethodNum,
	tVariant* pvarRetValue, tVariant* paParams, const long lSizeArray)
{
//	switch (lMethodNum)
//	{
//		// Method acceps one argument of type BinaryData ant returns its copy
//	}
	return false;
}
//---------------------------------------------------------------------------//
bool C1cConnectIntegration::wstring_to_p(std::wstring str, tVariant* val) {
	char* t1;
	TV_VT(val) = VTYPE_PWSTR;
	m_iMemory->AllocMemory((void**)&t1, (str.length() + 1) * sizeof(WCHAR_T));
	memcpy(t1, str.c_str(), (str.length() + 1) * sizeof(WCHAR_T));
	val->pstrVal = t1;
	val->strLen = str.length();
	return true;
}
//---------------------------------------------------------------------------//
void C1cConnectIntegration::SetLocale(const WCHAR_T* loc)
{
#if !defined( __linux__ ) && !defined(__APPLE__)
	_wsetlocale(LC_ALL, loc);
#else
	//We convert in char* char_locale
	//also we establish locale
	//setlocale(LC_ALL, char_locale);
#endif
}
/////////////////////////////////////////////////////////////////////////////
// LocaleBase
//---------------------------------------------------------------------------//
bool C1cConnectIntegration::setMemManager(void* mem)
{
	m_iMemory = (IMemoryManager*)mem;
	return m_iMemory != 0;
}
//---------------------------------------------------------------------------//
void C1cConnectIntegration::addError(uint32_t wcode, const wchar_t* source,
	const wchar_t* descriptor, long code)
{
	if (m_iConnect)
	{
		WCHAR_T *err = 0;
		WCHAR_T *descr = 0;

		::convToShortWchar(&err, source);
		::convToShortWchar(&descr, descriptor);

		m_iConnect->AddError(wcode, err, descr, code);
		delete[] err;
		delete[] descr;
	}
}
//---------------------------------------------------------------------------//
long C1cConnectIntegration::findName(const wchar_t* names[], const wchar_t* name,
	const uint32_t size) const
{
	long ret = -1;
	for (uint32_t i = 0; i < size; i++)
	{
		if (!wcscmp(names[i], name))
		{
			ret = i;
			break;
		}
	}
	return ret;
}
//---------------------------------------------------------------------------//
uint32_t convToShortWchar(WCHAR_T** Dest, const wchar_t* Source, uint32_t len)
{
	try
	{
		if (!len)
			len = ::wcslen(Source) + 1;

		if (!*Dest)
			*Dest = new WCHAR_T[len];

		WCHAR_T* tmpShort = *Dest;
		wchar_t* tmpWChar = (wchar_t*)Source;
		uint32_t res = 0;

		::memset(*Dest, 0, len * sizeof(WCHAR_T));
#ifdef __linux__
		size_t succeed = (size_t)-1;
		size_t f = len * sizeof(wchar_t), t = len * sizeof(WCHAR_T);
		const char* fromCode = sizeof(wchar_t) == 2 ? "UTF-16" : "UTF-32";
		iconv_t cd = iconv_open("UTF-16LE", fromCode);
		if (cd != (iconv_t)-1)
		{
			succeed = iconv(cd, (char**)&tmpWChar, &f, (char**)&tmpShort, &t);
			iconv_close(cd);
			if (succeed != (size_t)-1)
				return (uint32_t)succeed;
		}
#endif //__linux__
		for (; len; --len, ++res, ++tmpWChar, ++tmpShort)
		{
			*tmpShort = (WCHAR_T)*tmpWChar;
		}

		return res;
	}
	catch (const std::exception& e)
	{
		if (fout)
			fout << e.what() << '\n';
	}

}
//---------------------------------------------------------------------------//
uint32_t convFromShortWchar(wchar_t** Dest, const WCHAR_T* Source, uint32_t len)
{
	if (!len)
		len = getLenShortWcharStr(Source) + 1;

	if (!*Dest)
		*Dest = new wchar_t[len];

	wchar_t* tmpWChar = *Dest;
	WCHAR_T* tmpShort = (WCHAR_T*)Source;
	uint32_t res = 0;

	::memset(*Dest, 0, len * sizeof(wchar_t));
#ifdef __linux__
	size_t succeed = (size_t)-1;
	const char* fromCode = sizeof(wchar_t) == 2 ? "UTF-16" : "UTF-32";
	size_t f = len * sizeof(WCHAR_T), t = len * sizeof(wchar_t);
	iconv_t cd = iconv_open("UTF-32LE", fromCode);
	if (cd != (iconv_t)-1)
	{
		succeed = iconv(cd, (char**)&tmpShort, &f, (char**)&tmpWChar, &t);
		iconv_close(cd);
		if (succeed != (size_t)-1)
			return (uint32_t)succeed;
	}
#endif //__linux__
	for (; len; --len, ++res, ++tmpWChar, ++tmpShort)
	{
		*tmpWChar = (wchar_t)*tmpShort;
	}

	return res;
}
//---------------------------------------------------------------------------//
uint32_t getLenShortWcharStr(const WCHAR_T* Source)
{
	uint32_t res = 0;
	WCHAR_T *tmpShort = (WCHAR_T*)Source;

	while (*tmpShort++)
		++res;

	return res;
}
//---------------------------------------------------------------------------//
//Устанавливаем соединение с namedpipe
void connectToNamedPipe(std::wstring namepipe)
{
	WCHAR_T *name = 0;
	WCHAR_T *type = 0;
	WCHAR_T *message = 0;
	
	try
	{
		char* temp_path;
		size_t len;
		_dupenv_s(&temp_path, &len, "TEMP");

		time_t rawtime;
		struct tm * timeinfo;
		char buffer[80];
		time(&rawtime);
		timeinfo = localtime(&rawtime);
		strftime(buffer, 80, "%d%m%y_%H_%M%p.log", timeinfo);

		std::string path(temp_path);
		path = path + "\\DriverIntegration";
		
		std::wstring widestr = std::wstring(path.begin(), path.end());
		const wchar_t* widecstr = widestr.c_str();
		CreateDirectory(widecstr, NULL);
	
		path += "\\"+std::string(buffer);
		//	std::ofstream fout;
		fout.open(path);
		//fout << path;
//		fout.close();

	}
	catch (const std::exception&)
	{

	}
	::convToShortWchar(&name, L"ДрайверВзаимодействияАгент1СКоннект");
	::convToShortWchar(&type, L"Подключение");
	try
	{
		if (fout)
			fout << "Подключение к каналу" << '\n';
		if (hPipe = INVALID_HANDLE_VALUE)
		{
			hPipe = CreateFile(const_cast<wchar_t*>((L"\\\\.\\pipe\\BuhphoneAgentAPI2_" + namepipe).c_str()),
				GENERIC_WRITE | GENERIC_READ,
				FILE_SHARE_READ | FILE_SHARE_WRITE,
				NULL,
				OPEN_EXISTING,
				FILE_FLAG_OVERLAPPED,
				NULL);
			hEventWrt = CreateEvent(NULL, true, false, NULL);
			OverLapWrt.hEvent = hEventWrt;
		}
		if (hPipe != INVALID_HANDLE_VALUE)
		{
			if (fout)
				fout << "Соединение установлено успешно" << '\n';
			::convToShortWchar(&message, L"Соединение установлено успешно");
			pAsyncEvent->ExternalEvent(name, type, message);
		}
		else
		{
			if (fout)
				fout << "Соединение не установлено" << '\n';
			::convToShortWchar(&message, L"Соединение не установлено");
			pAsyncEvent->ExternalEvent(name, type, message);
		}
	}
	catch (const std::exception& e)
	{
		if (fout)
			fout << e.what() << '\n';
	}
	
}
//---------------------------------------------------------------------------//
//посылаем команду в namedpipe
void writeToNamedPipe(std::wstring wstringwrite)
{
	WCHAR_T *name = 0;
	WCHAR_T *type = 0;
	WCHAR_T *message = 0;
	::convToShortWchar(&name, L"ДрайверВзаимодействияАгент1СКоннект");
	DWORD dwWritten;
	try
	{
		WCHAR_T *stringwrite = const_cast<WCHAR_T*>((wstringwrite).c_str());
		if (hPipe != INVALID_HANDLE_VALUE)
		{
			bool pSuccess = WriteFile(hPipe,
				stringwrite,
				wcslen(stringwrite) * sizeof(WCHAR_T),
				&dwWritten,
				&OverLapWrt);
			Sleep(500);
			if (!pSuccess)
			{
				if (fout)
					fout << "Запись в канал невозможна" << '\n';
				::convToShortWchar(&type, L"Запись в канал");
				::convToShortWchar(&message, L"Запись в канал невозможна");
				pAsyncEvent->ExternalEvent(name, type, message);
			}
		}
		else
		{
			if (fout)
				fout << "Соединение с каналом разорвано или не было установлено" << '\n';
			::convToShortWchar(&type, L"Подключение");
			::convToShortWchar(&message, L"Соединение с каналом разорвано или не было установлено");
			pAsyncEvent->ExternalEvent(name, type, message);
		}

	}
	catch (const std::exception& e)
	{
		if (fout)
			fout << e.what() << '\n';
	}

}
//---------------------------------------------------------------------------//
//читаем данные из namedpipe
void readFromNamedPipe()
{
	WCHAR_T *name = 0;
	WCHAR_T *type = 0;
	WCHAR_T *text = 0;
	WCHAR_T chBuf[9048];
	int leng = 0;
	bool fsuccess;
	::convToShortWchar(&name, L"ДрайверВзаимодействияАгент1СКоннект");
	::convToShortWchar(&type, L"Сообщение");
	::convToShortWchar(&text, L"Соединение с каналом было разорвано");
	DWORD dwRead = 0;
	try
	{
		if (hPipe != INVALID_HANDLE_VALUE)
		{
			while (1)

			{
				fsuccess = ReadFile(hPipe, chBuf, 9048 * sizeof(WCHAR_T), &dwRead, NULL);
				if (fsuccess && dwRead > 2)
				{
					//		for (int i = 0; i < 2; i++)
					leng = int(chBuf[0]) / 2;
					chBuf[0] = ' ';
					chBuf[1] = ' ';

					chBuf[leng] = '\0';
					//		chBuf[dwRead-1] = 'a';
					if (fout)
						fout << "Сообщение от pipe канала" << '\n';
					pAsyncEvent->ExternalEvent(name, type, chBuf);
					//			for (int i = 0; i < dwRead; i++)
					//				chBuf[i] = '\0';
				}
				else
					if (!fsuccess)
						break;
			}
		}
		if (fout)
			fout << "Сообщение от pipe канала" << '\n';
		pAsyncEvent->ExternalEvent(name, type, text);

	}
	catch (const std::exception& e)
	{
		if (fout)
			fout << e.what() << '\n';
	}
}
//---------------------------------------------------------------------------//
std::wstring getUUID()
{
	/*	UUID uuid;
	::UuidCreate(&uuid);
	WCHAR* wszUuid = NULL;
	::UuidToStringW(&uuid, (RPC_WSTR*)&wszUuid);
	if (wszUuid != NULL)
	{
	::RpcStringFree((RPC_WSTR*)&wszUuid);
	wszUuid = NULL;
	}

	guid = wszUuid;
	*/
	std::wstring guid;
	return guid;
}
//---------------------------------------------------------------------------//
#ifdef LINUX_OR_MACOS
WcharWrapper::WcharWrapper(const WCHAR_T* str) : m_str_WCHAR(NULL),
m_str_wchar(NULL)
{
	if (str)
	{
		int len = getLenShortWcharStr(str);
		m_str_WCHAR = new WCHAR_T[len + 1];
		memset(m_str_WCHAR, 0, sizeof(WCHAR_T) * (len + 1));
		memcpy(m_str_WCHAR, str, sizeof(WCHAR_T) * len);
		::convFromShortWchar(&m_str_wchar, m_str_WCHAR);
	}
}
#endif
//---------------------------------------------------------------------------//
WcharWrapper::WcharWrapper(const wchar_t* str) :
#ifdef LINUX_OR_MACOS
	m_str_WCHAR(NULL),
#endif 
	m_str_wchar(NULL)
{
	if (str)
	{
		int len = wcslen(str);
		m_str_wchar = new wchar_t[len + 1];
		memset(m_str_wchar, 0, sizeof(wchar_t) * (len + 1));
		memcpy(m_str_wchar, str, sizeof(wchar_t) * len);
#ifdef LINUX_OR_MACOS
		::convToShortWchar(&m_str_WCHAR, m_str_wchar);
#endif
	}

}
//---------------------------------------------------------------------------//
WcharWrapper::~WcharWrapper()
{
#ifdef LINUX_OR_MACOS
	if (m_str_WCHAR)
	{
		delete[] m_str_WCHAR;
		m_str_WCHAR = NULL;
	}
#endif
	if (m_str_wchar)
	{
		delete[] m_str_wchar;
		m_str_wchar = NULL;
	}
}
//---------------------------------------------------------------------------//


//----------------------------------------------------------------------//

