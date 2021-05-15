#ifndef COMUNICARPCS_COMPATINET_H
#define COMUNICARPCS_COMPATINET_H

#ifdef _WIN32
#include <winsock2.h>
#else
#include <asm-generic/errno.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#endif

#ifdef _WIN32
#define CAST_TO_SEND_BUFFER(x) ((const char*) (x))
#define CAST_TO_RECV_BUFFER(x) ((char*) (x))
#define ADD_FLAG_NONBLOCKING(x) (x)
#define SHUTDOWN_ALL SD_BOTH
#define ACCEPT_LEN_TYPE int
#else
#define CAST_TO_SEND_BUFFER(x) ((const void*) (x))
#define CAST_TO_RECV_BUFFER(x) ((void*) (x))
#define ADD_FLAG_NONBLOCKING(x) (x | MSG_DONTWAIT)
#define SHUTDOWN_ALL SHUT_RDWR
#define ACCEPT_LEN_TYPE socklen_t
#endif

#endif //COMUNICARPCS_COMPATINET_H
