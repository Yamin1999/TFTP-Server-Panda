#ifdef _WIN32
#include <inttypes.h>
#else
#include <stdint.h>
#endif

#define FOPEN(fp, filename, mode) ((fp = fopen((const char *)filename, mode)))

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <io.h>
#include <time.h>


#ifdef _WIN32
#ifdef _MSC_VER
#pragma comment(lib, "Ws2_32.lib")
#endif
#endif

#define LIGHTGRAY 7
#define DARKGRAY 8
#define LIGHTBLUE 9
#define LIGHTGREEN 10
#define LIGHTCYAN 11
#define LIGHTRED 12
#define LIGHTMAGENTA 13

#define RRQ 1
#define WRQ 2
#define DATA 3
#define ACK 4
#define ERROR_TFTP 5
#define BAR_WIDTH 50


#define READ 1 
#define WRITE 2

#define ERR_NOT_DEFINE 0
#define ERR_FILE_NOT_FOUND 1
#define ERR_FILE_ALREADY_EXITS 6

#define RECV_TIMEOUT 5
#define RECV_RETRIES 10
#define TRANSFER_MODE_SIZE 10
#define MAX_FILENAME_SIZE 50

#define MAX_SESSION 10
#define BLOCKSIZE 512

#define BUFSIZE 65535

#define LITTLE_BUF 600

#define MAX_READ_SIZE 60000

#define PORT 69

#define READ_SESSION_CLOSE(fp, socket_fd_s, index)            \
    do                                                        \
    {                                                         \
        if (fp)                                               \
            fclose(fp);                                       \
        if (socket_fd_s != INVALID_SOCKET)                    \
            closesocket(socket_fd_s);                         \
        session_flag[index] = 0;                              \
        session_count--;                                      \
        memset(&sessions[index], 0, sizeof(sessions[index])); \
        return 0;                                             \
    } while (0)

#define WRITE_SESSION_CLOSE(fp, socket_fd_s, index)           \
    do                                                        \
    {                                                         \
        if (fp)                                               \
            fclose(fp);                                       \
        if (socket_fd_s != INVALID_SOCKET)                    \
            closesocket(socket_fd_s);                         \
        session_flag[index] = 0;                              \
        session_count--;                                      \
        write_flag = 0;                                       \
        memset(&sessions[index], 0, sizeof(sessions[index])); \
        return 0;                                             \
    } while (0)

#define println() printf("-----------------------------------------------------------------------------------------------------------------\n")


#pragma pack(push, 1) // Disable structure member alignment

typedef struct request
{
    uint16_t opcode;
    uint8_t filename_mode[0];
} request_pkt;

typedef struct data
{
    uint16_t opcode;
    uint16_t block_number;
    uint8_t data[0];
} data_pkt;

typedef struct ack
{
    uint16_t opcode;
    uint16_t block_number;
} ack_pkt;

typedef struct error
{
    uint16_t opcode;
    uint16_t error_code;
    uint8_t error_string[0];
} error_pkt;

typedef struct option_packet
{
    uint16_t opcode;
    uint8_t option[0];
} optn_pkt;

typedef struct sesion_headr
{
    struct sockaddr_in client_adderess;
    uint8_t filename[MAX_FILENAME_SIZE];
    uint8_t transfer_mode[TRANSFER_MODE_SIZE];
    uint8_t option[50];
    uint32_t option_flag;
    uint8_t tsize[20];
    uint32_t option_len;
    uint32_t block_size;
    uint32_t timeout;
    uint32_t current_block;
    uint32_t session_id;
    uint32_t index_count;
    uint32_t operation;
    // time_t last_send_time;
} session_t;

#pragma pack(pop) // Restore the previous structure member alignment

session_t sessions[MAX_SESSION];
extern uint32_t session_flag[MAX_SESSION];

extern uint32_t session_count;
extern uint32_t session_index_count;
extern uint32_t write_flag;

void setColor(int colorCode);
void printPANDA();
void printHeader();