/* BSD Socket API Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

#define PORT                        CONFIG_EXAMPLE_PORT
#define KEEPALIVE_IDLE              CONFIG_EXAMPLE_KEEPALIVE_IDLE
#define KEEPALIVE_INTERVAL          CONFIG_EXAMPLE_KEEPALIVE_INTERVAL
#define KEEPALIVE_COUNT             CONFIG_EXAMPLE_KEEPALIVE_COUNT
#define MAX_EVENTS                  CONFIG_ESP_MAX_STA_CONN

static const char *TAG = "example";

int fd_A[MAX_EVENTS];    // accepted connection fd
int conn_amount;    // current connection amount
#define BUF_SIZE 128

void showclient()
{
    int i;
    printf("client amount: %d\n", conn_amount);
    for (i = 0; i < MAX_EVENTS; i++) {
        printf("[%d]:%d  ", i, fd_A[i]);
    }
    printf("\n\n");
}

static void tcp_server_task(void *pvParameters)
{
    char rx_buffer[BUF_SIZE];
    int addr_family = (int)pvParameters;
    int ip_protocol = 0;
    struct sockaddr_storage dest_addr;
    fd_set fdsr;
    struct timeval tv;
    int maxsockfd;
    struct sockaddr_in client_addr;

    if (addr_family == AF_INET) {
        struct sockaddr_in *dest_addr_ip4 = (struct sockaddr_in *)&dest_addr;
        dest_addr_ip4->sin_addr.s_addr = htonl(INADDR_ANY);
        dest_addr_ip4->sin_family = AF_INET;
        dest_addr_ip4->sin_port = htons(PORT);
        ip_protocol = IPPROTO_IP;
    }
#ifdef CONFIG_EXAMPLE_IPV6
    else if (addr_family == AF_INET6) {
        struct sockaddr_in6 *dest_addr_ip6 = (struct sockaddr_in6 *)&dest_addr;
        bzero(&dest_addr_ip6->sin6_addr.un, sizeof(dest_addr_ip6->sin6_addr.un));
        dest_addr_ip6->sin6_family = AF_INET6;
        dest_addr_ip6->sin6_port = htons(PORT);
        ip_protocol = IPPROTO_IPV6;
    }
#endif

    int listen_sock = socket(addr_family, SOCK_STREAM, ip_protocol);
    if (listen_sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        vTaskDelete(NULL);
        return;
    }
    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#if defined(CONFIG_EXAMPLE_IPV4) && defined(CONFIG_EXAMPLE_IPV6)
    // Note that by default IPV6 binds to both protocols, it is must be disabled
    // if both protocols used at the same time (used in CI)
    setsockopt(listen_sock, IPPROTO_IPV6, IPV6_V6ONLY, &opt, sizeof(opt));
#endif

    ESP_LOGI(TAG, "Socket created");

    int err = bind(listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err != 0) {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        ESP_LOGE(TAG, "IPPROTO: %d", addr_family);
        goto CLEAN_UP;
    }
    ESP_LOGI(TAG, "Socket bound, port %d", PORT);

    err = listen(listen_sock, MAX_EVENTS);
    if (err != 0) {
        ESP_LOGE(TAG, "Error occurred during listen: errno %d", errno);
        goto CLEAN_UP;
    }


#ifdef CONFIG_EXAMPLE_IPV6
    struct sockaddr_in6 sourceAddr; // Large enough for both IPv4 or IPv6
#else
    struct sockaddr_in sourceAddr;
#endif
    socklen_t addrLen = sizeof(sourceAddr);
    maxsockfd = listen_sock;//use for select() to check the readable fd

    while (1) {
        // initialize file descriptor set
        FD_ZERO(&fdsr);
        FD_SET(listen_sock, &fdsr);

        // timeout setting
        tv.tv_sec = 60;
        tv.tv_usec = 0;

        // add active connection to fd set
        for (int i = 0; i < MAX_EVENTS; i++) {
            if (fd_A[i] != 0) {
                FD_SET(fd_A[i], &fdsr);
            }
        }
        //block here until listen_sock can read
        int ret = select(maxsockfd+1, &fdsr, NULL, NULL, &tv);
        if (ret < 0) {
            perror("select");
            break;
        } else if (ret == 0) {
            printf("timeout\n");
            continue;
        }
            // check every fd in the set and handle the recv()
        for (int i = 0; i < conn_amount; i++) {
            if (FD_ISSET(fd_A[i], &fdsr)) {
                ret = recv(fd_A[i], rx_buffer, sizeof(rx_buffer) - 1, 0);
                if (ret <= 0) {        // client close
                    ESP_LOGE(TAG, "recv failed or connection closed: errno %d", errno);
                    printf("client[%d] close\n", i);
                    close(fd_A[i]);
                    FD_CLR(fd_A[i], &fdsr);
                    fd_A[i] = 0;
                } else {        // receive data                        
                    if (ret < BUF_SIZE)
                        memset(&rx_buffer[ret], '\0', 1);
                    printf("client[%d] send:%s\n", i, rx_buffer);
                    int err = send(fd_A[i], rx_buffer, ret, 0);
                    if (err < 0) {
                        ESP_LOGE(TAG, "Error occured during sending: errno %d", errno);
                        continue;
                    }
                }
            }
        }

        // check whether a new connection comes and handler the new connection
        if (FD_ISSET(listen_sock, &fdsr)) {
            int connect_sock = accept(listen_sock, (struct sockaddr *)&client_addr, &addrLen);
            if (connect_sock <= 0) {
                perror("accept");
                continue;
            }
            ESP_LOGI(TAG, "Socket accepted");

            // add to fd queue
            if (conn_amount < MAX_EVENTS) {
                fd_A[conn_amount++] = connect_sock;
                printf("new connection client[%d] %s:%d\n", conn_amount,
                        inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                if (connect_sock > maxsockfd)
                    maxsockfd = connect_sock;
            }
            else {
                printf("max connections arrive, exit\n");
                send(connect_sock, "bye", 4, 0);
                close(connect_sock);
                continue;
            }
        }
        showclient();
    }

CLEAN_UP:
    close(listen_sock);
    vTaskDelete(NULL);
}

void create_tcp_task(void)
{
#ifdef CONFIG_EXAMPLE_IPV4
    xTaskCreate(tcp_server_task, "tcp_server", 4096, (void*)AF_INET, 5, NULL);
#endif
#ifdef CONFIG_EXAMPLE_IPV6
    xTaskCreate(tcp_server_task, "tcp_server", 4096, (void*)AF_INET6, 5, NULL);
#endif
}
