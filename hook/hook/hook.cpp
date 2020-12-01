#include "stdafx.h"
#include "hook.h"
#include <windows.h>
#include <stdio.h>
#include <CONIO.H>
#include <Winsock2.h>

//�������½ṹ������һ��InlineHook����Ҫ����Ϣ
typedef struct _HOOK_DATA {
	char szApiName[128];		//��Hook��API����
	char szModuleName[64];		//��Hook��API����ģ�������
	int  HookCodeLen;			//Hook����
	BYTE oldEntry[16];			//����Hookλ�õ�ԭʼָ��
	BYTE newEntry[16];			//����Ҫд��Hookλ�õ���ָ��
	ULONG_PTR HookPoint;		//��HOOK��λ��
	ULONG_PTR JmpBackAddr;		//������ԭ�����е�λ��
	ULONG_PTR pfnTrampolineFun;	//����ԭʼ������ͨ��
	ULONG_PTR pfnDetourFun;		//HOOK���˺���
}HOOK_DATA, *PHOOK_DATA;
HOOK_DATA RecvHookData, SendHookData;

//�ص�������ָ�루�ú���λ��c#�У�
CallBackFun1 RecvCallBack = NULL;
CallBackFun2 SendCallBack = NULL;

//��Ч��HOOKǰ��recv��send�ĺ�����ָ��
PFN_Recv OriginalRecv = NULL;
PFN_Send OriginalSend = NULL;

//����
int WINAPI My_Recv(SOCKET s, char *buf, int len, int flags);
int WINAPI My_Send(SOCKET s, const char *buf, int len, int flags);
BOOL Inline_InstallHook_Recv();
BOOL Inline_InstallHook_Send();
LPVOID GetAddress(char *, char *);
void InitHookEntry(PHOOK_DATA pHookData);
VOID InitTrampoline(PHOOK_DATA pHookData);
BOOL InstallCodeHook(PHOOK_DATA pHookData);
void SetRecvCallBack(CallBackFun1 pFun);
void SetSendCallBack(CallBackFun2 pFun);


int WINAPI My_Recv(SOCKET s, char *buf, int len, int flags)
{
	int ret = OriginalRecv(s, buf, len, flags);
	if (ret > 0) {
		if (RecvCallBack) {
			RecvCallBack(s, buf, ret);
		}
	}
	return ret;
}

int WINAPI My_Send(SOCKET s, const char *buf, int len, int flags)
{
	/*int ret = OriginalSend(s, buf, len, flags);
	if (ret > 0) {
		if (SendCallBack) {
			SendCallBack(s, buf, ret);
		}
	}
	return ret;*/

	return SendCallBack(s, buf, len);
}

BOOL Inline_InstallHook_Recv()
{
	ZeroMemory(&RecvHookData, sizeof(HOOK_DATA));
	strcpy_s(RecvHookData.szApiName, "recv");
	strcpy_s(RecvHookData.szModuleName, "ws2_32.dll");
	RecvHookData.HookCodeLen = 15;
	RecvHookData.HookPoint = (ULONG_PTR)GetAddress(RecvHookData.szModuleName, RecvHookData.szApiName);//HOOK�ĵ�ַ
																									  //MsgBoxHookData.pfnOriginalFun = (PVOID)OriginalMessageBox;//����ԭʼ������ͨ��
																									  //x64�²�����������ˣ���������һ���ڴ�����TrampolineFun��shellcode
	RecvHookData.pfnTrampolineFun = (ULONG_PTR)VirtualAlloc(NULL, 128, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
	RecvHookData.pfnDetourFun = (ULONG_PTR)My_Recv;//�Զ���hook����
	BOOL result = InstallCodeHook(&RecvHookData);
	OriginalRecv = (PFN_Recv)RecvHookData.pfnTrampolineFun;			//�൱��HOOKǰ��recv����
	return result;
}

BOOL Inline_InstallHook_Send()
{
	ZeroMemory(&SendHookData, sizeof(HOOK_DATA));
	strcpy_s(SendHookData.szApiName, "send");
	strcpy_s(SendHookData.szModuleName, "ws2_32.dll");
	SendHookData.HookCodeLen = 15;
	SendHookData.HookPoint = (ULONG_PTR)GetAddress(SendHookData.szModuleName, SendHookData.szApiName);//HOOK�ĵ�ַ
	SendHookData.pfnTrampolineFun = (ULONG_PTR)VirtualAlloc(NULL, 128, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
	SendHookData.pfnDetourFun = (ULONG_PTR)My_Send;//�Զ���hook����
	BOOL result = InstallCodeHook(&SendHookData);
	OriginalSend = (PFN_Send)SendHookData.pfnTrampolineFun;			//�൱��HOOKǰ��send����
	return result;
}

//��ȡָ��ģ����ָ��API�ĵ�ַ
LPVOID GetAddress(char *dllname, char *funname)
{
	HMODULE hMod = 0;
	if (hMod = GetModuleHandle(dllname))
	{
		printf("%s���Ѽ���\n", dllname);
		return GetProcAddress(hMod, funname);
	}
	else
	{
		printf("�ɹ�����%s\n", dllname);
		hMod = LoadLibrary(dllname);
		return GetProcAddress(hMod, funname);
	}
}

/*
�����ڵ�ָ��
ʹ�õ���mov rax xxxxx; jmp rax������12
Ϊ�����ָ����м������Ҫ����3��nop
*/
void InitHookEntry(PHOOK_DATA pHookData)
{
	pHookData->newEntry[0] = 0x48;
	pHookData->newEntry[1] = 0xb8;
	*(ULONG_PTR*)(pHookData->newEntry + 2) = (ULONG_PTR)pHookData->pfnDetourFun;
	pHookData->newEntry[10] = 0xff;
	pHookData->newEntry[11] = 0xe0;
	pHookData->newEntry[12] = 0x90;
	pHookData->newEntry[13] = 0x90;
	pHookData->newEntry[14] = 0x90;
}


/*
�����hook��ĺ����лص�ԭ�к�����ָ��
��ԭ����������ڵ�ָ�����һ��jmp����
ԭ������ڵ�ָ�
48 89 5C 24 08       mov     [rsp+arg_0], rbx
48 89 6C 24 10       mov     [rsp+arg_8], rbp
44 89 4C 24 20       mov     [rsp+arg_18], r9d
*/
VOID InitTrampoline(PHOOK_DATA pHookData)
{
	//����ǰ15�ֽ�
	PBYTE pFun = (PBYTE)pHookData->pfnTrampolineFun;
	memcpy(pFun, (PVOID)pHookData->HookPoint, 15);

	//�ں������һ����תָ��
	pFun += 15; //����ǰ����ָ��
	pFun[0] = 0xFF;
	pFun[1] = 0x25;
	*(ULONG_PTR*)(pFun + 6) = pHookData->JmpBackAddr;
}


BOOL InstallCodeHook(PHOOK_DATA pHookData)
{
	SIZE_T dwBytesReturned = 0;
	HANDLE hProcess = GetCurrentProcess();
	BOOL bResult = FALSE;
	if (pHookData == NULL
		|| pHookData->HookPoint == 0
		|| pHookData->pfnDetourFun == NULL
		|| pHookData->pfnTrampolineFun == NULL)
	{
		return FALSE;
	}
	pHookData->JmpBackAddr = pHookData->HookPoint + pHookData->HookCodeLen;
	LPVOID OriginalAddr = (LPVOID)pHookData->HookPoint;
	printf("Address To HOOK=0x%p\n", OriginalAddr);
	InitHookEntry(pHookData);//���Inline Hook����
	InitTrampoline(pHookData);//����Trampoline
	if (ReadProcessMemory(hProcess, OriginalAddr, pHookData->oldEntry, pHookData->HookCodeLen, &dwBytesReturned))	//��ȡ������ԭ����ڵ�ļ���ָ��
	{
		if (WriteProcessMemory(hProcess, OriginalAddr, pHookData->newEntry, pHookData->HookCodeLen, &dwBytesReturned))
		{
			printf("Install Hook write OK! WrittenCnt=%lld\n", dwBytesReturned);
			bResult = TRUE;
		}
	}
	return bResult;
}

void SetRecvCallBack(CallBackFun1 pFun) {
	RecvCallBack = pFun;
}

void SetSendCallBack(CallBackFun2 pFun) {
	SendCallBack = pFun;
}

int WINAPI RealSend(SOCKET s, const char *buf, int len) {
	return OriginalSend(s, buf, len, 0);		//flags���ô���c#���ˣ�ֱ��Ĭ��0����
}