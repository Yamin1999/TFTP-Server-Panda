/*
 * =====================================================================================
 *
 *       Filename:  tftp_write.c
 *
 *    Description:  This file contains the function implementations for handling write 
 *                  requests (WRQ) in the TFTP server, including managing client sessions,
 *                  receiving data, and saving the received data to a file.
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
#include <tftp_write.h>
#include <tftp_utils.h>

DWORD WINAPI WRQ_func(LPVOID arg)
{
    int index = *(int *)arg;
    sessions[index].session_id = index;
    sessions[index].operation = WRITE;
    sessions[index].index_count = ++session_index_count;

    SOCKET socket_fd_s;
    struct sockaddr_in server_sock_addr, client_addrs, client_address;
    int slen = sizeof(client_addrs);
    int clen = sizeof(sessions[index].client_adderess);
    client_address = sessions[index].client_adderess;
    int c;

    int block_number = 0;
    int total_bytes = 0;

    uint32_t block_size = sessions[index].block_size;

    uint16_t block, error_code, type;
    uint8_t error_message[LITTLE_BUF];
    ack_pkt *ack_packet;
    FILE *fp = NULL;
    data_pkt *data_packet;
    uint32_t block_write;

    error_pkt *erro;
    uint32_t retries = 0;
    const uint32_t max_retries = RECV_RETRIES;

     start_session_timer(&sessions[index]);

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
        return 1;
    }

    //printf("\nSession %d : Request for write file : %s from %s\n", index, sessions[index].filename, inet_ntoa(client_address.sin_addr));

    if (_access((const char *)sessions[index].filename, 0) != -1)
    {
        printf("| %-3d|    File %s is already exists in the current directory.\n", sessions[index].index_count, sessions[index].filename);
        send_error(socket_fd_s, (uint16_t)ERR_FILE_ALREADY_EXITS, (uint8_t *)"File already exits", &client_address, clen);
        println();
        WRITE_SESSION_CLOSE(fp, socket_fd_s, index);
    }

    if (strcmp((char *)sessions[index].transfer_mode, (char *)"octet") != 0)
    {
        printf("| %-3d|    Olny support octet mode!\n", sessions[index].index_count);
        send_error(socket_fd_s, (uint16_t)ERR_NOT_DEFINE, (uint8_t *)"Olny support octet mode!", &client_address, clen);
        println();
        WRITE_SESSION_CLOSE(fp, socket_fd_s, index);
    }

    uint8_t buff[BUFSIZE];
    uint8_t write_buffer[BUFSIZE];
    uint8_t lillte_buf[LITTLE_BUF];
    uint32_t write_buffer_pos = 0;

    if (sessions[index].option_flag)
    {
        memset(&lillte_buf, 0, sizeof(lillte_buf));
        optn_pkt *optn_packet = (optn_pkt *)lillte_buf;
        optn_packet->opcode = htons(6);
        memcpy(optn_packet->option, sessions[index].option, sessions[index].option_len);
        sendto(socket_fd_s, (const char *)lillte_buf, 2 + sessions[index].option_len, 0, (struct sockaddr *)&client_address, clen);
        block_number++;
    }
    else
    {
        memset(&lillte_buf, 0, sizeof(lillte_buf));
        ack_packet = (ack_pkt *)lillte_buf;
        ack_packet->opcode = htons(ACK);
        ack_packet->block_number = htons(block_number);
        sendto(socket_fd_s, (const char *)lillte_buf, sizeof(*ack_packet), 0, (struct sockaddr *)&client_address, clen);
        block_number++;
    }

    FOPEN(fp, sessions[index].filename, "wb");

    if (fp == NULL)
    {
        printf("| %-3d|    file opened failed!\n",sessions[index].index_count);
        println();
        WRITE_SESSION_CLOSE(fp, socket_fd_s, index);
    }

    while (1)
    {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(socket_fd_s, &read_fds);

        struct timeval timeout;
        timeout.tv_sec = RECV_TIMEOUT;
        timeout.tv_usec = 0;

        int select_result = select(0, &read_fds, NULL, NULL, &timeout);

        if (select_result == SOCKET_ERROR)
        {
            printf("Select failed with error: %d\n", WSAGetLastError());
            WRITE_SESSION_CLOSE(fp, socket_fd_s, index);
        }
        else if (select_result == 0)
        {
            if (retries < max_retries)
            {
                if (sessions[index].option_flag)
                    sendto(socket_fd_s, (const char *)lillte_buf, 2 + sessions[index].option_len, 0, (struct sockaddr *)&client_address, clen);
                else
                    sendto(socket_fd_s, (const char *)lillte_buf, sizeof(*ack_packet), 0, (struct sockaddr *)&client_address, clen);
                retries++;
            }
            else
            {
                send_error(socket_fd_s, 0, (uint8_t *)"Maximum retries reached", &client_address, clen);
                printf("\n| %-3d|    TFTP timeout!\n", sessions[index].index_count);
                println();
                WRITE_SESSION_CLOSE(fp, socket_fd_s, index);
            }
        }
        else
        {
            memset(&buff, 0, sizeof(buff));
            c = recvfrom(socket_fd_s, (char *)buff, sizeof(buff), 0, (struct sockaddr *)&client_addrs, &slen);

            if (c == SOCKET_ERROR)
            {
                int error_code = WSAGetLastError();
                if (error_code == 10054)
                {
                    printf("\n| %-3d|    Remote host forcibly closed the connection\n", sessions[index].index_count);
                    println();
                    WRITE_SESSION_CLOSE(fp, socket_fd_s, index);
                }
                else
                {
                    printf("\nSession %d : recvfrom failed with error: %d\n", index, error_code);
                    WRITE_SESSION_CLOSE(fp, socket_fd_s, index);
                }
            }

            type = ntohs(*(uint16_t *)buff);

            switch (type)
            {
            case DATA:
                data_packet = (data_pkt *)buff;
                block_write = c - 4;
                total_bytes += block_write;
                block = ntohs(data_packet->block_number);

                memset(&lillte_buf, 0, sizeof(lillte_buf));
                ack_packet = (ack_pkt *)lillte_buf;
                ack_packet->opcode = htons(ACK);

                // After sending/receiving block 65535
                if (block_number > 65535)
                {
                    block_number = 0; 
                }

                if (block == block_number)
                {
                    if (write_buffer_pos + block_write <= MAX_READ_SIZE)
                    {
                        memcpy(write_buffer + write_buffer_pos, data_packet->data, block_write);
                        write_buffer_pos += block_write;
                    }
                    else
                    {
                        fwrite(write_buffer, 1, write_buffer_pos, fp);
                        write_buffer_pos = 0;
                        memcpy(write_buffer, data_packet->data, block_write);
                        write_buffer_pos = block_write;
                    }

                    //print_progress_write(block, total_bytes, index);
                    log_session_progress(sessions[index].index_count, &sessions[index], total_bytes);
                    ack_packet->block_number = htons(block_number);
                    sendto(socket_fd_s, (const char *)lillte_buf, sizeof(*ack_packet), 0, (struct sockaddr *)&client_address, clen);

                    if (block_write < block_size)
                    {
                        if (write_buffer_pos > 0)
                        {
                            fwrite(write_buffer, 1, write_buffer_pos, fp);
                        };
                        end_session_timer(&sessions[index]);
                        log_session_complete(sessions[index].index_count, &sessions[index], total_bytes);
                        WRITE_SESSION_CLOSE(fp, socket_fd_s, index);
                    }
                    block_number++;
                }
                else if (block == block_number - 1)
                {
                    ack_packet->block_number = htons(block_number - 1);
                    sendto(socket_fd_s, (const char *)lillte_buf, sizeof(*ack_packet), 0, (struct sockaddr *)&client_address, clen);
                    continue;
                }
                else
                {
                    send_error(socket_fd_s, 0, (uint8_t *)"Wrong Data block send", &client_address, clen);
                    WRITE_SESSION_CLOSE(fp, socket_fd_s, index);
                }
                break;

            case ERROR_TFTP:
                erro = (error_pkt *)buff;
                error_code = ntohs(erro->error_code);
                memcpy(error_message, erro->error_string, strlen((char *)erro->error_string) + 1);
                printf("| %-3d|    Error Code : %d and Error Message: %s\n", sessions[index].index_count, error_code, error_message);
                println();
                WRITE_SESSION_CLOSE(fp, socket_fd_s, index);
                break;

            default:
                break;
            }
        }
    }

    return 0;
}