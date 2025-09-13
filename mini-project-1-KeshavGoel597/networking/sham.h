//<-------LLM GENERATED CODE STARTS HERE-------->
#ifndef SHAM_H
#define SHAM_H

#include <stdint.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <stdarg.h>

// S.H.A.M. Header Structure
struct sham_header {
    uint32_t seq_num;      // Sequence Number
    uint32_t ack_num;      // Acknowledgment Number  
    uint16_t flags;        // Control flags (SYN, ACK, FIN)
    uint16_t window_size;  // Flow control window size
    uint16_t datalen;      // Data length (for convenience)
    uint16_t reserved;     // Reserved for future use
};

// Flag values
#define SHAM_SYN 0x1
#define SHAM_ACK 0x2
#define SHAM_FIN 0x4

// Function declarations
void usage(const char *prog);

// Logging functions
int sham_logging_enabled(void);
FILE *sham_get_log_file(void);
void sham_log_event(const char *fmt, ...);
void sham_close_log(void);

// Handshake functions
int sham_connect(int sockfd, const struct sockaddr_in *server_addr, socklen_t addrlen, uint32_t *my_seq, uint32_t *peer_seq);
int sham_accept(int sockfd, struct sockaddr_in *client_addr, socklen_t *addrlen, uint32_t *my_seq, uint32_t *peer_seq);
int sham_close_initiator(int sockfd, const struct sockaddr_in *peer_addr, socklen_t addrlen, uint32_t seq, uint32_t ack);
int sham_close_responder(int sockfd, struct sockaddr_in *peer_addr, socklen_t *addrlen, uint32_t seq, uint32_t ack);

// File transfer functions  
int sham_send_file_fc(int sockfd, const struct sockaddr_in *peer_addr, socklen_t addrlen, FILE *fp, uint32_t seq_start);
int sham_recv_file_fc(int sockfd, FILE *fp, uint32_t seq_start, struct sockaddr_in *peer_addr, socklen_t *addrlen, size_t buffer_capacity);
int sham_send_file(int sockfd, const struct sockaddr_in *peer_addr, socklen_t addrlen, FILE *fp, uint32_t seq_start);
int sham_recv_file(int sockfd, FILE *fp, uint32_t seq_start, struct sockaddr_in *peer_addr, socklen_t *addrlen);

// Global loss rate for packet loss simulation
extern float sham_loss_rate;

#endif // SHAM_H

//<-------LLM GENERATED CODE ENDS HERE-------->