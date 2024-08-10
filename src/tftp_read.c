/*
 * =====================================================================================
 *
 *       Filename:  tftp_read.c
 *
 *    Description:  This file contains the function implementations for handling read 
 *                  requests (RRQ) in the TFTP server, including managing client sessions,
 *                  sending data from a file, and handling acknowledgments and errors.
 *
 *        Version:  1.0.1
 *        Created:  Sat Aug 10 10:17:38 2024
 *       Revision:  1.0.1
 *       Compiler:  gcc
 *
 *         Author:  Yamin Haque, R&D Engineer, yamin.haque@bdcom.com.cn
 *        Company:  Shanghai BDCOM Information Technology Co., Ltd.
 *        
 *        This file is part of the TFTP server distribution (https://github.com/Yamin1999).
 *        Copyright (c) 2024 Yamin Haque.
 *        This program is free software: you can redistribute it and/or modify
 *        it under the terms of the GNU General Public License as published by  
 *        the Free Software Foundation, version 3.
 *
 *        This program is distributed in the hope that it will be useful, but 
 *        WITHOUT ANY WARRANTY; without even the implied warranty of 
 *        MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
 *        General Public License for more details.
 *
 *        You should have received a copy of the GNU General Public License 
 *        along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * =====================================================================================
 */

#include <tftp_types.h>
#include <tftp_read.h>
#include <tftp_utils.h>

DWORD WINAPI RRQ_func(LPVOID arg)
{
    int index = *(int *)arg;
    sessions[index].session_id = index;

    SOCKET socket_fd_s;
    struct sockaddr_in server_sock_addr, client_addrs, client_address;
    int slen = sizeof(client_addrs);
    int clen = sizeof(sessions[index].client_adderess);
    client_address = sessions[index].client_adderess;
    uint16_t type;
    uint32_t block_read;

    int block_number = 1;

    uint32_t block_size = sessions[index].block_size;
    uint32_t tsize = atoi((const char *)sessions[index].tsize);

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

    FILE *fp = NULL;

    printf("\nSession %d : Request for read file : %s from %s\n", index, sessions[index].filename, inet_ntoa(client_address.sin_addr));

    if (_access((const char *)sessions[index].filename, 0) == 0)
    {
        printf("Session %d : File %s does not exist in the current directory.\n", index, sessions[index].filename);
        send_error(socket_fd_s, (uint16_t)ERR_FILE_NOT_FOUND, (uint8_t *)"File not exits", &client_address, clen);
        READ_SESSION_CLOSE(fp, socket_fd_s, index);
    }

    if (strcmp((char *)sessions[index].transfer_mode, (char *)"octet") != 0)
    {
        printf("Session %d : Olny support octet mode!\n", index);
        send_error(socket_fd_s, (uint16_t)ERR_NOT_DEFINE, (uint8_t *)"Olny support octet mode!", &client_address, clen);
        READ_SESSION_CLOSE(fp, socket_fd_s, index);
    }

    FOPEN(fp, sessions[index].filename, "rb");

    if (fp == NULL)
    {
        printf("Session %d : file opened failed!\n", index);
        READ_SESSION_CLOSE(fp, socket_fd_s, index);
    }

    if (sessions[index].option_flag)
    {
        memset(&buff, 0, sizeof(buff));
        optn_pkt *optn_packet = (optn_pkt *)buff;
        optn_packet->opcode = htons(6);
        memcpy(optn_packet->option, sessions[index].option, sessions[index].option_len);
        sendto(socket_fd_s, (const char *)buff, 2 + sessions[index].option_len, 0, (struct sockaddr *)&client_address, clen);
    }

    uint32_t total_read = 0;
    uint32_t total_byte = 0;
    char *position;
    char *block_space = NULL;

    block_space = (char *)malloc((block_size + 100) * sizeof(char));
    if (block_space == NULL)
    {
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

            memset(block_space, 0, (block_size + 100) * sizeof(char));
            memcpy(block_space, position, block_read);

            tftp_send_data(socket_fd_s, block_number, (uint8_t *)block_space, block_read, &client_address, clen);

            position += block_read;
            total_byte += block_read;

            print_progress(total_byte, tsize, index);
        }

        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(socket_fd_s, &read_fds);

        struct timeval timeout;
        timeout.tv_sec = RECV_TIMEOUT;
        timeout.tv_usec = 0;

        int select_result = select(0, &read_fds, NULL, NULL, &timeout);

        if (select_result == SOCKET_ERROR)
        {
            printf("\nSession %d : Select failed with error: %d\n", WSAGetLastError(), index);
            READ_SESSION_CLOSE(fp, socket_fd_s, index);
        }
        else if (select_result == 0)
        {
            retry_flag = 1;
            if (retries < max_retries)
            {
                if (sessions[index].option_flag)
                    sendto(socket_fd_s, (const char *)buff, 2 + sessions[index].option_len, 0, (struct sockaddr *)&client_address, clen);
                else
                    tftp_send_data(socket_fd_s, block_number, (uint8_t *)block_space, block_read, &client_address, clen);
                retries++;
            }
            else
            {
                send_error(socket_fd_s, 0, (uint8_t *)"Maximum retries reached", &client_address, clen);
                printf("\nSession %d : TFTP timeout!\n", index);
                READ_SESSION_CLOSE(fp, socket_fd_s, index);
            }
        }
        else
        {
            memset(&buff, 0, sizeof(buff));

            if (recvfrom(socket_fd_s, (char *)buff, sizeof(buff), 0, (struct sockaddr *)&client_addrs, &slen) == SOCKET_ERROR)
            {
                int error_code = WSAGetLastError();
                if (error_code == 10054)
                {
                    printf("\nSession %d : Remote host forcibly closed the connection\n", index);
                    READ_SESSION_CLOSE(fp, socket_fd_s, index);
                }
                else
                {
                    printf("\nSession %d : recvfrom failed with error: %d\n", index, error_code);
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
                    block_number++;
                }
                else if (block == block_number - 1)
                {
                    retry_flag = 1;
                    tftp_send_data(socket_fd_s, block_number, (uint8_t *)block_space, block_read, &client_address, clen);
                    continue;
                }
                else
                {
                    send_error(socket_fd_s, 0, (uint8_t *)"Wrong block acknowledgement", &client_address, clen);
                    READ_SESSION_CLOSE(fp, socket_fd_s, index);
                }

                if (block_read < block_size)
                {
                    time_t t;
                    time(&t);
                    printf("\nSession %d : Transfer complete %d bytes from Panda at: %s\n", index, total_byte, ctime(&t));
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