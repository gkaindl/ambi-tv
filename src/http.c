/* ambi-tv: a flexible ambilight clone for embedded linux
*  Copyright (C) 2013 Georg Kaindl
*  
*  This file is part of ambi-tv.
*  
*  ambi-tv is free software: you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation, either version 2 of the License, or
*  (at your option) any later version.
*  
*  ambi-tv is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*  
*  You should have received a copy of the GNU General Public License
*  along with ambi-tv.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "http.h"
#include "log.h"


#define LOGNAME      "http: "

#define BUF_SIZE           1024
#define BUF_SIZE_MSG       768

#define LISTEN_ADDRESS "127.0.0.1"
#define LISTEN_PORT 10000

#define SELECT_WAIT_MILLIS 5000


typedef struct connection {
   int fd;
   struct sockaddr_in remote_ip_addr;
   char in_buf[BUF_SIZE], out_buf[BUF_SIZE], msg_buf[BUF_SIZE_MSG];
   unsigned in_pos, out_pos, msg_pos;
   struct connection* next;
} connection;

struct http {
   int server_fd;
   int (*handle_keypair_callback) (http*, const char*, const char*);
   connection* cur_conn;
   connection* connections;
};

static connection*
create_connection()
{
   connection* c = (connection*)malloc(sizeof(connection));
   
   if (NULL != c) {
      memset(c, 0, sizeof(connection));
   }
   
   return c;
}

static void
free_connection(connection* c)
{
   if (NULL != c) {
      free(c);
   }
}

static void
insert_connection(http* server, connection* c)
{
   if (NULL != c) {
      c->next = server->connections;
      server->connections = c;
   }
}

static void
remove_connection(http* server, connection* c)
{
   if (NULL != c) {
      connection* t = server->connections, *prev = NULL;
      while (NULL != t) {
         if (t == c) {
            if (NULL == prev) {
               server->connections = t->next;
            } else {
               prev->next = t->next;
            }
                        
            break;
         }
         
         prev = t;
         t = t->next;
      }
   }
}

static int
set_nonblock_fd(int fd)
{
   int opts;
   
   opts = fcntl(fd, F_GETFL);
   if (opts >= 0) {
   
      opts = (opts | O_NONBLOCK);
      if (fcntl(fd, F_SETFL,opts) >= 0) {
         return 0;
      }
   }
   
   return -1;
}

static int
create_server_socket(const char* ip_addr, unsigned port)
{
   struct sockaddr_in addr;
   int opt, fd = socket(AF_INET, SOCK_STREAM, 0);
   
   if (fd < 0) {
      ambitv_log(ambitv_log_error,
         LOGNAME "failed to open tcp server socket: %s\n", strerror(errno));
      return fd;
   }
   
   opt = 1;
   
   if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
       ambitv_log(ambitv_log_error,
          LOGNAME "failed to set SO_REUSEADDR: %s\n", strerror(errno));
   }

   memset(&addr, 0, sizeof(addr));
   
   addr.sin_family = AF_INET;
   addr.sin_addr.s_addr = (NULL != ip_addr)
      ? inet_addr(ip_addr) : htonl(INADDR_ANY);
   addr.sin_port = htons(port);
   
   if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
      ambitv_log(ambitv_log_error,
          LOGNAME "failed to bind tcp server socket: %s\n", strerror(errno));
      close(fd);
      return -1;
   }
   
   if (listen(fd, 16) < 0) {
      ambitv_log(ambitv_log_error,
          LOGNAME "failed to listen with tcp server socket: %s\n",
             strerror(errno));
      close(fd);
      return -1;
   }
   
   
   (void)set_nonblock_fd(fd);
   
   return fd;
}

static void
send_http_reply(connection* c, int status)
{
   char* text = "Unknown";
   
   if (!c) {
      return;
   }
   
   switch (status) {
      case 200: text = "OK"; break;
      case 400: text = "Bad Request"; break;
      case 403: text = "Not authenticated"; break;
      case 405: text = "Method not allowed"; break;
      case 414: text = "URI too long"; break;
      default: break;
   }
   
   c->out_pos = 0;
   snprintf(
      c->out_buf,
      BUF_SIZE,
      "HTTP/1.1 %d %s\r\n"
      "Server: ambi-tv API\r\n"
      "Connection: close\r\n"
      "Cache-Control: no-cache\r\n"
      "Content-Type: application/json\r\n"
      "\r\n"
      "{ \"success\" : \"%s\", \"status\" : \"%d\", \"message\" : \"%s\""
      "%s}\r\n",
         status,
         text,
         (200 == status ? "true" : "false"),
         status,
         text,
         (c->msg_pos > 0 && 200 == status) ? c->msg_buf : ""
   );
}

static int
handle_http_request(http* server, connection* c)
{
   int i;
   char* params = NULL, *name = NULL, *value = NULL;
   
   if (!c) {
      return -1;
   }
   
   if (c->in_pos < 5) {
      close(c->fd);
      remove_connection(server, c);
      free_connection(c);
   
      return -1;
   }
   
   if (0 != strncmp(c->in_buf, "GET /", 5)) {
      send_http_reply(c, 405);
      return 0;
   }
   
   c->in_buf[c->in_pos-1] = 0;
   
   for (i=5; i<c->in_pos; i++) {
      switch (c->in_buf[i]) {
         case ' ':
         case '\r':
         case '\n': {
            c->in_buf[i] = 0;
            params = &c->in_buf[5];
            break;
         }
         
         default:
            break;
      }
      
      if (params) break;
   }
   
   if (!params || '?' != params[0]) {
      send_http_reply(c, 400);
      return 0;
   }
   
   params++;
   
   while (*params) {
      name = params;
      value = NULL;
      
      while (*params) {
         if ('=' == *params) {
            *params = 0;
            value = params;
            params++;
            break;
         }
         
         params++;
      }
      
      value = params;
      
      while (*params) {
         switch (*params) {
            case '&':
            case ' ':
            case '\r':
            case '\n': {
               *params = 0;
               break;
            }
            
            default:
               break;
         }
         
         if (!*params) {
            params++;
            break;
         }
         
         params++;
      }
      
      if (strlen(name) > 0 && strlen(value) > 0) {         
         if (server->handle_keypair_callback) {
            int res;
            
            server->cur_conn = c;
            res = server->handle_keypair_callback(server, name, value);
            server->cur_conn = NULL;
            
            if (0 != res) {
               send_http_reply(c, 400);
               return 0;
            }
         }
         
         name = value = NULL;
      } else {
         send_http_reply(c, 400);
      }
   }
   
   if (name || value) {
      send_http_reply(c, 400);
   } else {
      send_http_reply(c, 200);
   }

   return 0;
}

static void
write_http_reply_to_socket(http* server, connection* c)
{
   if (!c) {
      return;
   }
   
   if (c->out_buf[c->out_pos]) {
      int len = strlen(&c->out_buf[c->out_pos]);
      int sent = write(c->fd, &c->out_buf[c->out_pos], len);
      
      if (len > 0) {
         c->out_pos += sent;
         
         if (0 == c->out_buf[c->out_pos]) {
            goto close_connection;
         }
      } else if (len < 0) {
         goto close_connection;
      }
   } else {
      goto close_connection;
   }
   
   return;
   
close_connection:
   close(c->fd);
   remove_connection(server, c);
   free_connection(c);
}

static void
handle_server_fd(http* server, fd_set* read_fds)
{
   if (FD_ISSET(server->server_fd, read_fds)) {
      struct sockaddr_in addr;
      socklen_t addr_len = sizeof(addr);
      
      int client_fd =
         accept(server->server_fd, (struct sockaddr*)&addr, &addr_len);
      
      if (client_fd < 0) {
         ambitv_log(ambitv_log_warn,
            LOGNAME "failed to dequeue a tcp connection...\n");
      } else {
         connection* c = create_connection();
         
         if (NULL != c) {
            (void)set_nonblock_fd(client_fd);
            
            c->fd = client_fd;
            
            memcpy(
               &c->remote_ip_addr,
               &addr,
               sizeof(struct sockaddr_in)
            );
               
            insert_connection(server, c);
         } else {
            close(client_fd);
         }
      }
   }
}

static void
handle_connection_fds(http* server, fd_set* read_fds, fd_set* write_fds)
{
   connection* t = server->connections;
   
   while (t) {
      connection* next = t->next;
      
      if (FD_ISSET(t->fd, read_fds)) {          
         int len = read(
            t->fd,
            &t->in_buf[t->in_pos],
            BUF_SIZE - t->in_pos - 1
         );

         if (len <= 0) {
            close(t->fd);
         
             ambitv_log(ambitv_log_info, LOGNAME "%s:%u - connection %s\n",
               inet_ntoa(t->remote_ip_addr.sin_addr),
               ntohs(t->remote_ip_addr.sin_port),
               (0 == len) ? "closed" : "broke"
            );
         
            remove_connection(server, t);
            free_connection(t);
         
            goto next_iteration;
         } else {
            t->in_pos += len;
   
            if (
               t->in_pos >= 4 &&
               0 == strncmp("\r\n\r\n", &t->in_buf[t->in_pos - 4], 4)
            ) {
               if (0 != handle_http_request(server, t)) {
                  goto next_iteration;
               }
            } else if (t->in_pos >= BUF_SIZE) {
               send_http_reply(t, 414);
            }
         }
      }
      
      if (FD_ISSET(t->fd, write_fds)) {         
         write_http_reply_to_socket(server, t);
      }
      
      next_iteration: t = next;
   }
}

static void
close_all_fds(http* server)
{
   connection* t = server->connections;
   
   while (t) {
      close(t->fd);
      t = t->next;
   }
}

http*
http_create(const char* ip_addr, unsigned port)
{
   http* server = (http*)malloc(sizeof(http));
   
   if (!server) return NULL;
   
   memset(server, 0, sizeof(http));
   
   server->server_fd = create_server_socket(ip_addr, port);
   
   if (server->server_fd < 0) {
      free(server);
      server = NULL;
   }
   
   return server;
}

void
http_free(http* server)
{
   if (server) {
      close_all_fds(server);
      
      connection* c = server->connections;
      while(c) {
         connection* next = c->next;
         free_connection(c);
         c = next;
      }
      
      free(server);
   }
}

void
http_set_keypair_callback(
   http* server, int (*callback) (http*, const char*, const char* )
) {
   if (server) {
      server->handle_keypair_callback = callback;
   }
}

int
http_reply_keypair(http* server, const char* name, const char* value)
{
   size_t slen, vlen, total_len;
   connection* c = NULL;
   
   if (!server || !server->cur_conn || !name) {
      return -1;
   }
   
   c    = server->cur_conn;
   slen = strlen(name);
   vlen = value ? strlen(value) : 0;

   // magic number 7 = ,"":""\0, e.g. 7 chars
   total_len = slen + vlen + 7;

   if (c->msg_pos + total_len < BUF_SIZE_MSG) {
      snprintf(
         &c->msg_buf[c->msg_pos],
         BUF_SIZE_MSG - c->msg_pos - 1,
         ",\"%s\":\"%s\"",
         name, value ? value : ""
      );
      
      c->msg_pos += total_len - 1;
   } else {
      // no available space, silently drop this.
      return -1;
   }
   
   return 0;
}

int
http_reply_keypair_int(http* server, const char* name, int value)
{
   char buf[32];
   snprintf(buf, 32, "%d", value);
   buf[31] = 0;
   
   return http_reply_keypair(server, name, buf);
}

int
http_fill_fd_sets(
   http* server, fd_set* read_fds, fd_set* write_fds, int cur_max_fd
) {
   if (!read_fds && !write_fds) {
      return cur_max_fd;
   }
   
   int max_fd =
      (server->server_fd > cur_max_fd) ? server->server_fd : cur_max_fd;
   connection* c =
      server->connections;
   
   FD_SET(server->server_fd, read_fds);
   
   while (c) {
      if (c->fd > max_fd) {
         max_fd = c->fd;
      }
         
      FD_SET(c->fd, read_fds);
      
      if (c->out_buf[0] != 0) {
         FD_SET(c->fd, write_fds);
      }
      
      c = c->next;
   }
   
   return max_fd;
}

void
http_handle_fd_sets(http* server, fd_set* read_fds, fd_set* write_fds)
{
   handle_server_fd(server, read_fds);
   handle_connection_fds(server, read_fds, write_fds);
}
