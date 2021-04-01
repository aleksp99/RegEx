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

#include <stdio.h>
#include <wchar.h>
#include "RegEx.h"
#include <string>
#include <clocale>

#define TIME_LEN 65

#define BASE_ERRNO     7

#ifdef WIN32
//#pragma setlocale("")
#endif

static const wchar_t *g_PropNames[] = { 
	L"Pattern",
    L"Text",
    L"Position",
    L"Length",
    L"Value"
};

static const wchar_t *g_PropNamesRu[] = {
	L"Шаблон",
    L"Текст",
    L"Позиция",
    L"Длина",
    L"Значение"
};
static const wchar_t *g_MethodNames[] = {
	L"Version",
	L"Search"
};

static const wchar_t *g_MethodNamesRu[] = {
	L"Версия",
	L"Найти"
};

boost::wregex regex_expr;
std::wstring::const_iterator start, end;
boost::match_results<std::wstring::const_iterator> result;
std::wstring pattern;
std::wstring wtext;
uint8_t position = 0;

const wchar_t* kComponentVersion = L"1.0";

static const wchar_t g_kClassNames[] = L"Boost";
static IAddInDefBase *pAsyncEvent = NULL;

uint32_t convToShortWchar(WCHAR_T** Dest, const wchar_t* Source, uint32_t len = 0);
uint32_t convFromShortWchar(wchar_t** Dest, const WCHAR_T* Source, uint32_t len = 0);
uint32_t getLenShortWcharStr(const WCHAR_T* Source);
static AppCapabilities g_capabilities = eAppCapabilitiesInvalid;
static WcharWrapper s_names(g_kClassNames);

long GetClassObject(const WCHAR_T* wsName, IComponentBase** pInterface){
    if(!*pInterface){
        *pInterface= new RegEx;
        return (long)*pInterface;
    }
    return 0;
}

AppCapabilities SetPlatformCapabilities(const AppCapabilities capabilities){
    g_capabilities = capabilities;
    return eAppCapabilitiesLast;
}

long DestroyObject(IComponentBase** pIntf){
    if(!*pIntf)
        return -1;

    delete *pIntf;
    *pIntf = 0;
    return 0;
}

const WCHAR_T* GetClassNames(){
    return s_names;
}

#if !defined( __linux__ ) && !defined(__APPLE__)
VOID CALLBACK MyTimerProc(PVOID lpParam, BOOLEAN TimerOrWaitFired);
#else
static void MyTimerProc(int sig);
#endif //__linux__

RegEx::RegEx(){
    m_iMemory = 0;
    m_iConnect = 0;
#if !defined( __linux__ ) && !defined(__APPLE__)
    m_hTimerQueue = 0;
#endif //__linux__
}

RegEx::~RegEx(){}

bool RegEx::Init(void* pConnection){ 
    m_iConnect = (IAddInDefBase*)pConnection;
    return m_iConnect != NULL;
}

long RegEx::GetInfo(){
    return 2000; 
}

void RegEx::Done(){
#if !defined( __linux__ ) && !defined(__APPLE__)
    if(m_hTimerQueue ){
        DeleteTimerQueue(m_hTimerQueue);
        m_hTimerQueue = 0;
    }
#endif //__linux__
}

bool RegEx::RegisterExtensionAs(WCHAR_T** wsExtensionName){ 
    const wchar_t *wsExtension = L"Regex";
    int iActualSize = ::wcslen(wsExtension) + 1;
    WCHAR_T* dest = 0;

    if (m_iMemory){
        if(m_iMemory->AllocMemory((void**)wsExtensionName, iActualSize * sizeof(WCHAR_T)))
            ::convToShortWchar(wsExtensionName, wsExtension, iActualSize);
        return true;
    }

    return false; 
}

long RegEx::GetNProps(){ 
    return ePropLast;
}

long RegEx::FindProp(const WCHAR_T* wsPropName){ 
    long plPropNum = -1;
    wchar_t* propName = 0;

    ::convFromShortWchar(&propName, wsPropName);
    plPropNum = findName(g_PropNames, propName, ePropLast);

    if (plPropNum == -1)
        plPropNum = findName(g_PropNamesRu, propName, ePropLast);

    delete[] propName;

    return plPropNum;
}

const WCHAR_T* RegEx::GetPropName(long lPropNum, long lPropAlias){ 
    if (lPropNum >= ePropLast)
        return NULL;

    wchar_t *wsCurrentName = NULL;
    WCHAR_T *wsPropName = NULL;
    int iActualSize = 0;

    switch(lPropAlias){
    case 0:
        wsCurrentName = (wchar_t*)g_PropNames[lPropNum];
        break;
    case 1:
        wsCurrentName = (wchar_t*)g_PropNamesRu[lPropNum];
        break;
    default:
        return 0;
    }
    
    iActualSize = wcslen(wsCurrentName) + 1;

    if (m_iMemory && wsCurrentName){
        if (m_iMemory->AllocMemory((void**)&wsPropName, iActualSize * sizeof(WCHAR_T)))
            ::convToShortWchar(&wsPropName, wsCurrentName, iActualSize);
    }

    return wsPropName;
}

bool RegEx::GetPropVal(const long lPropNum, tVariant* pvarPropVal)
{ 
    switch(lPropNum){
    case ePropPattern:
        wstring_to_p(pattern, pvarPropVal);
        return true;
    case ePropText:
        wstring_to_p(wtext, pvarPropVal);
        return true;
    case ePropPosition:
        if (position != 0) {
            TV_VT(pvarPropVal) = VTYPE_UI1;
            TV_UI1(pvarPropVal) = position;
        }
        return true;
    case ePropLength:
        if (position != 0) {
            TV_VT(pvarPropVal) = VTYPE_UI1;
            TV_UI1(pvarPropVal) = result.length();
        }
        return true;
    case ePropValue:
        if (position != 0)
            wstring_to_p(result.str(), pvarPropVal);
        return true;
	default:
        return false;
    }
}

bool RegEx::SetPropVal(const long lPropNum, tVariant *varPropVal){ 
	switch (lPropNum) {
	case ePropPattern: {
		if (TV_VT(varPropVal) != VTYPE_PWSTR)
			return false;
        pattern = (wchar_t*)(varPropVal->pstrVal);
		regex_expr = boost::wregex(std::wstring(pattern.begin(), pattern.end()));//boost::regex::perl
        if (wtext.length() != 0) {
            start = wtext.begin();
            end = wtext.end();
            position = 0;
        }
		return true;
	}
	case ePropText: {
		if (TV_VT(varPropVal) != VTYPE_PWSTR)
			return false;
        wtext = (wchar_t*)(varPropVal->pstrVal);
		start = wtext.begin();
		end = wtext.end();
		position = 0;
		return true;
	}
	default:
		return false;
	}
}

bool RegEx::IsPropReadable(const long lPropNum){ 
    switch (lPropNum) {
    case ePropPattern: return true;
    case ePropText: return true;
    case ePropPosition: return true;
    case ePropLength: return true;
    case ePropValue: return true;
    
    default: return false;
    }
}

bool RegEx::IsPropWritable(const long lPropNum){
    switch (lPropNum) {
    case ePropPattern: return true;
    case ePropText: return true;

    default: return false;
    }
}

long RegEx::GetNMethods(){ 
    return eMethLast;
}

long RegEx::FindMethod(const WCHAR_T* wsMethodName){ 
    long plMethodNum = -1;
    wchar_t* name = 0;

    ::convFromShortWchar(&name, wsMethodName);

    plMethodNum = findName(g_MethodNames, name, eMethLast);

    if (plMethodNum == -1)
        plMethodNum = findName(g_MethodNamesRu, name, eMethLast);

    delete[] name;

    return plMethodNum;
}

const WCHAR_T* RegEx::GetMethodName(const long lMethodNum, const long lMethodAlias){ 
    if (lMethodNum >= eMethLast)
        return NULL;

    wchar_t *wsCurrentName = NULL;
    WCHAR_T *wsMethodName = NULL;
    int iActualSize = 0;

    switch(lMethodAlias){
    case 0:
        wsCurrentName = (wchar_t*)g_MethodNames[lMethodNum];
        break;
    case 1:
        wsCurrentName = (wchar_t*)g_MethodNamesRu[lMethodNum];
        break;
    default: 
        return 0;
    }

    iActualSize = wcslen(wsCurrentName) + 1;

    if (m_iMemory && wsCurrentName){
        if(m_iMemory->AllocMemory((void**)&wsMethodName, iActualSize * sizeof(WCHAR_T)))
            ::convToShortWchar(&wsMethodName, wsCurrentName, iActualSize);
    }

    return wsMethodName;
}

long RegEx::GetNParams(const long lMethodNum){ 
    return 0;
}

bool RegEx::GetParamDefValue(const long lMethodNum, const long lParamNum,
                        tVariant *pvarParamDefValue){ 
    return false;
} 

bool RegEx::HasRetVal(const long lMethodNum){ 
    switch(lMethodNum){
	case eVersion: return true;
	case eMethSearch: return true;
    
    default: return false;
    }

    return false;
}

bool RegEx::CallAsProc(const long lMethodNum,
                    tVariant* paParams, const long lSizeArray){ 
    return true;
}

bool RegEx::CallAsFunc(const long lMethodNum,
                tVariant* pvarRetValue, tVariant* paParams, const long lSizeArray)
{ 
	switch(lMethodNum){
	case eVersion:{
		if (pvarRetValue){
			size_t strLen = wcslen(kComponentVersion);
			if (m_iMemory->AllocMemory((void**)&pvarRetValue->pwstrVal, (strLen + 1) * sizeof(kComponentVersion[0]))){
				wcscpy_s(pvarRetValue->pwstrVal, strLen + 1, kComponentVersion);
				pvarRetValue->wstrLen = strLen;
				TV_VT(pvarRetValue) = VTYPE_PWSTR;
			}
		}
        return true;
    }
    case eMethSearch: {
        if (pattern.length() == 0 | wtext.length() == 0)
            return false;
        if (result.size() != 0)
            position += result.length();
        if (boost::regex_search(start, end, result, regex_expr)) {
            if (position == 0)
                position++;
            position += result.position();
            start = result[0].second;
            pvarRetValue->bVal = true;
            TV_VT(pvarRetValue) = VTYPE_BOOL;
        }
        else {
            position = 0;
            pvarRetValue->bVal = false;
            TV_VT(pvarRetValue) = VTYPE_BOOL;
        }
        return true;
    }
    default:
        return false;
    }
}

// This code will work only on the client!
#if !defined( __linux__ ) && !defined(__APPLE__)
VOID CALLBACK MyTimerProc(PVOID lpParam, BOOLEAN TimerOrWaitFired)
{
    if (!pAsyncEvent)
        return;
    DWORD dwTime = 0;
    wchar_t *who = L"ComponentNative", *what = L"Timer";

    wchar_t *wstime = new wchar_t[TIME_LEN];
    if (wstime){
        wmemset(wstime, 0, TIME_LEN);
        time_t vtime;
        time(&vtime);
        ::_ui64tow_s(vtime, wstime, TIME_LEN, 10);
        pAsyncEvent->ExternalEvent(who, what, wstime);
        delete[] wstime;
    }
}
#else
void MyTimerProc(int sig)
{
    if (!pAsyncEvent)
        return;

    WCHAR_T *who = 0, *what = 0, *wdata = 0;
    wchar_t *data = 0;
    time_t dwTime = time(NULL);

    data = new wchar_t[TIME_LEN];
    
    if (data)
    {
        wmemset(data, 0, TIME_LEN);
        swprintf(data, TIME_LEN, L"%ul", dwTime);
        ::convToShortWchar(&who, L"ComponentNative");
        ::convToShortWchar(&what, L"Timer");
        ::convToShortWchar(&wdata, data);

        pAsyncEvent->ExternalEvent(who, what, wdata);

        delete[] who;
        delete[] what;
        delete[] wdata;
        delete[] data;
    }
}
#endif

void RegEx::SetLocale(const WCHAR_T* loc){
#if !defined( __linux__ ) && !defined(__APPLE__)
    _wsetlocale(LC_ALL, loc);
#else
    //We convert in char* char_locale
    //also we establish locale
    //setlocale(LC_ALL, "");
#endif
}

bool RegEx::setMemManager(void* mem){
    m_iMemory = (IMemoryManager*)mem;
    return m_iMemory != 0;
}

void RegEx::addError(uint32_t wcode, const wchar_t* source,
                        const wchar_t* descriptor, long code){
    if (m_iConnect){
        WCHAR_T *err = 0;
        WCHAR_T *descr = 0;
        
        ::convToShortWchar(&err, source);
        ::convToShortWchar(&descr, descriptor);

        m_iConnect->AddError(wcode, err, descr, code);
        delete[] err;
        delete[] descr;
    }
}

long RegEx::findName(const wchar_t* names[], const wchar_t* name, 
                        const uint32_t size) const {
    long ret = -1;
    for (uint32_t i = 0; i < size; i++) {
        if (!wcscmp(names[i], name)) {
            ret = i;
            break;
        }
    }
    return ret;
}

uint32_t convToShortWchar(WCHAR_T** Dest, const wchar_t* Source, uint32_t len){
    if (!len)
        len = ::wcslen(Source) + 1;

    if (!*Dest)
        *Dest = new WCHAR_T[len];

    WCHAR_T* tmpShort = *Dest;
    wchar_t* tmpWChar = (wchar_t*) Source;
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
        if(succeed != (size_t)-1)
            return (uint32_t)succeed;
    }
#endif //__linux__
    for (; len; --len, ++res, ++tmpWChar, ++tmpShort)
        *tmpShort = (WCHAR_T)*tmpWChar;
    
    return res;
}

uint32_t convFromShortWchar(wchar_t** Dest, const WCHAR_T* Source, uint32_t len){
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
        if(succeed != (size_t)-1)
            return (uint32_t)succeed;
    }
#endif //__linux__
    for (; len; --len, ++res, ++tmpWChar, ++tmpShort)
        *tmpWChar = (wchar_t)*tmpShort;

    return res;
}

uint32_t getLenShortWcharStr(const WCHAR_T* Source){
    uint32_t res = 0;
    WCHAR_T *tmpShort = (WCHAR_T*)Source;

    while (*tmpShort++)
        ++res;

    return res;
}

#ifdef LINUX_OR_MACOS
WcharWrapper::WcharWrapper(const WCHAR_T* str) : m_str_WCHAR(NULL),
                           m_str_wchar(NULL)
{
    if (str)
    {
        int len = getLenShortWcharStr(str);
        m_str_WCHAR = new WCHAR_T[len + 1];
        memset(m_str_WCHAR,   0, sizeof(WCHAR_T) * (len + 1));
        memcpy(m_str_WCHAR, str, sizeof(WCHAR_T) * len);
        ::convFromShortWchar(&m_str_wchar, m_str_WCHAR);
    }
}
#endif

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

WcharWrapper::~WcharWrapper()
{
#ifdef LINUX_OR_MACOS
    if (m_str_WCHAR)
    {
        delete [] m_str_WCHAR;
        m_str_WCHAR = NULL;
    }
#endif
    if (m_str_wchar)
    {
        delete [] m_str_wchar;
        m_str_wchar = NULL;
    }
}

bool RegEx::wstring_to_p(std::wstring str, tVariant* val) {
    char* t1;
    TV_VT(val) = VTYPE_PWSTR;
    m_iMemory->AllocMemory((void**)&t1, (str.length() + 1) * sizeof(WCHAR_T));
    memcpy(t1, str.c_str(), (str.length() + 1) * sizeof(WCHAR_T));
    val->pstrVal = t1;
    val->strLen = str.length();
    return true;
}