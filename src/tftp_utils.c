/*
 * =====================================================================================
 *
 *       Filename:  tftp_server.c
 *
 *    Description:  This file contains the main functions for starting and handling the 
 *                  TFTP server, including processing read and write requests and managing
 *                  client sessions.
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
#include <tftp_utils.h>

void printPANDA()
{
    printf("            ____    _    _   _   ____    _    \n");
    printf("           |  _ \\  / \\  | \\ | | |  _ \\  / \\   \n");
    printf("           | |_) |/ _ \\ |  \\| | | | | |/ _ \\  \n");
    printf("           |  __// ___ \\| |\\  | | |_| / ___ \\ \n");
    printf("           |_|  /_/   \\_\\_| \\_| |____/_/   \\_\\\n");
}

void printHeader()
{
    printf("\n+------------------------------------------------------+\n");
    printf("|                     Panda v1.0.1                     |\n");
    printf("+------------------------------------------------------+\n");
    printf("|                                                      |\n");
    printf("|   Md. Yamin Haque                                    |\n");
    printf("|   R&D Engineer                                       |\n");
    printf("|   Shanghai BDCOM Information Technology Co., Ltd.    |\n");
    printf("|   Bangladesh Office                                  |\n");
    printf("|   yamin.haque@bdcom.com.cn                           |\n");
    printf("|                                                      |\n");
    printf("+------------------------------------------------------+\n");
}

void setColor(int colorCode)
{
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hConsole, colorCode);
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
    printf("\rSession %d : Received %lu blocks: %lu bytes", index, (unsigned long)count, (unsigned long)max);
    fflush(stdout);
}

void send_error(SOCKET socket_f, uint16_t error_code, uint8_t *error_string, struct sockaddr_in *client_sock, int slen)
{
    char buff[LITTLE_BUF];
    error_pkt *err = (error_pkt *)buff;

    err->opcode = htons(ERROR_TFTP);
    err->error_code = htons(error_code);
    memcpy(err->error_string, error_string, strlen((char *)error_string) + 1);

    sendto(socket_f, (const char *)buff, sizeof(*err) + strlen((char *)error_string) + 1, 0, (struct sockaddr *)client_sock, slen);
}

void tftp_send_data(SOCKET socket_f, uint16_t block_number, uint8_t *data, int dlen, struct sockaddr_in *client_sock, int slen)
{
    char buff[BUFSIZE];
    data_pkt *data_packet = (data_pkt *)buff;

    data_packet->opcode = htons(DATA);
    data_packet->block_number = htons(block_number);
    memcpy(data_packet->data, data, dlen);

    sendto(socket_f, (const char *)buff, sizeof(*data_packet) + dlen, 0, (struct sockaddr *)client_sock, slen);
}

void tftp_send_ack(SOCKET socket_f, uint16_t block_number, struct sockaddr_in *client_sock, int slen)
{
    char buff[LITTLE_BUF];
    ack_pkt *ack_packet = (ack_pkt *)buff;

    ack_packet->opcode = htons(ACK);
    ack_packet->block_number = htons(block_number);

    sendto(socket_f, (const char *)buff, sizeof(*ack_packet), 0, (struct sockaddr *)client_sock, slen);
}