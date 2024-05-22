#ifdef _WIN32
#include <inttypes.h>
#else
#include <stdint.h>
#endif

#define FOPEN(fp, filename, mode) ((fp = fopen(filename, mode)) == NULL)

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

#pragma comment(lib, "Ws2_32.lib")

#define RRQ 1
#define WRQ 2
#define DATA 3
#define ACK 4
#define ERROR_TFTP 5
#define BAR_WIDTH 50

#define ERR_FILE_NOT_FOUND 1
#define ERR_FILE_ALREADY_EXITS 6

#define RECV_TIMEOUT 5
#define RECV_RETRIES 10
#define TRANSFER_MODE_SIZE 10
#define MAX_FILENAME_SIZE 50

#define MAX_SESSION 10
#define BLOCKSIZE 512

#define BUFSIZE 10000

#define LITTLE_BUF 600

#define MAX_READ_SIZE 9500

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
    // time_t last_send_time;
} session_t;

#pragma pack(pop) // Restore the previous structure member alignment

session_t sessions[MAX_SESSION];
uint32_t session_flag[MAX_SESSION] = {0};

uint32_t session_count = 0;
uint32_t write_flag = 0;

// Function prototypes
void filename_mode_option_fetch(uint8_t *buff, int session_id);
void send_error(SOCKET socket_f, uint16_t error_code, uint8_t *error_string, struct sockaddr_in *client_sock, int slen);
void tftp_send_data(SOCKET socket_f, uint16_t block_number, uint8_t *data, int dlen, struct sockaddr_in *client_sock, int slen);
void tftp_send_ack(SOCKET socket_f, uint16_t block_number, struct sockaddr_in *client_sock, int slen);
DWORD WINAPI RRQ_func(LPVOID arg);
DWORD WINAPI WRQ_func(LPVOID arg);
void printHeader()
{
    printf("=============== TFTP Server v1.0.1 =====================\n");
    printf("*                                                      *\n");
    printf("*   Md. Yamin Haque                                    *\n");
    printf("*   R&D Engineer                                       *\n");
    printf("*   Shanghai BDCOM Information Technology Co., Ltd.    *\n");
    printf("*   (Bangladesh office)                                *\n");
    printf("*   yamin.haque@bdcom.com.cn                           *\n");
    printf("*                                                      *\n");
    printf("*======================================================*\n");
}

void print_progress(size_t count, size_t max, int index)
{
    char bar[BAR_WIDTH + 1];
    float progress = (float)count / max;
    int pos = progress * BAR_WIDTH;

    memset(bar, '#', pos);
    memset(bar + pos, ' ', BAR_WIDTH - pos);
    bar[BAR_WIDTH] = '\0';

    printf("\rSession %d : Progress: [%s] %.2f%%", index, bar, progress * 100);
    fflush(stdout);
}

void print_progress_write(size_t count, size_t max, int index)
{
    printf("\rSession %d : Received %zu blocks: %zu bytes", index, count, max);
    fflush(stdout);
}

int main()
{
    printHeader();
    WSADATA wsaData;
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != NO_ERROR)
    {
        printf("WSAStartup failed with error: %d\n", iResult);
        return 1;
    }

    SOCKET socket_fd;
    struct sockaddr_in server_sock, client_sock;
    int len = sizeof(client_sock);
    uint8_t recv_req[LITTLE_BUF];
    request_pkt *request;

    uint32_t session_id, i;

    socket_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if (socket_fd == INVALID_SOCKET)
    {
        printf("Socket creation failed with error: %d\n", WSAGetLastError());
        WSACleanup();
        return 0;
    }

    server_sock.sin_family = AF_INET;
    server_sock.sin_addr.s_addr = htonl(INADDR_ANY);
    server_sock.sin_port = htons(PORT);

    if (bind(socket_fd, (struct sockaddr *)&server_sock, sizeof(server_sock)) == SOCKET_ERROR)
    {
        printf("Socket bind failed with error: %d\n", WSAGetLastError());
        closesocket(socket_fd);
        WSACleanup();
        return 0;
    }

    printf("TFTP server: listening on %d\n", ntohs(server_sock.sin_port));

    while (1)
    {
        memset(&client_sock, 0, sizeof(client_sock));
        memset(&recv_req, 0, sizeof(recv_req));

        // printf("tftp server: Waiting for socket request...\n");

        if (recvfrom(socket_fd, (char *)recv_req, sizeof(recv_req), 0, (struct sockaddr *)&client_sock, &len) == SOCKET_ERROR)
        {
            printf("recvfrom failed with error: %d\n", WSAGetLastError());
            continue;
        }

        uint16_t opcode = ntohs(*(uint16_t *)recv_req);

        if (opcode == RRQ)
        {
            HANDLE thread_handle;
            DWORD thread_id;

            if (session_count < MAX_SESSION && !write_flag)
            {
                for (i = 0; i < MAX_SESSION; i++)
                {
                    if (!session_flag[i])
                    {
                        session_id = i;
                        session_flag[i] = 1;
                        break;
                    }
                }
            }
            else
            {
                printf("tftp server: Can't start the read session\n");
                continue;
            }
            memset(&sessions[session_id], 0, sizeof(sessions[session_id]));
            filename_mode_option_fetch(recv_req, session_id);
            memcpy(&sessions[session_id].client_adderess, &client_sock, sizeof(sessions[session_id].client_adderess));

            thread_handle = CreateThread(NULL, 0, RRQ_func, (LPVOID)&session_id, 0, &thread_id);
            if (thread_handle == NULL)
            {
                printf("tftp server: Thread creation failed with error: %d\n", GetLastError());
            }
            else
            {
                CloseHandle(thread_handle);
            }

            session_count++;
        }
        else if (opcode == WRQ)
        {
            HANDLE thread_handle;
            DWORD thread_id;

            if (session_count == 0)
            {
                session_id = 0;
                write_flag = 1;
                session_flag[session_id] = 1;
            }
            else
            {
                printf("tftp server: Can't start the write session\n");
                continue;
            }
            memset(&sessions[session_id], 0, sizeof(sessions[session_id]));
            filename_mode_option_fetch(recv_req, session_id);
            memcpy(&sessions[session_id].client_adderess, &client_sock, sizeof(sessions[session_id].client_adderess));

            thread_handle = CreateThread(NULL, 0, WRQ_func, (LPVOID)&session_id, 0, &thread_id);
            if (thread_handle == NULL)
            {
                printf("tftp server: Thread creation failed with error: %d\n", GetLastError());
            }
            else
            {
                CloseHandle(thread_handle);
            }

            session_count++;
        }
    }

    closesocket(socket_fd);
    WSACleanup();
    return 0;
}

void filename_mode_option_fetch(uint8_t *buff, int session_id)
{
    request_pkt *req_packet = (request_pkt *)buff;

    uint8_t *file_name = req_packet->filename_mode;
    uint8_t *file_mode = file_name + strlen((char *)file_name) + 1;

    sessions[session_id].block_size = BLOCKSIZE;
    sessions[session_id].timeout = RECV_TIMEOUT;

    uint8_t *optN = file_mode + strlen((char *)file_mode) + 1;

    int32_t len = strlen((char *)optN);

    memcpy(sessions[session_id].filename, file_name, strlen((char *)file_name) + 1);

    memcpy(sessions[session_id].transfer_mode, file_mode, strlen((char *)file_mode) + 1);
    
    if (!write_flag)
    {
        FILE *fp;

        // fopen_s(&fp, file_name, "r");

        FOPEN(fp, file_name, "r");

        if (fp == NULL)
        {
            return;
        }

        struct stat st;
        if (stat(file_name, &st) == 0)
        {
            sprintf(sessions[session_id].tsize, "%d", st.st_size);

            // printf("File size: %s\n", sessions[session_id].tsize);
        }
        fclose(fp);
    }

    uint8_t *valueN;
    memset(sessions[session_id].option, 0, sizeof(sessions[session_id].option));
    uint8_t *position = (uint8_t *)sessions[session_id].option;
    sessions[session_id].option_len = 0;

    while (len > 0)
    {
        if (strcmp((char *)optN, "blksize") == 0)
        {
            valueN = optN + strlen((char *)optN) + 1;
            memcpy(position, optN, strlen((char *)optN) + 1);
            position += strlen((char *)optN) + 1;
            memcpy(position, valueN, strlen((char *)valueN) + 1);
            position += strlen((char *)valueN) + 1;
            sessions[session_id].option_flag = 1;
            sessions[session_id].block_size = atoi((char *)valueN);
            sessions[session_id].option_len += strlen((char *)optN) + strlen((char *)valueN) + 2;
        }
        else if (strcmp((char *)optN, "tsize") == 0)
        {
            valueN = optN + strlen((char *)optN) + 1;
            memcpy(position, optN, strlen((char *)optN) + 1);
            position += strlen((char *)optN) + 1;
            sessions[session_id].option_flag = 1;
            if (write_flag)
            {
                memcpy(position, valueN, strlen((char *)valueN) + 1);
                position += strlen((char *)valueN) + 1;
                sessions[session_id].option_len += strlen((char *)optN) + strlen((char *)valueN) + 2;
            }
            else
            {
                memcpy(position, sessions[session_id].tsize, strlen((char *)sessions[session_id].tsize) + 1);
                position += strlen((char *)sessions[session_id].tsize) + 1;
                sessions[session_id].option_len += strlen((char *)optN) + strlen((char *)sessions[session_id].tsize) + 2;
            }
        }
        else if (strcmp((char *)optN, "timeout") == 0)
        {
            valueN = optN + strlen((char *)optN) + 1;
            memcpy(position, optN, strlen((char *)optN) + 1);
            position += strlen((char *)optN) + 1;
            memcpy(position, valueN, strlen((char *)valueN) + 1);
            position += strlen((char *)valueN) + 1;
            sessions[session_id].option_flag = 1;
            sessions[session_id].timeout = atoi((char *)valueN);
            sessions[session_id].option_len += strlen((char *)optN) + strlen((char *)valueN) + 2;
        }
        else
            break;

        optN = valueN + strlen((char *)valueN) + 1;
        len = strlen((char *)optN);
    }

    // printf("tftp server: Received request for file: %s with mode: %s block_size: %d tsize: %s\n", sessions[session_id].filename,
    //        sessions[session_id].transfer_mode, sessions[session_id].block_size, sessions[session_id].tsize);
}

void send_error(SOCKET socket_f, uint16_t error_code, uint8_t *error_string, struct sockaddr_in *client_sock, int slen)
{
    char buff[LITTLE_BUF];

    error_pkt *err = (error_pkt *)buff;

    err->opcode = htons(ERROR_TFTP);
    err->error_code = htons(error_code);
    memcpy(err->error_string, error_string, strlen((char *)error_string) + 1);

    sendto(socket_f, buff, sizeof(*err) + strlen((char *)error_string) + 1, 0, (struct sockaddr *)client_sock, slen);
}

void tftp_send_data(SOCKET socket_f, uint16_t block_number, uint8_t *data, int dlen, struct sockaddr_in *client_sock, int slen)
{
    char buff[BUFSIZE];
    data_pkt *data_packet = (data_pkt *)buff;

    data_packet->opcode = htons(DATA);
    data_packet->block_number = htons(block_number);
    memcpy(data_packet->data, data, dlen);

    sendto(socket_f, (uint8_t *)buff, sizeof(*data_packet) + dlen, 0, (struct sockaddr *)client_sock, slen);
}

void tftp_send_ack(SOCKET socket_f, uint16_t block_number, struct sockaddr_in *client_sock, int slen)
{
    char buff[LITTLE_BUF];
    ack_pkt *ack_packet = (ack_pkt *)buff;

    ack_packet->opcode = htons(ACK);
    ack_packet->block_number = htons(block_number);

    sendto(socket_f, (uint8_t *)buff, sizeof(*ack_packet), 0, (struct sockaddr *)client_sock, slen);
}

DWORD WINAPI RRQ_func(LPVOID arg)
{
    int index = *(int *)arg;
    sessions[index].session_id = index;

    // printf("Start read Session : %d\n", index);

    SOCKET socket_fd_s;
    struct sockaddr_in server_sock_addr, client_addrs, client_address;
    int slen = sizeof(client_addrs);
    int clen = sizeof(sessions[index].client_adderess);
    client_address = sessions[index].client_adderess;
    uint16_t type;
    uint32_t block_read;

    int block_number = 1;

    uint32_t block_size = sessions[index].block_size;
    uint32_t tsize = atoi(sessions[index].tsize);

    uint16_t block, error_code;
    uint8_t error_message[LITTLE_BUF];
    ack_pkt *ack_packet;
    error_pkt *erro;

    uint32_t retries = 0;
    const uint32_t max_retries = RECV_RETRIES;

    socket_fd_s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if (socket_fd_s == INVALID_SOCKET)
    {
        printf("Socket creation Failed!\n");
        session_flag[index] = 0;
        session_count--;
        return 0;
    }

    server_sock_addr.sin_family = AF_INET;
    server_sock_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_sock_addr.sin_port = 0;

    if (bind(socket_fd_s, (struct sockaddr *)&server_sock_addr, sizeof(server_sock_addr)) == SOCKET_ERROR)
    {
        printf("Socket bind Failed!\n");
        closesocket(socket_fd_s);
        session_flag[index] = 0;
        session_count--;
        return 0;
    }

    uint8_t buff[BUFSIZE];
    uint8_t payload[BUFSIZE];

    FILE *fp;

    printf("\nSession %d : Request for read file : %s\n", index, sessions[index].filename);
    // printf("Session %d : Transfer mode:  %s\n", index, sessions[index].transfer_mode);

    if (_access(sessions[index].filename, 0) != -1)
    {
        // printf("Session %d : File %s exists in the current directory.\n", index, sessions[index].filename);
    }
    else
    {
        printf("\nSession %d : File %s does not exist in the current directory.\n", index, sessions[index].filename);

        send_error(socket_fd_s, (uint16_t)ERR_FILE_NOT_FOUND, (uint8_t *)"File not exits", &client_address, clen);

        READ_SESSION_CLOSE(fp, socket_fd_s, index);
    }

    // fopen_s(&fp, sessions[index].filename, "rb");

    FOPEN(fp, sessions[index].filename, "rb");

    if (fp == NULL)
    {
        printf("\nSession %d : file opened failed!\n", index);
        READ_SESSION_CLOSE(fp, socket_fd_s, index);
    }

    if (sessions[index].option_flag)
    {
        memset(&buff, 0, sizeof(buff));

        optn_pkt *optn_packet = (optn_pkt *)buff;

        optn_packet->opcode = htons(6);

        memcpy(optn_packet->option, sessions[index].option, sessions[index].option_len);

        sendto(socket_fd_s, (uint8_t *)buff, 2 + sessions[index].option_len, 0, (struct sockaddr *)&client_address, clen);

        // memset(&buff, 0, sizeof(buff));

        // if (recvfrom(socket_fd_s, (uint8_t *)buff, sizeof(buff), 0, (struct sockaddr *)&client_addrs, &slen) == SOCKET_ERROR)
        // {
        //     printf("\nrecvfrom failed with error: %d\n", WSAGetLastError());
        // }

        // ack_pkt *ack_packet = (ack_pkt *)buff;

        // printf("Received packet type: %d, block : %d\n", ntohs(ack_packet->opcode), ntohs(ack_packet->block_number));
    }

    int total_read = 0;
    int total_byte = 0;
    char *position;
    char *block_space = NULL;

    // Allocate memory for block_space
    block_space = (char *)malloc((block_size + 100) * sizeof(char));
    if (block_space == NULL)
    {
        // Handle memory allocation failure
        printf("Session %d : Failed to allocate memory for block_space\n", index);
        READ_SESSION_CLOSE(fp, socket_fd_s, index);
    }

    int read_size = MAX_READ_SIZE / block_size;

    read_size = read_size * block_size;

    uint32_t retry_flag = 0;

    while (1)
    {
        if (!retry_flag && !sessions[index].option_flag)
        {
            memset(&buff, 0, sizeof(buff));

            if (!total_read)
            {
                memset(&payload, 0, sizeof(payload));
                position = (char *)payload;
                total_read = fread(payload, 1, read_size, fp);
            }

            if (total_read >= block_size)
            {
                block_read = block_size;
                total_read = total_read - block_size;
            }
            else
            {
                block_read = total_read;
                total_read = 0;
            }

            // printf("Session %d : Read data block: %d\n", index, block_read);

            memset(block_space, 0, sizeof(block_space));
            memcpy(block_space, position, block_read);

            data_pkt *data_packet;

            tftp_send_data(socket_fd_s, block_number, (uint8_t *)block_space, block_read, &client_address, clen);
            // sessions[index].last_send_time = time(NULL);

            // printf("Session %d : Send packet type: %d , data block : %d\n", index, DATA, block_number);

            position += block_read;
            total_byte += block_read;

            if (session_count < 2)
                print_progress(total_byte, tsize, index);
        }

        // time_t current_time = time(NULL);
        // double elapsed_seconds = difftime(current_time, sessions[index].last_send_time);

        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(socket_fd_s, &read_fds);

        struct timeval timeout;
        timeout.tv_sec = RECV_TIMEOUT;
        timeout.tv_usec = 0;

        int select_result = select(0, &read_fds, NULL, NULL, &timeout);

        if (select_result == SOCKET_ERROR)
        {
            printf("Session %d : Select failed with error: %d\n", WSAGetLastError(), index);
            READ_SESSION_CLOSE(fp, socket_fd_s, index);
        }
        else if (select_result == 0)
        {
            retry_flag = 1;
            // Timeout occurred, retransmit the packet
            if (retries < max_retries)
            {
                if (sessions[index].option_flag)
                    sendto(socket_fd_s, (uint8_t *)buff, 2 + sessions[index].option_len, 0, (struct sockaddr *)&client_address, clen);
                else
                    tftp_send_data(socket_fd_s, block_number, (uint8_t *)block_space, block_read, &client_address, clen);
                // sessions[index].last_send_time = current_time; // Update the send time
                retries++;
                // printf("\nSession %d : Retransmiting data block : %d\n", index, block_number);
            }
            else
            {
                // Maximum retries reached, terminate the session
                send_error(socket_fd_s, 0, (uint8_t *)"Maximum retries reached", &client_address, clen);
                printf("\nSession %d : TFTP timeout!\n", index);
                READ_SESSION_CLOSE(fp, socket_fd_s, index);
            }
        }
        else
        {
            memset(&buff, 0, sizeof(buff));

            if (recvfrom(socket_fd_s, (uint8_t *)buff, sizeof(buff), 0, (struct sockaddr *)&client_addrs, &slen) == SOCKET_ERROR)
            {
                int error_code = WSAGetLastError();
                if (error_code == 10054)
                {
                    // Remote host forcibly closed the connection
                    printf("Session %d: Remote host forcibly closed the connection\n", index);
                    READ_SESSION_CLOSE(fp, socket_fd_s, index);
                }
                else
                {
                    printf("Session %d: recvfrom failed with error: %d\n", error_code);
                    READ_SESSION_CLOSE(fp, socket_fd_s, index);
                }
            }

            type = ntohs(*(uint16_t *)buff);

            switch (type)
            {
            case ACK:
                ack_packet = (ack_pkt *)buff;

                block = ntohs(ack_packet->block_number);

                if (block == 0)
                {
                    sessions[index].option_flag = 0;
                    continue;
                }
                else if (block == block_number)
                {
                    retry_flag = 0;
                    // printf("Session %d : ACK packet data block : %d\n\n", index, block);
                    block_number++;
                }
                else if (block == block_number - 1)
                {
                    retry_flag = 1;
                    tftp_send_data(socket_fd_s, block_number, (uint8_t *)block_space, block_read, &client_address, clen);

                    // printf("\nblock == block - 1 Session %d : Retransmiting data block : %d\n", index, DATA, block_number);

                    continue;
                }
                else
                {
                    send_error(socket_fd_s, 0, (uint8_t *)"Wrong block acknoledgement", &client_address, clen);

                    READ_SESSION_CLOSE(fp, socket_fd_s, index);
                }

                if (block_read < block_size)
                {
                    printf("\nSession %d : %s transfer complete %d blocks: %d bytes\n", index, sessions[index].filename, block_number - 1, total_byte);

                    READ_SESSION_CLOSE(fp, socket_fd_s, index);
                }
                break;

            case ERROR_TFTP:
                erro = (error_pkt *)buff;
                error_code = ntohs(erro->error_code);
                memcpy(error_message, erro->error_string, strlen((char *)erro->error_string) + 1);

                printf("\nSession %d : Error Code : %d and Error Message: %s\n", index, error_code, error_message);

                READ_SESSION_CLOSE(fp, socket_fd_s, index);
                break;

            default:
                break;
            }
        }
    }

    READ_SESSION_CLOSE(fp, socket_fd_s, index);
}

DWORD WINAPI WRQ_func(LPVOID arg)
{
    int index = *(int *)arg;
    sessions[index].session_id = index;

    printf("\nSession %d : Request for write file : %s\n", index, sessions[index].filename);

    SOCKET socket_fd_s;
    struct sockaddr_in server_sock_addr, client_addrs, client_address;
    int slen = sizeof(client_addrs);
    int clen = sizeof(sessions[index].client_adderess);
    client_address = sessions[index].client_adderess;
    size_t c;

    int block_number = 0;
    int total_bytes = 0;

    uint32_t block_size = sessions[index].block_size;

    uint16_t block, error_code, opcode, type;
    uint8_t error_message[BUFSIZE];
    ack_pkt *ack_packet;
    FILE *fp;
    data_pkt *data_packet;
    int block_write;

    error_pkt *erro;
    uint32_t retries = 0;
    const uint32_t max_retries = RECV_RETRIES;

    socket_fd_s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if (socket_fd_s == INVALID_SOCKET)
    {
        printf("\nSocket creation Failed!\n");
        session_flag[index] = 0;
        session_count--;
        return 0;
    }

    server_sock_addr.sin_family = AF_INET;
    server_sock_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_sock_addr.sin_port = 0;

    if (bind(socket_fd_s, (struct sockaddr *)&server_sock_addr, sizeof(server_sock_addr)) == SOCKET_ERROR)
    {
        printf("\nSocket bind Failed!\n");
        closesocket(socket_fd_s);
        session_flag[index] = 0;
        session_count--;
        return 1;
    }

    // printf("Session %d : Request for write file : %s\n", index, sessions[index].filename);
    // printf("Session %d : Transfer mode: %s\n", index, sessions[index].transfer_mode);

    if (_access(sessions[index].filename, 0) != -1)
    {
        printf("\nSession %d : File %s is already exists in the current directory.\n", index, sessions[index].filename);

        send_error(socket_fd_s, (uint16_t)ERR_FILE_ALREADY_EXITS, (uint8_t *)"File already exits", &client_address, clen);

        WRITE_SESSION_CLOSE(fp, socket_fd_s, index);
    }

    uint8_t buff[BUFSIZE];
    uint8_t payload[BUFSIZE];

    if (sessions[index].option_flag)
    {
        memset(&buff, 0, sizeof(buff));

        optn_pkt *optn_packet = (optn_pkt *)buff;

        optn_packet->opcode = htons(6);

        memcpy(optn_packet->option, sessions[index].option, sessions[index].option_len);

        sendto(socket_fd_s, (uint8_t *)buff, 2 + sessions[index].option_len, 0, (struct sockaddr *)&client_address, clen);
        block_number++;
    }
    else
    {
        memset(&buff, 0, sizeof(buff));

        ack_packet = (ack_pkt *)buff;

        ack_packet->opcode = htons(ACK);

        ack_packet->block_number = htons(block_number);

        sendto(socket_fd_s, (uint8_t *)buff, sizeof(*ack_packet), 0, (struct sockaddr *)&client_address, clen);

        // printf("Sent ACK for write file\n");
        block_number++;
    }

    // fopen_s(&fp, sessions[index].filename, "wb");

    FOPEN(fp, sessions[index].filename, "wb");

    if (fp == NULL)
    {
        printf("\nfile opened failed!\n");
        WRITE_SESSION_CLOSE(fp, socket_fd_s, index);
    }

    while (1)
    {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(socket_fd_s, &read_fds);

        struct timeval timeout;
        timeout.tv_sec = RECV_TIMEOUT; // sessions[index].timeout;
        timeout.tv_usec = 0;

        int select_result = select(0, &read_fds, NULL, NULL, &timeout);

        if (select_result == SOCKET_ERROR)
        {
            printf("Select failed with error: %d\n", WSAGetLastError());
            READ_SESSION_CLOSE(fp, socket_fd_s, index);
        }
        else if (select_result == 0)
        {
            // Timeout occurred, retransmit the packet
            if (retries < max_retries)
            {
                if (sessions[index].option_flag)
                    sendto(socket_fd_s, (uint8_t *)buff, 2 + sessions[index].option_len, 0, (struct sockaddr *)&client_address, clen);
                else
                    sendto(socket_fd_s, (uint8_t *)buff, sizeof(*ack_packet), 0, (struct sockaddr *)&client_address, clen);
                // sessions[index].last_send_time = current_time; // Update the send time
                retries++;
                // printf("\nSession %d : Retransmiting data block : %d\n", index, ntohs(ack_packet->block_number));
            }
            else
            {
                // Maximum retries reached, terminate the session
                send_error(socket_fd_s, 0, (uint8_t *)"Maximum retries reached", &client_address, clen);
                printf("\nSession %d : TFTP timeout!\n", index);
                READ_SESSION_CLOSE(fp, socket_fd_s, index);
            }
        }
        else
        {

            memset(&payload, 0, sizeof(payload));
            memset(&buff, 0, sizeof(buff));

            c = recvfrom(socket_fd_s, (uint8_t *)buff, sizeof(buff), 0, (struct sockaddr *)&client_addrs, &slen);

            type = ntohs(*(uint16_t *)buff);

            switch (type)
            {
            case DATA:
                data_packet = (data_pkt *)buff;

                block_write = c - 4;
                total_bytes += block_write;

                opcode = ntohs(data_packet->opcode);
                block = ntohs(data_packet->block_number);
                memcpy(payload, data_packet->data, block_write);

                print_progress_write(block, total_bytes, index);

                // printf("Receive packet type: %d, data block: %d\n", opcode, block);

                fwrite(payload, 1, block_write, fp);

                // printf("Write block: %d\n", block_write);

                memset(&buff, 0, sizeof(buff));

                ack_packet = (ack_pkt *)buff;

                ack_packet->opcode = htons(ACK);

                if (block == block_number)
                {
                    ack_packet->block_number = htons(block_number);
                }
                else if (block == block_number - 1)
                {
                    ack_packet->block_number = htons(block_number - 1);
                    sendto(socket_fd_s, (uint8_t *)buff, sizeof(*ack_packet), 0, (struct sockaddr *)&client_address, clen);
                    continue;
                }
                else
                {
                    send_error(socket_fd_s, 0, (uint8_t *)"Wrong Data block send", &client_address, clen);
                    WRITE_SESSION_CLOSE(fp, socket_fd_s, index);
                }

                sendto(socket_fd_s, (uint8_t *)buff, sizeof(*ack_packet), 0, (struct sockaddr *)&client_address, clen);

                // printf("Sent ACK for data block: %d\n\n", block_number);

                if (block_write < block_size)
                {
                    printf("\nSession %d : %s receiving complete %d blocks : %d bytes\n", index, sessions[index].filename, block_number, total_bytes);
                    WRITE_SESSION_CLOSE(fp, socket_fd_s, index);
                }
                block_number++;
                break;

            case ERROR_TFTP:
                erro = (error_pkt *)buff;
                error_code = ntohs(erro->error_code);
                memcpy(error_message, erro->error_string, strlen((char *)erro->error_string) + 1);

                printf("Session %d : Error Code : %d and Error Message: %s\n", index, error_code, error_message);

                WRITE_SESSION_CLOSE(fp, socket_fd_s, index);
                break;

            default:
                break;
            }
        }
    }

    return 0;
}
