#ifndef TFTP_UTILS_H
#define TFTP_UTILS_H

void printPANDA();
void printHeader();
void setColor(int colorCode);
void print_progress(size_t count, size_t max, int index);
void print_progress_write(size_t count, size_t max, int index);
void send_error(SOCKET socket_f, uint16_t error_code, uint8_t *error_string, struct sockaddr_in *client_sock, int slen);
void tftp_send_data(SOCKET socket_f, uint16_t block_number, uint8_t *data, int dlen, struct sockaddr_in *client_sock, int slen);
void tftp_send_ack(SOCKET socket_f, uint16_t block_number, struct sockaddr_in *client_sock, int slen);
void init_session_table();
void log_session_start(uint32_t index_count, session_t *session);
void log_session_progress(uint32_t index_count, session_t *session, uint32_t bytes);
void log_session_complete(uint32_t index_count, session_t *session, uint32_t total_bytes);


#endif // TFTP_UTILS_H