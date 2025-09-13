// S.H.A.M. Client: File Transfer and Chat Mode
#include "sham.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/time.h>
#define _GNU_SOURCE
#include <sys/types.h>
#define BUF_SIZE 2048

// ---- BEGIN: sham_log.c ----
#include <time.h>
static FILE *sham_log_fp = NULL;
static int sham_log_checked = 0;
static int sham_log_enabled = 0;
int sham_logging_enabled() {
    if (!sham_log_checked) {
        const char *env = getenv("RUDP_LOG");
        sham_log_enabled = (env && strcmp(env, "1") == 0);
        sham_log_checked = 1;
    }
    return sham_log_enabled;
}
FILE *sham_get_log_file() {
    if (!sham_logging_enabled()) return NULL;
    if (sham_log_fp) return sham_log_fp;
    const char *role = getenv("SHAM_LOG_ROLE");
    const char *fname = "server_log.txt";
    if (role && strcmp(role, "client") == 0) fname = "client_log.txt";
    else if (role && strcmp(role, "server") == 0) fname = "server_log.txt";
    else {
        FILE *cmd = fopen("/proc/self/cmdline", "r");
        static char buf[256];
        if (cmd) {
            size_t n = fread(buf, 1, sizeof(buf)-1, cmd);
            fclose(cmd);
            buf[n] = '\0';
            if (strstr(buf, "client")) fname = "client_log.txt";
            else if (strstr(buf, "server")) fname = "server_log.txt";
        }
    }
    sham_log_fp = fopen(fname, "a");
    return sham_log_fp;
}
void sham_log_event(const char *fmt, ...) {
    if (!sham_logging_enabled()) return;
    FILE *fp = sham_get_log_file();
    if (!fp) return;
    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct tm *tm = localtime(&tv.tv_sec);
    char tbuf[64];
    strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", tm);
    fprintf(fp, "[%s.%06ld] [LOG] ", tbuf, (long)tv.tv_usec);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(fp, fmt, ap);
    va_end(ap);
    fprintf(fp, "\n");
    fflush(fp);
}
void sham_close_log() {
    if (sham_log_fp) fclose(sham_log_fp);
    sham_log_fp = NULL;
    sham_log_checked = 0;
    sham_log_enabled = 0;
}
// ---- END: sham_log.c ----

// ---- BEGIN: sham_handshake.c ----
#include <time.h>
typedef struct sockaddr_in sham_addr_t;
// Loss guard counter: when >0, artificial loss disabled (used only during 3-way handshake and 4-way termination)
static int sham_loss_guard = 0;
static inline void sham_loss_disable_push(){ sham_loss_guard++; }
static inline void sham_loss_disable_pop(){ if (sham_loss_guard>0) sham_loss_guard--; }
static uint32_t sham_random_seq() {
    srand(time(NULL) ^ getpid());
    return (uint32_t)rand();
}
static int sham_send_packet(int sockfd, const struct sham_header *hdr, const void *data, size_t data_len, const sham_addr_t *addr, socklen_t addrlen) {
    float r;
    char buf[sizeof(struct sham_header) + 4096];
    memcpy(buf, hdr, sizeof(struct sham_header));
    if (data && data_len > 0) memcpy(buf + sizeof(struct sham_header), data, data_len);
    // Loss only when guard inactive and not SYN/FIN (ACKs may be lost during data phase intentionally)
    if (!sham_loss_guard && sham_loss_rate > 0.0f && !(hdr->flags & (SHAM_SYN|SHAM_FIN))) {
        r = (float)rand() / (float)RAND_MAX;
      //  printf("Random: %.4f, Loss Rate: %.4f\n", r, sham_loss_rate);
        if (r < sham_loss_rate) {
            sham_log_event("[LOG] DROP DATA SEQ=%u", hdr->seq_num);
            return (int)(sizeof(struct sham_header) + data_len); // Pretend sent
        }
    }
   // printf("Random: %.4f, Loss Rate: %.4f\n", r, sham_loss_rate);
    return sendto(sockfd, buf, sizeof(struct sham_header) + data_len, 0, (const struct sockaddr *)addr, addrlen);
}
static int sham_recv_packet(int sockfd, struct sham_header *hdr, void *data, size_t max_data, sham_addr_t *addr, socklen_t *addrlen) {
    char buf[sizeof(struct sham_header) + 4096];
    int n = recvfrom(sockfd, buf, sizeof(buf), 0, (struct sockaddr *)addr, addrlen);
    if (n < (int)sizeof(struct sham_header)) return -1;
    memcpy(hdr, buf, sizeof(struct sham_header));
    if (data && n > (int)sizeof(struct sham_header)) memcpy(data, buf + sizeof(struct sham_header), n - sizeof(struct sham_header));
    return n - sizeof(struct sham_header);
}
int sham_connect(int sockfd, const sham_addr_t *server_addr, socklen_t addrlen, uint32_t *my_seq, uint32_t *peer_seq) {
    sham_loss_disable_push();
    struct sham_header syn = {0};
    *my_seq = sham_random_seq();
    syn.seq_num = *my_seq;
    syn.flags = SHAM_SYN;
    syn.window_size = 4096;
    if (sham_send_packet(sockfd, &syn, NULL, 0, server_addr, addrlen) < 0) { sham_loss_disable_pop(); return -1; }
    sham_log_event("[LOG] SND SYN SEQ=%u", syn.seq_num);
    struct sham_header synack;
    sham_addr_t from; socklen_t fromlen = sizeof(from);
    if (sham_recv_packet(sockfd, &synack, NULL, 0, &from, &fromlen) < 0) { sham_loss_disable_pop(); return -1; }
    if (!(synack.flags & SHAM_SYN) || !(synack.flags & SHAM_ACK)) { sham_loss_disable_pop(); return -1; }
    *peer_seq = synack.seq_num;
    if (synack.ack_num != *my_seq + 1) return -1;
    sham_log_event("[LOG] RCV SYN-ACK SEQ=%u ACK=%u", synack.seq_num, synack.ack_num);
    struct sham_header ack = {0};
    ack.seq_num = *my_seq + 1;
    ack.ack_num = *peer_seq + 1;
    ack.flags = SHAM_ACK;
    ack.window_size = 4096;
    if (sham_send_packet(sockfd, &ack, NULL, 0, server_addr, addrlen) < 0) { sham_loss_disable_pop(); return -1; }
    sham_log_event("[LOG] SND ACK=%u WIN=%u", ack.ack_num, ack.window_size);
    sham_loss_disable_pop();
    return 0;
}
int sham_accept(int sockfd, sham_addr_t *client_addr, socklen_t *addrlen, uint32_t *my_seq, uint32_t *peer_seq) {
    struct sham_header syn;
    if (sham_recv_packet(sockfd, &syn, NULL, 0, client_addr, addrlen) < 0) return -1;
    if (!(syn.flags & SHAM_SYN)) return -1;
    *peer_seq = syn.seq_num;
    *my_seq = sham_random_seq();
    sham_log_event("[LOG] RCV SYN SEQ=%u", syn.seq_num);
    struct sham_header synack = {0};
    synack.seq_num = *my_seq;
    synack.ack_num = *peer_seq + 1;
    synack.flags = SHAM_SYN | SHAM_ACK;
    synack.window_size = 4096;
    if (sham_send_packet(sockfd, &synack, NULL, 0, client_addr, *addrlen) < 0) return -1;
    sham_log_event("[LOG] SND SYN-ACK SEQ=%u ACK=%u", synack.seq_num, synack.ack_num);
    struct sham_header ack;
    if (sham_recv_packet(sockfd, &ack, NULL, 0, client_addr, addrlen) < 0) return -1;
    if (!(ack.flags & SHAM_ACK)) return -1;
    if (ack.ack_num != *my_seq + 1) return -1;
    sham_log_event("[LOG] RCV ACK=%u", ack.ack_num);
    return 0;
}
int sham_close_initiator(int sockfd, const sham_addr_t *peer_addr, socklen_t addrlen, uint32_t seq, uint32_t ack) {
    // Disable artificial loss during termination
    sham_loss_disable_push();
    // Initiator side of 4-way close: FIN -> ACK, then wait for FIN -> ACK
    // Robust to duplicates/out-of-order and timeouts.
    const int timeout_ms = 500;
    int fin_retries = 8;

    uint32_t my_next = seq + 1; // Next seq after our FIN
    int acked_my_fin = 0;
    int got_peer_fin = 0;
    struct sham_header fin = {0};
    fin.seq_num = seq;
    fin.flags = SHAM_FIN;
    fin.window_size = 4096;
    if (sham_send_packet(sockfd, &fin, NULL, 0, peer_addr, addrlen) < 0) { sham_loss_disable_pop(); return -1; }
    sham_log_event("[LOG] SND FIN SEQ=%u", fin.seq_num);

    while (1) {
        fd_set rfds; FD_ZERO(&rfds); FD_SET(sockfd, &rfds);
        struct timeval tv; tv.tv_sec = 0; tv.tv_usec = timeout_ms * 1000;
        int rv = select(sockfd + 1, &rfds, NULL, NULL, &tv);
        if (rv == 0) {
            // Timeout
            if (!acked_my_fin && fin_retries-- > 0) {
                // Retransmit our FIN until it's acked
                sham_log_event("[LOG] TIMEOUT waiting ACK of FIN; RETX FIN SEQ=%u", fin.seq_num);
                if (sham_send_packet(sockfd, &fin, NULL, 0, peer_addr, addrlen) < 0) { sham_loss_disable_pop(); return -1; }
                continue;
            }
            // If we've acked our FIN and already sent final ACK for peer FIN, we can exit.
            if (acked_my_fin && got_peer_fin) { sham_loss_disable_pop(); return 0; }
            // Keep waiting otherwise.
            continue;
        }
        if (rv < 0) {
            if (errno == EINTR) continue; sham_loss_disable_pop(); return -1;
        }

        if (FD_ISSET(sockfd, &rfds)) {
            struct sham_header hdr; sham_addr_t from; socklen_t fromlen = sizeof(from);
            int n = sham_recv_packet(sockfd, &hdr, NULL, 0, &from, &fromlen);
            if (n < 0) continue;
            if (hdr.flags & SHAM_ACK) {
                // Expect ACK of our FIN
                sham_log_event("[LOG] RCV ACK=%u", hdr.ack_num);
                // Accept any ACK that covers our FIN
                if (hdr.ack_num >= my_next) acked_my_fin = 1;
                if (acked_my_fin && got_peer_fin) { sham_loss_disable_pop(); return 0; }
            } else if (hdr.flags & SHAM_FIN) {
                // Peer half-closes; ACK immediately
                sham_log_event("[LOG] RCV FIN SEQ=%u", hdr.seq_num);
                struct sham_header ack2 = (struct sham_header){0};
                ack2.seq_num = my_next;              // any valid next seq
                ack2.ack_num = hdr.seq_num + 1;
                ack2.flags = SHAM_ACK;
                ack2.window_size = 4096;
                if (sham_send_packet(sockfd, &ack2, NULL, 0, peer_addr, addrlen) < 0) { sham_loss_disable_pop(); return -1; }
                sham_log_event("[LOG] SND ACK=%u WIN=%u", ack2.ack_num, ack2.window_size);
                got_peer_fin = 1;
                if (acked_my_fin) { sham_loss_disable_pop(); return 0; }
                // else continue waiting for ACK of our FIN
            } else {
                // Ignore other packet types here
                continue;
            }
        }
    }
}
int sham_close_responder(int sockfd, sham_addr_t *peer_addr, socklen_t *addrlen, uint32_t seq, uint32_t ack) {
    // Disable artificial loss during termination
    sham_loss_disable_push();
    // Responder side: RCV FIN -> SND ACK -> SND FIN -> RCV ACK
    const int timeout_ms = 500;
    int fin_retries = 8;

    struct sham_header fin;
    while (1) {
        if (sham_recv_packet(sockfd, &fin, NULL, 0, peer_addr, addrlen) < 0) continue;
        if (fin.flags & SHAM_FIN) break; // got FIN
        // Ignore other packets during close
    }
    sham_log_event("[LOG] RCV FIN SEQ=%u", fin.seq_num);

    struct sham_header ack1 = {0};
    ack1.seq_num = ack;
    ack1.ack_num = fin.seq_num + 1;
    ack1.flags = SHAM_ACK;
    ack1.window_size = 4096;
    if (sham_send_packet(sockfd, &ack1, NULL, 0, peer_addr, *addrlen) < 0) { sham_loss_disable_pop(); return -1; }
    sham_log_event("[LOG] SND ACK=%u WIN=%u", ack1.ack_num, ack1.window_size);

    struct sham_header fin2 = {0};
    fin2.seq_num = seq;
    fin2.flags = SHAM_FIN;
    fin2.window_size = 4096;
    if (sham_send_packet(sockfd, &fin2, NULL, 0, peer_addr, *addrlen) < 0) { sham_loss_disable_pop(); return -1; }
    sham_log_event("[LOG] SND FIN SEQ=%u", fin2.seq_num);

    // Wait for final ACK; on timeout, retransmit our FIN; handle duplicate FINs
    while (1) {
        fd_set rfds; FD_ZERO(&rfds); FD_SET(sockfd, &rfds);
        struct timeval tv; tv.tv_sec = 0; tv.tv_usec = timeout_ms * 1000;
        int rv = select(sockfd + 1, &rfds, NULL, NULL, &tv);
        if (rv == 0) {
            if (fin_retries-- > 0) {
                sham_log_event("[LOG] TIMEOUT waiting final ACK; RETX FIN SEQ=%u", fin2.seq_num);
                if (sham_send_packet(sockfd, &fin2, NULL, 0, peer_addr, *addrlen) < 0) { sham_loss_disable_pop(); return -1; }
                continue;
            }
            sham_loss_disable_pop();
            return 0;
        }
        if (rv < 0) {
            if (errno == EINTR) continue; sham_loss_disable_pop(); return -1;
        }
        if (FD_ISSET(sockfd, &rfds)) {
            struct sham_header hdr; sham_addr_t from; socklen_t fromlen = sizeof(from);
            int n = sham_recv_packet(sockfd, &hdr, NULL, 0, &from, &fromlen);
            if (n < 0) continue;
            if (hdr.flags & SHAM_ACK) {
                sham_log_event("[LOG] RCV ACK=%u", hdr.ack_num);
                sham_loss_disable_pop();
                return 0;
            } else if (hdr.flags & SHAM_FIN) {
                // Peer retransmitted FIN; re-ACK and keep waiting for final ACK
                sham_log_event("[LOG] RCV DUP FIN SEQ=%u", hdr.seq_num);
                ack1.ack_num = hdr.seq_num + 1;
                if (sham_send_packet(sockfd, &ack1, NULL, 0, peer_addr, *addrlen) < 0) { sham_loss_disable_pop(); return -1; }
                sham_log_event("[LOG] SND ACK=%u WIN=%u", ack1.ack_num, ack1.window_size);
                continue;
            }
        }
    }
}
// ---- END: sham_handshake.c ----

// ---- BEGIN: sham_flow_control.c ----
#include <time.h>
float sham_loss_rate = 0.0f;
#define SHAM_DATA_SIZE 1024
#define SHAM_WINDOW_PKTS 10
#define SHAM_RTO_MS 500
#define SHAM_MAX_WINDOW (SHAM_WINDOW_PKTS * SHAM_DATA_SIZE)
struct sham_pktbuf {
    struct sham_header hdr;
    char data[SHAM_DATA_SIZE];
    size_t datalen;
    int sent;
    struct timeval sent_time;
    int acked;
};
static uint64_t now_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}
int sham_send_file_fc(int sockfd, const struct sockaddr_in *peer_addr, socklen_t addrlen, FILE *fp, uint32_t seq_start) {
    struct sham_pktbuf pkts[SHAM_WINDOW_PKTS * 2];
    int base = 0, next = 0, total = 0, eof = 0;
    uint32_t seq = seq_start;
    uint32_t last_acked = seq_start;
    uint32_t receiver_window = SHAM_MAX_WINDOW;
    for (int i = 0; i < SHAM_WINDOW_PKTS; ++i) {
        size_t n = fread(pkts[i].data, 1, SHAM_DATA_SIZE, fp);
        pkts[i].datalen = n;
        pkts[i].hdr.seq_num = seq;
        pkts[i].hdr.ack_num = 0;
        pkts[i].hdr.flags = 0;
        pkts[i].hdr.window_size = SHAM_MAX_WINDOW;
        pkts[i].sent = 0; pkts[i].acked = 0;
        seq += n;
        next++;
        total++;
        if (n < SHAM_DATA_SIZE) { eof = 1; break; }
    }
    int window = SHAM_WINDOW_PKTS;
    while (base < total) {
        uint32_t inflight = 0;
        for (int i = base; i < next; ++i) {
            if (!pkts[i].acked) inflight += pkts[i].datalen;
        }
        for (int i = base; i < next && i < base + window; ++i) {
            int retrans = 0;
            if (!pkts[i].sent) {
                retrans = 0;
            } else if (!pkts[i].acked && (now_ms() - (pkts[i].sent_time.tv_sec * 1000 + pkts[i].sent_time.tv_usec / 1000) > SHAM_RTO_MS)) {
                retrans = 1;
            }
            if (!pkts[i].sent || retrans) {
                if (inflight + pkts[i].datalen <= receiver_window) {
                    if (retrans) {
                        sham_log_event("[LOG] TIMEOUT SEQ=%u", pkts[i].hdr.seq_num);
                    }
                    sham_send_packet(sockfd, &pkts[i].hdr, pkts[i].data, pkts[i].datalen, peer_addr, addrlen);
                    gettimeofday(&pkts[i].sent_time, NULL);
                    pkts[i].sent = 1;
                    inflight += pkts[i].datalen;
                    if (retrans) {
                        sham_log_event("[LOG] RETX DATA SEQ=%u LEN=%zu", pkts[i].hdr.seq_num, pkts[i].datalen);
                    } else {
                        sham_log_event("[LOG] SND DATA SEQ=%u LEN=%zu", pkts[i].hdr.seq_num, pkts[i].datalen);
                    }
                }
            }
        }
        fd_set rfds; FD_ZERO(&rfds); FD_SET(sockfd, &rfds);
        struct timeval tv; tv.tv_sec = 0; tv.tv_usec = 100 * 1000;
        int rv = select(sockfd+1, &rfds, NULL, NULL, &tv);
        if (rv > 0 && FD_ISSET(sockfd, &rfds)) {
            struct sham_header ack;
            struct sockaddr_in from; socklen_t fromlen = sizeof(from);
            int n = sham_recv_packet(sockfd, &ack, NULL, 0, &from, &fromlen);
            if (n >= 0 && (ack.flags & SHAM_ACK)) {
                receiver_window = ack.window_size;
                sham_log_event("[LOG] RCV ACK=%u", ack.ack_num);
                sham_log_event("[LOG] FLOW WIN UPDATE=%u", ack.window_size);
                for (int i = base; i < next; ++i) {
                    uint32_t pkt_end = pkts[i].hdr.seq_num + pkts[i].datalen;
                    if (!pkts[i].acked && pkt_end <= ack.ack_num) {
                        pkts[i].acked = 1;
                        last_acked = ack.ack_num;
                    }
                }
                while (base < next && pkts[base].acked) base++;
                while (next < base + window && !eof) {
                    size_t n = fread(pkts[next].data, 1, SHAM_DATA_SIZE, fp);
                    pkts[next].datalen = n;
                    pkts[next].hdr.seq_num = seq;
                    pkts[next].hdr.ack_num = 0;
                    pkts[next].hdr.flags = 0;
                    pkts[next].hdr.window_size = SHAM_MAX_WINDOW;
                    pkts[next].sent = 0; pkts[next].acked = 0;
                    seq += n;
                    next++;
                    total++;
                    if (n < SHAM_DATA_SIZE) { eof = 1; break; }
                }
            }
        }
    }
    return 0;
}
int sham_recv_file_fc(int sockfd, FILE *fp, uint32_t seq_start, struct sockaddr_in *peer_addr, socklen_t *addrlen, size_t buffer_capacity) {
    uint32_t expected = seq_start;
    struct {
        uint32_t seq;
        char data[SHAM_DATA_SIZE];
        size_t datalen;
    } buffer[SHAM_WINDOW_PKTS * 2];
    int bufused[SHAM_WINDOW_PKTS * 2] = {0};
    size_t used_bytes = 0;
    srand((unsigned)time(NULL) ^ getpid());
    while (1) {
        struct sham_header hdr;
        char data[SHAM_DATA_SIZE];
        int n = sham_recv_packet(sockfd, &hdr, data, sizeof(data), peer_addr, addrlen);
        if (n < 0) continue;
        if (sham_loss_rate > 0.0f && !(hdr.flags & (SHAM_ACK|SHAM_FIN))) {
            float r = (float)rand() / (float)RAND_MAX;
            if (r < sham_loss_rate) {
                sham_log_event("[LOG] DROP DATA SEQ=%u", hdr.seq_num);
                continue;
            }
        }
        if (n == 0 && (hdr.flags & SHAM_FIN)) break;
        if (hdr.seq_num == expected) {
            sham_log_event("[LOG] RCV DATA SEQ=%u LEN=%d", hdr.seq_num, n);
            fwrite(data, 1, n, fp);
            expected += n;
            // Do NOT decrement used_bytes here. used_bytes tracks only buffered out-of-order bytes.
            // Previously this subtraction could underflow (since used_bytes can be 0 for in-order data),
            // producing a huge window_size in ACK (after unsigned wrap) and collapsing sender progress
            // on large transfers. We only subtract when consuming an actually buffered segment below.
            for (int i = 0; i < SHAM_WINDOW_PKTS * 2; ++i) {
                if (bufused[i] && buffer[i].seq == expected) {
                    sham_log_event("[LOG] RCV DATA SEQ=%u LEN=%zu", buffer[i].seq, buffer[i].datalen);
                    fwrite(buffer[i].data, 1, buffer[i].datalen, fp);
                    expected += buffer[i].datalen;
                    used_bytes -= buffer[i].datalen;
                    bufused[i] = 0;
                }
            }
        } else if (hdr.seq_num > expected) {
            if (used_bytes + n <= buffer_capacity) {
                int found = 0;
                for (int i = 0; i < SHAM_WINDOW_PKTS * 2; ++i) {
                    if (!bufused[i]) {
                        buffer[i].seq = hdr.seq_num;
                        memcpy(buffer[i].data, data, n);
                        buffer[i].datalen = n;
                        bufused[i] = 1;
                        used_bytes += n;
                        found = 1;
                        break;
                    }
                }
            }
        }
        struct sham_header ack = {0};
        ack.seq_num = 0;
        ack.ack_num = expected;
        ack.flags = SHAM_ACK;
        ack.window_size = (uint16_t)(buffer_capacity - used_bytes);
        sham_log_event("[LOG] SND ACK=%u WIN=%u", ack.ack_num, ack.window_size);
        sham_send_packet(sockfd, &ack, NULL, 0, peer_addr, *addrlen);
    }
    return 0;
}
// ---- END: sham_flow_control.c ----

// ---- BEGIN: sham_reliable.c ----
#define SHAM_DATA_SIZE 1024
#define SHAM_WINDOW_PKTS 10
#define SHAM_RTO_MS 500
struct sham_pktbuf2 {
    struct sham_header hdr;
    char data[SHAM_DATA_SIZE];
    size_t datalen;
    int sent;
    struct timeval sent_time;
    int acked;
};
static uint64_t now_ms2() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}
typedef struct sockaddr_in sham_addr_t2;
int sham_send_file(int sockfd, const sham_addr_t2 *peer_addr, socklen_t addrlen, FILE *fp, uint32_t seq_start) {
    struct sham_pktbuf2 pkts[SHAM_WINDOW_PKTS * 2];
    int base = 0, next = 0, total = 0, eof = 0;
    uint32_t seq = seq_start;
    for (int i = 0; i < SHAM_WINDOW_PKTS; ++i) {
        size_t n = fread(pkts[i].data, 1, SHAM_DATA_SIZE, fp);
        pkts[i].datalen = n;
        pkts[i].hdr.seq_num = seq;
        pkts[i].hdr.ack_num = 0;
        pkts[i].hdr.flags = 0;
        pkts[i].hdr.window_size = SHAM_WINDOW_PKTS * SHAM_DATA_SIZE;
        pkts[i].sent = 0; pkts[i].acked = 0;
        seq += n;
        next++;
        total++;
        if (n < SHAM_DATA_SIZE) { eof = 1; break; }
    }
    int window = SHAM_WINDOW_PKTS;
    while (base < total) {
        for (int i = base; i < next && i < base + window; ++i) {
            if (!pkts[i].sent || (pkts[i].sent && !pkts[i].acked && (now_ms2() - (pkts[i].sent_time.tv_sec * 1000 + pkts[i].sent_time.tv_usec / 1000) > SHAM_RTO_MS))) {
                sham_send_packet(sockfd, &pkts[i].hdr, pkts[i].data, pkts[i].datalen, peer_addr, addrlen);
                gettimeofday(&pkts[i].sent_time, NULL);
                pkts[i].sent = 1;
            }
        }
        fd_set rfds; FD_ZERO(&rfds); FD_SET(sockfd, &rfds);
        struct timeval tv; tv.tv_sec = 0; tv.tv_usec = 100 * 1000;
        int rv = select(sockfd+1, &rfds, NULL, NULL, &tv);
        if (rv > 0 && FD_ISSET(sockfd, &rfds)) {
            struct sham_header ack;
            sham_addr_t2 from; socklen_t fromlen = sizeof(from);
            int n = sham_recv_packet(sockfd, &ack, NULL, 0, &from, &fromlen);
            if (n >= 0 && (ack.flags & SHAM_ACK)) {
                for (int i = base; i < next; ++i) {
                    uint32_t pkt_end = pkts[i].hdr.seq_num + pkts[i].datalen;
                    if (!pkts[i].acked && pkt_end <= ack.ack_num) {
                        pkts[i].acked = 1;
                    }
                }
                while (base < next && pkts[base].acked) base++;
                while (next < base + window && !eof) {
                    size_t n = fread(pkts[next].data, 1, SHAM_DATA_SIZE, fp);
                    pkts[next].datalen = n;
                    pkts[next].hdr.seq_num = seq;
                    pkts[next].hdr.ack_num = 0;
                    pkts[next].hdr.flags = 0;
                    pkts[next].hdr.window_size = SHAM_WINDOW_PKTS * SHAM_DATA_SIZE;
                    pkts[next].sent = 0; pkts[next].acked = 0;
                    seq += n;
                    next++;
                    total++;
                    if (n < SHAM_DATA_SIZE) { eof = 1; break; }
                }
            }
        }
    }
    return 0;
}
int sham_recv_file(int sockfd, FILE *fp, uint32_t seq_start, sham_addr_t2 *peer_addr, socklen_t *addrlen) {
    uint32_t expected = seq_start;
    struct {
        uint32_t seq;
        char data[SHAM_DATA_SIZE];
        size_t datalen;
    } buffer[SHAM_WINDOW_PKTS * 2];
    int bufused[SHAM_WINDOW_PKTS * 2] = {0};
    while (1) {
        struct sham_header hdr;
        char data[SHAM_DATA_SIZE];
        int n = sham_recv_packet(sockfd, &hdr, data, sizeof(data), peer_addr, addrlen);
        if (n < 0) continue;
        if (n == 0 && (hdr.flags & SHAM_FIN)) break; // End of file
        if (hdr.seq_num == expected) {
            fwrite(data, 1, n, fp);
            expected += n;
            for (int i = 0; i < SHAM_WINDOW_PKTS * 2; ++i) {
                if (bufused[i] && buffer[i].seq == expected) {
                    fwrite(buffer[i].data, 1, buffer[i].datalen, fp);
                    expected += buffer[i].datalen;
                    bufused[i] = 0;
                }
            }
        } else if (hdr.seq_num > expected) {
            int found = 0;
            for (int i = 0; i < SHAM_WINDOW_PKTS * 2; ++i) {
                if (!bufused[i]) {
                    buffer[i].seq = hdr.seq_num;
                    memcpy(buffer[i].data, data, n);
                    buffer[i].datalen = n;
                    bufused[i] = 1;
                    found = 1;
                    break;
                }
            }
        }
        struct sham_header ack = {0};
        ack.seq_num = 0;
        ack.ack_num = expected;
        ack.flags = SHAM_ACK;
        ack.window_size = SHAM_WINDOW_PKTS * SHAM_DATA_SIZE;
        sham_send_packet(sockfd, &ack, NULL, 0, peer_addr, *addrlen);
    }
    return 0;
}
// ---- END: sham_reliable.c ----

void usage(const char *prog) {
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  %s <server_ip> <server_port> <input_file> <output_file_name> [loss_rate]\n", prog);
    fprintf(stderr, "  %s <server_ip> <server_port> --chat [loss_rate]\n", prog);
    exit(1);
}

int main(int argc, char *argv[]) {
    if (argc < 4) usage(argv[0]);
    int chat_mode = 0;
    setenv("SHAM_LOG_ROLE", "client", 1);
    // Ensure log file is created and first event is logged
    sham_log_event("Client started with argc=%d", argc);
    float loss_rate = 0.0f;
    const char *server_ip = argv[1];
    int server_port = atoi(argv[2]);
    const char *input_file = NULL;
    const char *output_file = NULL;
    if (strcmp(argv[3], "--chat") == 0) {
        chat_mode = 1;
        if (argc >= 5) 
        {
            loss_rate = atof(argv[4]);
            sham_loss_rate = loss_rate;
            // printf("Chat mode with loss rate %.2f\n", loss_rate);
        }
    } else {
        if (argc < 5) usage(argv[0]);
        input_file = argv[3];
        output_file = argv[4];
        if (argc >= 6) 
        {
            loss_rate = atof(argv[5]);
            sham_loss_rate = loss_rate;
            // printf("File transfer mode with loss rate %.2f\n", loss_rate);
        }
    }
    // Socket
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) { perror("socket"); exit(1); }
    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    inet_pton(AF_INET, server_ip, &server_addr.sin_addr);
    socklen_t addrlen = sizeof(server_addr);
    // Handshake
    uint32_t my_seq = 0, peer_seq = 0;
    if (sham_connect(sockfd, &server_addr, addrlen, &my_seq, &peer_seq) < 0) {
        fprintf(stderr, "Handshake failed\n"); exit(1);
    }
    if (!chat_mode) {
        // File transfer mode
        // Step 1: Send output filename as first data packet
        size_t fname_len = strlen(output_file);
        struct sham_header fname_hdr = {0};
        fname_hdr.seq_num = my_seq + 1;
        fname_hdr.flags = 0;
        fname_hdr.window_size = BUF_SIZE;
    // Ensure filename metadata packet is never artificially dropped
    sham_loss_disable_push();
    sham_send_packet(sockfd, &fname_hdr, output_file, fname_len, &server_addr, addrlen);
    sham_loss_disable_pop();
        // Step 2: Send file data (start from next seq)
        FILE *fp = fopen(input_file, "rb");
        if (!fp) { perror("fopen input"); exit(1); }
        sham_send_file_fc(sockfd, &server_addr, addrlen, fp, my_seq + 1 + fname_len);
        fclose(fp);
        // Send FIN
        sham_close_initiator(sockfd, &server_addr, addrlen, my_seq + 1 + fname_len, peer_seq + 1);
        // printf("File sent successfully.\n");
    } else {
        // Chat mode with retransmission and timeout
        #define CHAT_WINDOW 10
        struct chat_pkt {
            struct sham_header hdr;
            char data[BUF_SIZE];
            size_t datalen;
            int sent;
            struct timeval sent_time;
            int acked;
        } pkts[CHAT_WINDOW];
        int base = 0, next = 0;
        uint32_t chat_my_seq = my_seq + 1;
        uint32_t chat_peer_seq = peer_seq + 1;
        int chat_active = 1;
        fd_set rfds;
        uint32_t last_printed_seq = 0;
        while (chat_active) {
            FD_ZERO(&rfds);
            FD_SET(0, &rfds);
            FD_SET(sockfd, &rfds);
            struct timeval tv; tv.tv_sec = 0; tv.tv_usec = 100 * 1000;
            int maxfd = sockfd > 0 ? sockfd : 0;
            int rv = select(maxfd+1, &rfds, NULL, NULL, &tv);
            // Handle user input
            if (FD_ISSET(0, &rfds)) {
                char buf[BUF_SIZE];
                if (!fgets(buf, sizeof(buf), stdin)) break;
                if (strcmp(buf, "/quit\n") == 0) {
                    sham_log_event("User typed /quit, initiating FIN handshake");
                    sham_close_initiator(sockfd, &server_addr, addrlen, chat_my_seq, chat_peer_seq);
                    chat_active = 0;
                    break;
                }
                if (next - base < CHAT_WINDOW) {
                    struct chat_pkt *pkt = &pkts[next % CHAT_WINDOW];
                    pkt->hdr.seq_num = chat_my_seq;
                    pkt->hdr.flags = 0;
                    pkt->hdr.window_size = BUF_SIZE;
                    strncpy(pkt->data, buf, BUF_SIZE);
                    pkt->datalen = strlen(buf);
                    pkt->sent = 0;
                    pkt->acked = 0;
                    // Send immediately
                    sham_send_packet(sockfd, &pkt->hdr, pkt->data, pkt->datalen, &server_addr, addrlen);
                    gettimeofday(&pkt->sent_time, NULL);
                    pkt->sent = 1;
                    sham_log_event("SND DATA SEQ=%u LEN=%zu", pkt->hdr.seq_num, pkt->datalen);
                    chat_my_seq += pkt->datalen;
                    next++;
                }
            }
            // Handle retransmission
            for (int i = base; i < next; ++i) {
                struct chat_pkt *pkt = &pkts[i % CHAT_WINDOW];
                if (pkt->sent && !pkt->acked) {
                    struct timeval now; gettimeofday(&now, NULL);
                    uint64_t sent_ms = pkt->sent_time.tv_sec * 1000 + pkt->sent_time.tv_usec / 1000;
                    uint64_t now_ms = now.tv_sec * 1000 + now.tv_usec / 1000;
                    if (now_ms - sent_ms > 500) {
                        sham_log_event("TIMEOUT SEQ=%u", pkt->hdr.seq_num);
                        sham_send_packet(sockfd, &pkt->hdr, pkt->data, pkt->datalen, &server_addr, addrlen);
                        gettimeofday(&pkt->sent_time, NULL);
                        sham_log_event("RETX DATA SEQ=%u LEN=%zu", pkt->hdr.seq_num, pkt->datalen);
                    }
                }
            }
            // Handle incoming packets
            if (FD_ISSET(sockfd, &rfds)) {
                struct sham_header hdr;
                char data[BUF_SIZE];
                struct sockaddr_in from; socklen_t fromlen = sizeof(from);
                int n = sham_recv_packet(sockfd, &hdr, data, sizeof(data), &from, &fromlen);
                if (n > 0 && !(hdr.flags & SHAM_FIN)) {
                    if (hdr.seq_num != last_printed_seq) {
                        fwrite(data, 1, n, stdout);
                        fflush(stdout);
                        last_printed_seq = hdr.seq_num;
                    }
                    sham_log_event("RCV DATA SEQ=%u LEN=%d", hdr.seq_num, n);
                    chat_peer_seq = hdr.seq_num + n;
                    // Send ACK for received data
                    struct sham_header ack = {0};
                    ack.seq_num = 0;
                    ack.ack_num = chat_peer_seq;
                    ack.flags = SHAM_ACK;
                    ack.window_size = BUF_SIZE;
                    sham_log_event("SND ACK=%u WIN=%u", ack.ack_num, ack.window_size);
                    sham_send_packet(sockfd, &ack, NULL, 0, &server_addr, addrlen);
                } else if (hdr.flags & SHAM_ACK) {
                    // Mark sent packets as acked
                    for (int i = base; i < next; ++i) {
                        struct chat_pkt *pkt = &pkts[i % CHAT_WINDOW];
                        uint32_t pkt_end = pkt->hdr.seq_num + pkt->datalen;
                        if (!pkt->acked && pkt_end <= hdr.ack_num) {
                            pkt->acked = 1;
                        }
                    }
                    while (base < next && pkts[base % CHAT_WINDOW].acked) base++;
                } else if (hdr.flags & SHAM_FIN) {
                    sham_log_event("RCV FIN SEQ=%u", hdr.seq_num);
                    sham_close_responder(sockfd, &server_addr, &addrlen, chat_my_seq, chat_peer_seq);
                    chat_active = 0;
                    break;
                }
            }
        }
        #undef CHAT_WINDOW
    }
    close(sockfd);
    return 0;
}
