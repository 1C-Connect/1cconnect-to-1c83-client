#pragma once
#ifndef __1CCONNECTINTEGRATION_H__
#define __1CCONNECTINTEGRATION_H__

#include "include\ComponentBase.h"
#include "include\AddInDefBase.h"
#include "include\IMemoryManager.h"
//#include <string>

//class
class C1cConnectIntegration : public IComponentBase
{
public:
	enum Props
	{
		ePropLogin = 0,
		ePropLast
	};
	enum Methods
	{
		eMethEnable = 0,
		eMethDisable,
		eMethShowMsgBox,
		eMethConnectToAgent,
		eMethCloseConnect,
		eSubColleaguesMessage,
		eMethSubSoftphone,
		eMethUnSubSoftphone,
		eMethCallSoftphone,
		eMethChangeStatus,
		eMethSubStatus,
		eMethUnSubStatus,
		eMethSubColleaguesCall,
		eMethUnSubColleaguesCall,
		eMethSubClientsCall,
		eMethUnSubClientsCall,
		eMethSubCall,
		eMethUnSubCall,
		eMethLast
	};
	enum Attributes
	{
		eID = 0,
		eAttribLast
	};

	C1cConnectIntegration(void);
	virtual ~C1cConnectIntegration();
	virtual bool ADDIN_API Init(void*);
	virtual bool ADDIN_API setMemManager(void* mem);
	virtual long ADDIN_API GetInfo();
	virtual void ADDIN_API Done();
	// ILanguageExtenderBase
	virtual bool ADDIN_API RegisterExtensionAs(WCHAR_T**);
	virtual long ADDIN_API GetNProps();
	virtual long ADDIN_API FindProp(const WCHAR_T* wsPropName);
	virtual const WCHAR_T* ADDIN_API GetPropName(long lPropNum, long lPropAlias);
	virtual bool ADDIN_API GetPropVal(const long lPropNum, tVariant* pvarPropVal);
	virtual bool ADDIN_API SetPropVal(const long lPropNum, tVariant* varPropVal);
	virtual bool ADDIN_API IsPropReadable(const long lPropNum);
	virtual bool ADDIN_API IsPropWritable(const long lPropNum);
	virtual long ADDIN_API GetNMethods();
	virtual long ADDIN_API FindMethod(const WCHAR_T* wsMethodName);
	virtual const WCHAR_T* ADDIN_API GetMethodName(const long lMethodNum,
		const long lMethodAlias);
	virtual long ADDIN_API GetNParams(const long lMethodNum);
	virtual bool ADDIN_API GetParamDefValue(const long lMethodNum, const long lParamNum,
		tVariant *pvarParamDefValue);
	virtual bool ADDIN_API HasRetVal(const long lMethodNum);
	virtual bool ADDIN_API CallAsProc(const long lMethodNum,
		tVariant* paParams, const long lSizeArray);
	virtual bool ADDIN_API CallAsFunc(const long lMethodNum,
		tVariant* pvarRetValue, tVariant* paParams, const long lSizeArray);
	// LocaleBase
	virtual void ADDIN_API SetLocale(const WCHAR_T* loc);
	bool wstring_to_p(std::wstring str, tVariant* val);

private:
	long findName(const wchar_t* names[], const wchar_t* name, const uint32_t size) const;
	void addError(uint32_t wcode, const wchar_t* source,
		const wchar_t* descriptor, long code);
	//	void subscribe();
	// Attributes
	IAddInDefBase      *m_iConnect;
	IMemoryManager     *m_iMemory;
	//WCHAR_T *commandtopipe[];

	bool				m_boolEnabled;
	std::wstring		login = L"", commandid = L"", phonenumber = L"", status = L"";
	uint32_t            m_uiTimer;
#if !defined( __linux__ ) && !defined(__APPLE__)
	HANDLE              m_hTimer;
	HANDLE              m_hTimerQueue;
#endif //__linux__
};

class WcharWrapper
{
public:
#ifdef LINUX_OR_MACOS
	WcharWrapper(const WCHAR_T* str);
#endif
	WcharWrapper(const wchar_t* str);
	~WcharWrapper();
#ifdef LINUX_OR_MACOS
	operator const WCHAR_T*() { return m_str_WCHAR; }
	operator WCHAR_T*() { return m_str_WCHAR; }
#endif
	operator const wchar_t*() { return m_str_wchar; }
	operator wchar_t*() { return m_str_wchar; }
private:
	WcharWrapper & operator = (const WcharWrapper& other) {};
	WcharWrapper(const WcharWrapper& other) {};
private:
#ifdef LINUX_OR_MACOS
	WCHAR_T* m_str_WCHAR;
#endif
	wchar_t* m_str_wchar;
};

#endif //__1CCONNECTINTEGRATION_H__