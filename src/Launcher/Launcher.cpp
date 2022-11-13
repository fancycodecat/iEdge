#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <io.h>
#include <Windows.h>
#include <winternl.h>
#include <TlHelp32.h>


using namespace std;

typedef NTSTATUS(NTAPI* pfnNtCreateThreadEx)(
    OUT PHANDLE hThread,
    IN ACCESS_MASK DesiredAccess,
    IN PVOID ObjectAttributes,
    IN HANDLE ProcessHandle,
    IN PVOID lpStartAddress,
    IN PVOID lpParameter,
    IN ULONG Flags,
    IN SIZE_T StackZeroBits,
    IN SIZE_T SizeOfStackCommit,
    IN SIZE_T SizeOfStackReserve,
    OUT PVOID lpBytesBuffer);


//ʹ��NtCreateThreadEx��Ŀ����̴����߳�ʵ��ע��
BOOL InjectDll(HANDLE ProcessHandle, LPWSTR DllFullPath)
{
    // �ڶԷ����̿ռ������ڴ�,�洢Dll����·��
    //UINT32	DllFullPathLength = (strlen(DllFullPath) + 1);
    size_t	DllFullPathLength = (wcslen(DllFullPath) + 1) * 2;
    PVOID 	DllFullPathBufferData = VirtualAllocEx(ProcessHandle, NULL, DllFullPathLength, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (DllFullPathBufferData == NULL)
    {
        CloseHandle(ProcessHandle);
        return FALSE;
    }

    // ��DllFullPathд���ո�������ڴ���
    SIZE_T	ReturnLength;
    BOOL bOk = WriteProcessMemory(ProcessHandle, DllFullPathBufferData, DllFullPath, DllFullPathLength, &ReturnLength);

    LPTHREAD_START_ROUTINE	LoadLibraryAddress = NULL;
    HMODULE					Kernel32Module = GetModuleHandle(L"Kernel32");

    LoadLibraryAddress = (LPTHREAD_START_ROUTINE)GetProcAddress(Kernel32Module, "LoadLibraryW");

    pfnNtCreateThreadEx NtCreateThreadEx = (pfnNtCreateThreadEx)GetProcAddress(GetModuleHandle(L"ntdll.dll"), "NtCreateThreadEx");
    if (NtCreateThreadEx == NULL)
    {
        CloseHandle(ProcessHandle);
        return FALSE;
    }

    HANDLE ThreadHandle = NULL;
    // 0x1FFFFF #define THREAD_ALL_ACCESS         (STANDARD_RIGHTS_REQUIRED | SYNCHRONIZE | 0xFFFF)
    NtCreateThreadEx(&ThreadHandle, 0x1FFFFF, NULL, ProcessHandle, (LPTHREAD_START_ROUTINE)LoadLibraryAddress, DllFullPathBufferData, FALSE, NULL, NULL, NULL, NULL);
    if (ThreadHandle == NULL)
    {
        CloseHandle(ProcessHandle);
        return FALSE;
    }

    if (WaitForSingleObject(ThreadHandle, INFINITE) == WAIT_FAILED)
    {
        return FALSE;
    }

    CloseHandle(ProcessHandle);
    CloseHandle(ThreadHandle);

    return TRUE;
}

wstring GetAppFolder(LPWSTR Dir)
{
    //�ļ����
    //ע�⣺�ҷ�����Щ���´���˴���long���ͣ�ʵ�������лᱨ������쳣
    intptr_t hFile = 0;
    //�ļ���Ϣ
    struct _wfinddata_t fileinfo;
    wstring p;
    if ((hFile = _wfindfirst(p.assign(Dir).append(L"\\*").c_str(), &fileinfo)) != -1)
    {
        do
        {
            if ((fileinfo.attrib & _A_SUBDIR))
            {
                wstring filename = fileinfo.name;
                if (count(filename.begin(), filename.end(), '.') == 3)
                    return filename;
            }
        } while (_wfindnext(hFile, &fileinfo) == 0);
        _findclose(hFile);
    }
    return wstring(L"");
}

wstring GetDataFolder(LPWSTR Dir)
{
    //�ļ����
    //ע�⣺�ҷ�����Щ���´���˴���long���ͣ�ʵ�������лᱨ������쳣
    intptr_t hFile = 0;
    //�ļ���Ϣ
    struct _wfinddata_t fileinfo;
    wstring p;
    if ((hFile = _wfindfirst(p.assign(Dir).append(L"\\*").c_str(), &fileinfo)) != -1)
    {
        do
        {
            if ((fileinfo.attrib & _A_SUBDIR))
            {
                wstring filename = fileinfo.name;
                if (filename.length() > 1 && count(filename.begin(), filename.end(), '.') == 1)
                    return filename;
            }
        } while (_wfindnext(hFile, &fileinfo) == 0);
        _findclose(hFile);
    }
    return wstring(L"");
}

void plog(const std::string str)
{
    char time[4096];
    SYSTEMTIME sys;
    GetLocalTime(&sys);
    sprintf_s(time, "[%4d/%02d/%02d %02d:%02d:%02d.%03d]\n",
        sys.wYear, sys.wMonth, sys.wDay, sys.wHour, sys.wMinute,
        sys.wSecond, sys.wMilliseconds);
    std::ofstream	OsWrite("plogs.txt", std::ofstream::app);
    OsWrite << time;
    OsWrite << str;
    OsWrite << std::endl;
    OsWrite.close();
}


int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow)
{

    LPWSTR sWinDir = new TCHAR[MAX_PATH];
    GetModuleFileNameW(NULL, sWinDir, MAX_PATH);
    (wcsrchr(sWinDir, '\\'))[1] = 0;

    wstring sConLin = wstring(sWinDir) + GetAppFolder(sWinDir) + L"\\msedge.exe";
    wstring sDllPath = wstring(sWinDir) + L"iEdge.dll";
    wstring lpConLin = wstring(lpCmdLine);

    DWORD dwAttrib = GetFileAttributes(sConLin.c_str());
    if(INVALID_FILE_ATTRIBUTES == dwAttrib || 0 != (dwAttrib & FILE_ATTRIBUTE_DIRECTORY))
        sConLin = wstring(sWinDir) + GetAppFolder(sWinDir) + L"\\chrome.exe";

    wchar_t Params[2048];
    GetPrivateProfileSectionW(L"��������", Params, 2048, wstring(sWinDir).append(L"iEdge.ini").c_str());
    wstring sParams = Params;
    size_t pIndex = sParams.find(L'"', sParams.find(L'"')+1);
    if (pIndex > 0 && pIndex != sParams.npos && sParams[pIndex-1] == L'.')
    {
        std::hash<std::wstring> hasher;
        size_t hWinDir = hasher(sWinDir);
        wstring hashStr = to_wstring(hWinDir).substr(0, 4);
        sParams.insert(pIndex, hashStr);

        wstring sData = wstring(sWinDir) + GetDataFolder(sWinDir);
        wstring nData = sParams.substr(sParams.find(L'"'), pIndex-sParams.find(L'"'));
        nData = wstring(sWinDir) + nData.substr(nData.rfind(L'\\')+1) + hashStr;
        
        if (sData != nData)
            _wrename(sData.c_str(), nData.c_str());
    }

    sConLin.append(L" ").append(sParams);


    STARTUPINFO si;
    PROCESS_INFORMATION pi;

    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));
    si.cb = sizeof(si);

    //����һ���½���  
    if (CreateProcessW(
        NULL,   //  ָ��һ��NULL��β�ġ�����ָ����ִ��ģ��Ŀ��ֽ��ַ���  
        &sConLin[0], // �������ַ���  
        NULL, //    ָ��һ��SECURITY_ATTRIBUTES�ṹ�壬����ṹ������Ƿ񷵻صľ�����Ա��ӽ��̼̳С�  
        NULL, //    ���lpProcessAttributes����Ϊ�գ�NULL������ô������ܱ��̳С�<ͬ��>  
        false,//    ָʾ�½����Ƿ�ӵ��ý��̴��̳��˾����   
        CREATE_SUSPENDED,  //  ָ�����ӵġ���������������ͽ��̵Ĵ����ı�  
            //  CREATE_NEW_CONSOLE  �¿���̨���ӽ���  
            //  CREATE_SUSPENDED    �ӽ��̴��������ֱ������ResumeThread����  
        NULL, //    ָ��һ���½��̵Ļ����顣����˲���Ϊ�գ��½���ʹ�õ��ý��̵Ļ���  
        NULL, //    ָ���ӽ��̵Ĺ���·��  
        &si, // �����½��̵������������ʾ��STARTUPINFO�ṹ��  
        &pi  // �����½��̵�ʶ����Ϣ��PROCESS_INFORMATION�ṹ��  
    ))
    {

        InjectDll(pi.hProcess, &sDllPath[0]);

        ResumeThread(pi.hThread);

        //�������йرվ������������̺��½��̵Ĺ�ϵ����Ȼ�п��ܲ�С�ĵ���TerminateProcess�����ص��ӽ���
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }

    //��ֹ�ӽ���
    //TerminateProcess(pi.hProcess, 300);

    //��ֹ�����̣�״̬��
    //ExitProcess(1001);

    return 0;
}