#ifndef TFTP_SERVER_H
#define TFTP_SERVER_H
void start_tftp_server();
void filename_mode_option_fetch(uint8_t *buff, int session_id);

#endif // TFTP_SERVER_H