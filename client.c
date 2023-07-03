#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <pthread.h>
#include <time.h>

#define RCV_SOCKET_PORT "3444" // socket number (plate number of istanbul-malatya :p)
#define BUFF_SIZE 1024         // buffer size
#define WINDOW_SIZE 16         // windows size
#define PAYLOAD_SIZE 16        // payload size for rdt packet
#define TIMEOUT 3              // timeout for client
#define SERVER_IP_ADDRESS "172.24.0.10"
#define CLIENT_IP_ADDRESS "172.24.0.20"

// RDT packet definition

struct rdt_packet
{
    char payload[PAYLOAD_SIZE];
    unsigned short sequence_number;
    unsigned short checksum;
    unsigned short is_acked;
    unsigned short eof;
};

// Variables

char *server_port;
char *server_ip;

int send_packet_socketfd, rcv_packet_socketfd;
int send_base, recv_base;
unsigned short next_sequence_number, expected_sequence_number;

struct rdt_packet *send_packets[WINDOW_SIZE];
struct rdt_packet *rcv_packets[WINDOW_SIZE];

time_t start;

// Functions

/**
 * Calculates the checksum for "count" bytes beginning at location "addr". I used checksum when I first started to implement the hw,
 * then i saw forum thread that there is no need to use checksum. so i leave it here
 *
 * @param addr
 * @param count
 * @return it returns calculated checksum
 */

unsigned short checksum_calculation(char *addr, int count)
{
    /*
    This is RFC1071 checksum algorithm. I get it from https://datatracker.ietf.org/doc/html/rfc1071#section-4.1
    The "C" code algorithm computes the checksum with an inner loop that sums 16-bits at a time in a 32-bit accumulator.
     */
    register long sum = 0;
    unsigned short checksum = 0;

    while (count > 1)
    {
        /*  This is the inner loop */
        sum += *(unsigned short *)addr++;
        count -= 2;
    }

    /*  Add left-over byte, if any */
    if (count > 0)
        sum += *(unsigned char *)addr;

    /*  Fold 32-bit sum to 16 bits */
    while (sum >> 16)
        sum = (sum & 0xffff) + (sum >> 16);

    checksum = ~sum;

    return checksum;
}

/*
 * It is a helper function to print a rdt_packet
 */

void print_packet(struct rdt_packet *rdt_packet)
{
    fprintf(stderr, "packet_sequence_number: %d, eof: %d, payload: ", rdt_packet->sequence_number, rdt_packet->eof);

    for (int i = 0; i < PAYLOAD_SIZE; i++)
        fprintf(stderr, "%c", rdt_packet->payload[i]);

    fprintf(stderr, "\n");
}

/**
 * It is a helper funtion to create a new rdt_packet
 *
 * @param payload A payload for a new rdt_packet
 * @param is_acked A is_acked value for a new rdt_packet
 * @return A new allocated rdt_packet
 */

struct rdt_packet *create_packet(char *payload, int is_acked)
{
    struct rdt_packet *temp_rdt_packet;
    temp_rdt_packet = malloc(sizeof(struct rdt_packet));
    temp_rdt_packet->checksum = checksum_calculation(payload, PAYLOAD_SIZE);
    temp_rdt_packet->is_acked = is_acked;

    if (is_acked == 0)
        temp_rdt_packet->sequence_number = next_sequence_number;
    else
        temp_rdt_packet->sequence_number = expected_sequence_number;

    for (int i = 0; i < PAYLOAD_SIZE; i++)
        temp_rdt_packet->payload[i] = payload[i];

    return temp_rdt_packet;
}

// send packet helper function
void udt_send_packet(struct rdt_packet *send_packet)
{
    send(send_packet_socketfd, (void *)send_packet, sizeof(struct rdt_packet), 0);
}

/**
 * GBN Sender
 * Helper function to send rdt_packet. It takes two argument and form a rdt_packet with them and send it.
 * More information in Chapter 3.4 packet send
 *
 * @param send_payload a payload to be sent
 * @param is_acked a is_acked value to be sent
 */
void rdt_send(char *send_payload, int is_acked)
{

    fprintf(stderr, "client rdt_send function is fire!!! \n");

    if (is_acked == 0) // if it is data packet
    {
        int len = strlen(send_payload);
        int index = 0;

        fprintf(stderr, "next_sequence_number: %d, base: %d, max: %d\n", next_sequence_number, send_base, send_base + WINDOW_SIZE); // @TODO: will change it

        while (len > 0)
        {
            if (next_sequence_number < send_base + WINDOW_SIZE)
            {
                char packet_payload[PAYLOAD_SIZE]; // payload
                int counter = 0;
                unsigned short temp_end = 0;
                do
                {
                    if (send_payload[index] == '\n')
                        temp_end = 1;
                    packet_payload[counter++] = send_payload[index++];

                } while (counter < PAYLOAD_SIZE);
                struct rdt_packet *temp_rdt_packet;

                temp_rdt_packet = create_packet(packet_payload, is_acked);
                temp_rdt_packet->checksum = checksum_calculation(send_payload, PAYLOAD_SIZE);
                temp_rdt_packet->sequence_number = next_sequence_number;
                temp_rdt_packet->eof = temp_end;
                send_packets[next_sequence_number % WINDOW_SIZE] = temp_rdt_packet; // added packet to send_packets

                udt_send_packet(temp_rdt_packet);
                print_packet(temp_rdt_packet);

                if (send_base == next_sequence_number)
                {
                    fprintf(stderr, "client timer start ins! \n");
                    // timeout_clock = clock();
                    start = time(NULL);
                }
                // rdt_timer[next_sequence_number % WINDOW_SIZE] = clock();

                next_sequence_number++;
                len -= PAYLOAD_SIZE;
            }
            else
            {
                // refuse data
            }
        }
    }
    else
    {
        char acked_packet_payload[PAYLOAD_SIZE];
        struct rdt_packet *temp_rdt_packet;
        // for (int i = 0; i < PAYLOAD_SIZE; i++)
        // {
        //     acked_packet_payload[i] = is_acked;
        // }

        temp_rdt_packet = create_packet(acked_packet_payload, is_acked);
        udt_send_packet(temp_rdt_packet);
        fprintf(stderr, "client send ack packet for \n");
    }
}

/**
 * GBN Reciever
 * Helper function to recieve rdt_packet. It takes one argument GBN Reciever
 * @param rcv_packet a recieved rdt_packet
 */
void rdt_rcv(struct rdt_packet *rcv_packet)
{

    fprintf(stderr, "client rdt_rcv func is fire!! \n");

    if (rcv_packet->is_acked == 0) // if data packet
    {
        fprintf(stderr, "client packet is received \n");

        if (rcv_packet->sequence_number == expected_sequence_number)
        {
            print_packet(rcv_packet);

            // printf("client packet is not received early\n");
            rcv_packets[(rcv_packet->sequence_number) % WINDOW_SIZE] = rcv_packet; // add packet to rcv_packets
            struct rdt_packet *temp_rdt_packet;
            temp_rdt_packet = create_packet("ACK", 1);
            temp_rdt_packet->sequence_number = expected_sequence_number;
            udt_send_packet(temp_rdt_packet);
            expected_sequence_number++;
        }
    }

    else
    {
        if (rcv_packet->is_acked == 1) // acked pacjet
        {
            fprintf(stderr, "client [rdt_recv] receiver get ackpkt: ignore it.\n");

            //  if (rcv_packet->checksum == checksum_calculation(rcv_packet->payload, PAYLOAD_SIZE))
            // {
            /* code */
            fprintf(stderr, "client slide window\n");

            send_base = rcv_packet->sequence_number + 1;
            if (send_base != next_sequence_number)
            {
                // start timer
                start = time(NULL);
                fprintf(stderr, "stop timer inseallahh \n");
            }
            // else
            // {
            //     printf("start timer inseallahh \n");
            //     start = time(NULL);

            // }

            // }
        }
    }
}

/**
 * timer function to find when timeout happens
 */
void *set_timer() // changed later
{
    // thread will be used with this func
    fprintf(stderr, "client set_timer thread is initialized\n");

    // clock_t now = clock();

    for (;;)
    {
        if ((time(NULL) - start) >= TIMEOUT)
        {
            // printf("client timeout check go-back-n\n");

            for (int i = send_base; i < next_sequence_number; i++)
            {
                // printf("sendpacket %d\n", i);
                if (send_packets[i] != NULL)
                {
                    // printf("client go-back-n\n");
                    udt_send_packet(send_packets[i]);
                }
            }
            start = time(NULL);
        }
    }
}

/**
 * It creates socket for sending packet. packet send
 */
void *create_send_socket()
{
    // I get influenced by this code snippet from man getaddrinfo more detail https://man7.org/linux/man-pages/man3/getaddrinfo.3.html

    fprintf(stderr, "create_send_socket thread is initialized\n");
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    int s;
    char buf[BUFF_SIZE];

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
    hints.ai_socktype = SOCK_DGRAM; /* Datagram socket */
    hints.ai_flags = AI_PASSIVE;

    // s = getaddrinfo(SERVER_IP_ADDRESS, server_port, &hints, &result);
    s = getaddrinfo(server_ip, server_port, &hints, &result);
    if (s != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
        exit(EXIT_FAILURE);
    }

    /* getaddrinfo() returns a list of address structures.
        Try each address until we successfully connect(2).
        If socket(2) (or connect(2)) fails, we (close the socket
        and) try the next address. */

    for (rp = result; rp != NULL; rp = rp->ai_next)
    {
        send_packet_socketfd = socket(rp->ai_family, rp->ai_socktype,
                                      rp->ai_protocol);
        if (send_packet_socketfd == -1)
            continue;

        if (connect(send_packet_socketfd, rp->ai_addr, rp->ai_addrlen) != -1)
            break; /* Success */

        close(send_packet_socketfd);
    }

    freeaddrinfo(result); /* No longer needed */

    if (rp == NULL)
    { /* No address succeeded */
        fprintf(stderr, "Could not connect\n");
        exit(EXIT_FAILURE);
    }

    // Connection established and ready to send packets

    for (;;)
    {
        memset(buf, 0, BUFF_SIZE);
        fgets(buf, BUFF_SIZE, stdin);

        int len = strlen(buf);

        rdt_send(buf, 0);

        if (buf[len - 2] == ' ' && buf[len - 3] == ' ')
        {
            fprintf(stderr, "-------CLIENT CONN CLOSING---------\n");
            shutdown(send_packet_socketfd, SHUT_RDWR);
            exit(0);
        }
    }
}

/**
 * It creates socket for recieving packet. when packet arrives
 */

void *create_rcv_socket()
{
    // I get influenced by this code snippet from man getaddrinfo more detail https://man7.org/linux/man-pages/man3/getaddrinfo.3.html

    // thread will be used with this func
    fprintf(stderr, "create_rcv_socket thread is initialized\n");

    struct addrinfo hints;
    struct addrinfo *result, *rp;
    int s;
    // struct sockaddr_storage peer_addr;
    // socklen_t peer_addr_len;
    ssize_t nread;
    char buf[BUFF_SIZE];
    int yes = 1;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
    hints.ai_socktype = SOCK_DGRAM; /* Datagram socket */
    hints.ai_flags = AI_PASSIVE;    /* For wildcard IP address */

    s = getaddrinfo(NULL, RCV_SOCKET_PORT, &hints, &result);
    if (s != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
        exit(EXIT_FAILURE);
    }

    /* getaddrinfo() returns a list of address structures.
        Try each address until we successfully bind(2).
        If socket(2) (or bind(2)) fails, we (close the socket
        and) try the next address. */

    for (rp = result; rp != NULL; rp = rp->ai_next)
    {
        rcv_packet_socketfd = socket(rp->ai_family, rp->ai_socktype,
                                     rp->ai_protocol);
        if (rcv_packet_socketfd == -1)
            continue;

        if (setsockopt(rcv_packet_socketfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                       sizeof(int)) == -1)
        {
            perror("setsockopt");
            exit(1);
        }

        if (bind(rcv_packet_socketfd, rp->ai_addr, rp->ai_addrlen) == 0)
            break; /* Success */

        close(rcv_packet_socketfd);
    }

    freeaddrinfo(result); /* No longer needed */

    if (rp == NULL)
    { /* No address succeeded */
        fprintf(stderr, "Could not bind\n");
        exit(EXIT_FAILURE);
    }

    /* Read datagrams and echo them back to sender. */

    fprintf(stderr, "Client is listening...\n");

    for (;;)
    {
        // peer_addr_len = sizeof(peer_addr);
        nread = recv(rcv_packet_socketfd, buf, BUFF_SIZE, 0);
        if (nread == -1)
            continue; /* Ignore failed request */

        struct rdt_packet *p;
        p = (struct rdt_packet *)buf;
        fprintf(stderr, "\n");
        fprintf(stderr, "Client packet recieved.....\n");
        fprintf(stderr, "#######################################################\n");
        fprintf(stderr, "\n");

        rdt_rcv(p);
    }
}

int main(int argc, char *argv[])
{
    pthread_t t_send_socket, t_rcv_socket, t_set_timer;

    if (argc != 3)
    {
        fprintf(stderr, "\nUsage:\n"
                        "------\n"
                        "%s <server_ip_address> <server_port_number>   \n",
                argv[0]);
        exit(EXIT_FAILURE);
        exit(EXIT_FAILURE);
    }

    server_ip = argv[1];
    server_port = argv[2];

    send_base = 1;
    recv_base = 0;
    next_sequence_number = 1;
    expected_sequence_number = 1;

    for (int i = 0; i < WINDOW_SIZE; i++)
    {
        send_packets[i] = NULL;
        rcv_packets[i] = NULL;
    }

    struct rdt_packet *temp_rdt_packet;
    temp_rdt_packet = create_packet("ACK", 1);
    temp_rdt_packet->sequence_number = 0;
    send_packets[0] = temp_rdt_packet;

    pthread_create(&t_send_socket, NULL, create_send_socket, NULL);
    pthread_create(&t_rcv_socket, NULL, create_rcv_socket, NULL);
    pthread_create(&t_set_timer, NULL, set_timer, NULL);

    pthread_join(t_send_socket, NULL);
}
