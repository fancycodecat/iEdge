#pragma data_seg(".SHARED")
#include <string>

bool HoverActivateTab = false;
int HoverTime = HOVER_DEFAULT;
bool RightTabSwitch = false;
bool OpenUrlNewTab = false;
bool MouseGesture = false;
std::string HomePage;

#pragma data_seg()
#pragma comment(linker, "/section:.SHARED,RWS")

void ReadConfig(const wchar_t *iniPath)
{
    HoverActivateTab = GetPrivateProfileInt(L"������ǿ", L"��ͣ�����ǩҳ", 0, iniPath) == 1;
    HoverTime = GetPrivateProfileInt(L"������ǿ", L"��ͣʱ��", HOVER_DEFAULT, iniPath);
    RightTabSwitch = GetPrivateProfileInt(L"������ǿ", L"�Ҽ����ٱ�ǩ�л�", 1, iniPath) == 1;
    OpenUrlNewTab = GetPrivateProfileInt(L"������ǿ", L"�±�ǩ����ַ", 0, iniPath) == 1;

    MouseGesture = GetPrivateProfileInt(L"�������", L"����", 1, iniPath) == 1;

    //MessageBox(NULL, std::to_wstring(MouseGesture).c_str(), L"", MB_OK);
    
}

std::wstring CheckArgs(const wchar_t* iniPath)
{
    wchar_t Params[2048];
    GetPrivateProfileSection(L"��������", Params, 2048, iniPath);
    std::wstring Args = GetCommandLineW();
    int index = Args.find(L".exe") + 6;
    std::wstring Path = Args.substr(0, index);
    Args = Args.substr(min(Args.length(), index), std::string::npos);
    if (Args.length() < 5)
    {
        std::wstring exec = Path + Params;
        std::string str(exec.begin(), exec.end());
        WinExec(str.c_str(), SW_SHOW);
        //ShellExecuteW(NULL, L"open", exec.c_str(), L"", L"", SW_HIDE);
        //RunExecute(exec.c_str());
        ExitProcess(0);
    }
    return Args;
}
