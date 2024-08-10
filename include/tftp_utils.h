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


#endif // TFTP_UTILS_H