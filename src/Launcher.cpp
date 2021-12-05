#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <io.h>
#include <Windows.h>
#include <winternl.h>
#include <TlHelp32.h>


using namespace std;


typedef NTSTATUS(NTAPI* pfnNtCreateThreadEx)
(
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

/************************************************************************
*  Name : InjectDll
*  Param: ProcessId		����Id
*  Ret  : BOOLEAN
*  ʹ��NtCreateThreadEx��Ŀ����̴����߳�ʵ��ע��
************************************************************************/
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


//prepare for call NtQueryInformationProcess func
typedef NTSTATUS(NTAPI* typedef_NtQueryInformationProcess)(
    IN HANDLE ProcessHandle,
    IN PROCESSINFOCLASS ProcessInformationClass,
    OUT PVOID ProcessInformation,
    IN ULONG ProcessInformationLength,
    OUT PULONG ReturnLength OPTIONAL
    );



wstring GetFolder(LPWSTR Dir)
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

VOID plog(const std::string str)
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


INT GetProcessThreadList() //���̵�ID 
{
    DWORD th32ProcessID = GetCurrentProcessId();
    HANDLE hThreadSnap;
    THREADENTRY32 th32;
    hThreadSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, th32ProcessID);
    if (hThreadSnap == INVALID_HANDLE_VALUE)
    {
        return -1;
    }
    th32.dwSize = sizeof(THREADENTRY32);
    if (!Thread32First(hThreadSnap, &th32))
    {
        CloseHandle(hThreadSnap);
        return -1;
    }
    do
    {
        if (th32.th32OwnerProcessID == th32ProcessID)
        {
            plog(to_string(th32.th32ThreadID));
            cout << th32.th32ThreadID << endl;
        }
    } while (Thread32Next(hThreadSnap, &th32));

    CloseHandle(hThreadSnap);
    return 0;
}



int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow)
//int main(int argc, LPWSTR lpCmdLine)
{

    LPWSTR sWinDir = new TCHAR[MAX_PATH];
    GetModuleFileNameW(NULL, sWinDir, MAX_PATH);
    (wcsrchr(sWinDir, '\\'))[1] = 0;
    //wstring sConLin = wstring(sWinDir) + L"notepad.exe";
    wstring sConLin = wstring(sWinDir) + GetFolder(sWinDir) + L"\\msedge.exe";
    wstring sDllPath = wstring(sWinDir) + L"iEdge.dll";
    wstring lpConLin = wstring(lpCmdLine);

    wchar_t Params[2048];
    GetPrivateProfileSectionW(L"��������", Params, 2048, wstring(sWinDir).append(L"iEdge.ini").c_str());
    sConLin.append(L" ").append(wstring(Params));
    wcout << sConLin << endl;
    
    //MessageBox(NULL, &lpConLin[0], L"", MB_OK);

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
        //cout << "create process success" << endl;

        InjectDll(pi.hProcess, &sDllPath[0]);
        //Sleep(100);
        ResumeThread(pi.hThread);
        //GetProcessThreadList();

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