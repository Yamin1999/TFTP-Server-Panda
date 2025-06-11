/*
 * =====================================================================================
 *
 *       Filename:  tftp_server.c
 *
 *    Description:  This file contains the main functions for starting and handling the 
 *                  TFTP server, including processing read and write requests and managing
 *                  client sessions.
 *
 *        Version:  1.0.2
 *        Created:  Sat Aug 10 10:17:38 2024
 *       Revision:  1.0.2 - Added transfer duration tracking
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
    printf("                                        ____    _    _   _   ____    _    \n");
    printf("                                       |  _ \\  / \\  | \\ | | |  _ \\  / \\   \n");
    printf("                                       | |_) |/ _ \\ |  \\| | | | | |/ _ \\  \n");
    printf("                                       |  __// ___ \\| |\\  | | |_| / ___ \\ \n");
    printf("                                       |_|  /_/   \\_\\_| \\_| |____/_/   \\_\\\n");
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

void init_session_table() {
    printf("\n=============================================================================================================");
    printf("\n| No |    Filename                                     |   Client IP       | Op     | Bytes      | Duration |");
    printf("\n=============================================================================================================");
    printf("\n");
}

void format_time(char *buffer, size_t size) {
    time_t current_time;
    struct tm *time_info;
    
    time(&current_time);
    time_info = localtime(&current_time);
    strftime(buffer, size, "%I:%M:%S %p", time_info);
}

void format_duration(char *buffer, size_t buffer_size, double duration_seconds) {
    if (duration_seconds < 1.0) {
        snprintf(buffer, buffer_size, "%.0f ms", duration_seconds * 1000);
    } else if (duration_seconds < 60.0) {
        snprintf(buffer, buffer_size, "%.2f s", duration_seconds);
    } else {
        int minutes = (int)(duration_seconds / 60);
        double seconds = duration_seconds - (minutes * 60);
        snprintf(buffer, buffer_size, "%dm %.1fs", minutes, seconds);
    }
}

double calculate_transfer_duration(session_t *session) {
    return difftime(session->end_time, session->start_time);
}

void log_session_progress(uint32_t index_count, session_t *session, uint32_t bytes) {
    // Use \r to return to beginning of line and overwrite
    printf("\r| %-3d|    %-44s |   %-15s | %-6s | %-10d | %-8s |",
           index_count,
           session->filename,
           inet_ntoa(session->client_adderess.sin_addr),
           session->operation == READ ? "READ" : "WRITE",
           bytes,
           "---");
    fflush(stdout);
    // Don't print newline - keep updating the same line
}

void log_session_complete(uint32_t index_count, session_t *session, uint32_t total_bytes) {
    char duration_str[20];
    
    // Calculate and format duration
    double duration = calculate_transfer_duration(session);
    format_duration(duration_str, sizeof(duration_str), duration);

    // Final update with duration, then add newline to move to next line
    printf("\r| %-3d|    %-44s |   %-15s | %-6s | %-10d | %-8s |\n",
           index_count,
           session->filename,
           inet_ntoa(session->client_adderess.sin_addr),
           session->operation == READ ? "READ" : "WRITE",
           total_bytes,
           duration_str);
    
    // Add separator line after each completed transfer
    printf("-------------------------------------------------------------------------------------------------------------\n");
    fflush(stdout);
}

// Helper function to start timing a session
void start_session_timer(session_t *session) {
    time(&session->start_time);
}

// Helper function to end timing a session
void end_session_timer(session_t *session) {
    time(&session->end_time);
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