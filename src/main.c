#include <tftp_types.h>
#include <tftp_server.h>
#include <tftp_utils.h>

uint32_t session_flag[MAX_SESSION] = {0};
uint32_t session_count = 0;
uint32_t write_flag = 0;


int main() {
    setColor(LIGHTCYAN);
    printPANDA();
    printHeader();
    
    // Initialize Winsock
    WSADATA wsaData;
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != NO_ERROR) {
        printf("WSAStartup failed with error: %d\n", iResult);
        return 1;
    }

    start_tftp_server();

    WSACleanup();
    return 0;
}