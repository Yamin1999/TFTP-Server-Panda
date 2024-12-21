/*
 * =====================================================================================
 *
 *       Filename:  tftp_server.c
 *
 *    Description:  This files handles the setup of the 
 *                  TFTP server, manages client sessions for read and write requests, 
 *                  and processes options related to TFTP operations.
 *
 *        Version:  1.0.0
 *        Created:  Fri Aug 10 11:05:52 2024
 *       Revision:  1.0.0
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
#include <tftp_write.h>
#include <tftp_utils.h>
#include <tftp_server.h>
#include <conio.h> // For _getch()

void show_error_and_wait(const char* error_message, int error_code) {
    printf("\nError: %s\nError code: %d\n", error_message, error_code);
    printf("\nPress any key to continue...");
    _getch();
}

void start_tftp_server() {
    SOCKET socket_fd;
    struct sockaddr_in server_sock, client_sock;
    int len = sizeof(client_sock);
    uint8_t recv_req[LITTLE_BUF];
    uint32_t session_id, i;

    socket_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if (socket_fd == INVALID_SOCKET)
    {
        show_error_and_wait("Socket creation failed", WSAGetLastError());
        WSACleanup();
        return;
    }

    server_sock.sin_family = AF_INET;
    server_sock.sin_addr.s_addr = htonl(INADDR_ANY);
    server_sock.sin_port = htons(PORT);

    if (bind(socket_fd, (struct sockaddr *)&server_sock, sizeof(server_sock)) == SOCKET_ERROR)
    {
        show_error_and_wait("Socket bind failed", WSAGetLastError());
        closesocket(socket_fd);
        WSACleanup();
        return;
    }

    //printf("TFTP server : Panda listening on port %d\n", ntohs(server_sock.sin_port));

    init_session_table();

    while (1)
    {
        memset(&client_sock, 0, sizeof(client_sock));
        memset(&recv_req, 0, sizeof(recv_req));

        if (recvfrom(socket_fd, (char *)recv_req, sizeof(recv_req), 0, (struct sockaddr *)&client_sock, &len) == SOCKET_ERROR)
        {
            show_error_and_wait("recvfrom failed", WSAGetLastError());
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
                show_error_and_wait("Can't start the read session", 0);
                continue;
            }
            memset(&sessions[session_id], 0, sizeof(sessions[session_id]));
            filename_mode_option_fetch(recv_req, session_id);
            memcpy(&sessions[session_id].client_adderess, &client_sock, sizeof(sessions[session_id].client_adderess));

            thread_handle = CreateThread(NULL, 0, RRQ_func, (LPVOID)&session_id, 0, &thread_id);
            if (thread_handle == NULL)
            {
                show_error_and_wait("Thread creation failed", GetLastError());
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
                show_error_and_wait("Can't start the write session", 0);
                continue;
            }
            memset(&sessions[session_id], 0, sizeof(sessions[session_id]));
            filename_mode_option_fetch(recv_req, session_id);
            memcpy(&sessions[session_id].client_adderess, &client_sock, sizeof(sessions[session_id].client_adderess));

            thread_handle = CreateThread(NULL, 0, WRQ_func, (LPVOID)&session_id, 0, &thread_id);
            if (thread_handle == NULL)
            {
                show_error_and_wait("Thread creation failed", GetLastError());
            }
            else
            {
                CloseHandle(thread_handle);
            }

            session_count++;
        }
    }

    closesocket(socket_fd);
}

void filename_mode_option_fetch(uint8_t *buff, int session_id) {
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
        FOPEN(fp, file_name, "r");

        if (fp == NULL)
        {
            return;
        }

        struct stat st;
        if (stat((const char*) file_name, &st) == 0)
        {
            sprintf((char *) sessions[session_id].tsize, "%ld", st.st_size);
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
}