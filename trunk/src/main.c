/*
 * =====================================================================================
 *
 *       Filename:  main.c
 *
 *    Description:  guarder main
 *
 *        Version:  1.0
 *        Created:  2012年05月27日 18时03分43秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Zilong.Zhu (), zilong.whu@gmail.com
 *        Company:  edu.whu
 *
 * =====================================================================================
 */

#include <errno.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <alsa/iatomic.h>
#include "log.h"
#include "utils.h"
#include "error.h"
#include "structs.h"
#include "alloc.h"
#include "parser.h"

#define WORKER_CONN_NUM_DELTA	5

atomic_t g_worker_num = ATOMIC_INIT(0);

struct config
{
	struct
	{
		char _addr[INET_ADDRSTRLEN];
		unsigned short _port;
	};
	struct
	{
		int _client_connection_timeout;
		int _client_read_timeout;
		int _client_write_timeout;

		int _max_active_conn_num;
	};
} g_conf;

int g_listen_fd = -1;

atomic_t g_active_conn_num = ATOMIC_INIT(0);
atomic_t g_conn_denied_by_limit_num = ATOMIC_INIT(0);
atomic_t g_conn_aborted_err_num = ATOMIC_INIT(0);
atomic_t g_conn_nofile_err_num = ATOMIC_INIT(0);
atomic_t g_conn_nomem_err_num = ATOMIC_INIT(0);
atomic_t g_conn_other_err_num = ATOMIC_INIT(0);

int do_listen()
{
	struct sockaddr_in addr;

	atomic_set(&g_active_conn_num, 0);
	atomic_set(&g_conn_denied_by_limit_num, 0);
	atomic_set(&g_conn_aborted_err_num, 0);
	atomic_set(&g_conn_nofile_err_num, 0);
	atomic_set(&g_conn_nomem_err_num, 0);
	atomic_set(&g_conn_other_err_num, 0);

	bzero(&addr, sizeof addr);
	addr.sin_family = AF_INET;
	addr.sin_port = htons(g_conf._port);

	if ( strncasecmp("any", g_conf._addr, sizeof("any")) == 0 )
	{
		addr.sin_addr.s_addr = htonl(INADDR_ANY);
	}
	else
	{
		int ret = inet_pton(AF_INET, g_conf._addr, &addr.sin_addr.s_addr);
		if ( ret < 0 )
		{
			WARNING("invalid addr[%s], error[%s].", g_conf._addr, strerror_t(errno));
			return -1;
		}
		else if ( 0 == ret )
		{
			WARNING("invalid addr[%s], not inet4 address.", g_conf._addr);
			return -1;
		}
	}

	if ( g_listen_fd >= 0 )
	{
		WARNING("already listend.");
		return -1;
	}
	g_listen_fd = socket(AF_INET, SOCK_STREAM, 0);
	if ( g_listen_fd < 0 )
	{
		WARNING("socket error[%s].", strerror_t(errno));
		return -1;
	}
	const int on = 1;
	if ( setsockopt(g_listen_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0 )
	{
		WARNING("setsockopt reuseable error[%s].", strerror_t(errno));
		goto FAIL;
	}
	if ( bind(g_listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0 )
	{
		WARNING("bind error[%s].", strerror_t(errno));
		goto FAIL;
	}
	if ( listen(g_listen_fd, 10) < 0 )
	{
		WARNING("listen error[%s].", strerror_t(errno));
		goto FAIL;
	}
	NOTICE("listen on [%s:%hu] ok, listen fd[%d].", g_conf._addr, g_conf._port, g_listen_fd);
	return 0;
FAIL:
	SAFE_CLOSE(g_listen_fd);
	g_listen_fd = -1;
	return -1;
}

void do_with_listen(struct worker_context *ctx, netresult_t result)
{
	if ( NET_OP_NOTIFY == result._op_type )
	{
		WARNING("listen fd[%d] error.", g_listen_fd);
		return ;
	}
	int err;
	int sock;
	int avg_conn;
	struct sockaddr_in addr;
	socklen_t addrlen;
	struct connection *conn;

	while (1)
	{
		avg_conn = atomic_read(&g_active_conn_num) / atomic_read(&g_worker_num);
		if ( ctx->_active_conn_num >= avg_conn + WORKER_CONN_NUM_DELTA )
		{
			DEBUG("drop accept ignore unbalance, avg conn num[%d], worker[%d] active conn num[%d].",
					avg_conn, ctx->_wid, ctx->_active_conn_num);
			break;
		}
		addrlen = sizeof(addr);
		sock = accept(g_listen_fd, (struct sockaddr *)&addr, &addrlen);
		if ( sock < 0 )
		{
			err = errno;
			if ( EINTR == err )
			{
				continue;
			}
			else if ( EAGAIN == err )
			{
				return ;
			}
			else if ( EMFILE == err || ENFILE == err )
			{
				atomic_add(1, &g_conn_nofile_err_num);
			}
			else if ( ENOBUFS == err || ENOMEM == err )
			{
				atomic_add(1, &g_conn_nomem_err_num);
			}
			else if ( ECONNABORTED == err )
			{
				atomic_add(1, &g_conn_aborted_err_num);
			}
			else
			{
				atomic_add(1, &g_conn_other_err_num);
			}
			WARNING("accept ret error[%s] on fd[%d].", strerror_t(err), g_listen_fd);
			return ;
		}
		else if ( atomic_read(&g_active_conn_num) > g_conf._max_active_conn_num )
		{
			atomic_add(1, &g_conn_denied_by_limit_num);
			SAFE_CLOSE(sock);
			WARNING("deny connection, too many active connections[> %d].", g_conf._max_active_conn_num);
		}
		else
		{
			atomic_add(1, &g_active_conn_num);
			conn = alloc_connection(ctx);
			if ( NULL != conn )
			{
				conn->_client._fd = sock;
				if ( NULL == inet_ntop(AF_INET, &addr.sin_addr,
							conn->_client._ip_str, sizeof(conn->_client._ip_str)) )
				{
					WARNING("inet_ntop error[%s], addr[%X].",
							strerror_t(errno), *(int *)&addr.sin_addr);
					conn->_client._ip_str[0] = '\0';
				}
				if ( epex_attach(ctx->_epoll, sock, conn, g_conf._client_connection_timeout) )
				{
					if ( epex_read_any(ctx->_epoll, sock, conn->_client._header,
								MAX_HTTP_HEADER_LEN, NULL, g_conf._client_read_timeout) )
					{
						DEBUG("worker[%d] accept connection[%d][%p] ok.",
								ctx->_wid, sock, conn);
						++ctx->_active_conn_num;
						continue;
					}
					else
					{
						WARNING("failed to submit read request.");
						epex_detach(ctx->_epoll, sock, NULL);
						free_connection(ctx, conn);
						SAFE_CLOSE(sock);
					}
				}
				else
				{
					WARNING("failed to attach sock[%d] to epoll.", sock);
					free_connection(ctx, conn);
					SAFE_CLOSE(sock);
				}
			}
			else
			{
				WARNING("failed to alloc connection.");
			}
			atomic_add(-1, &g_active_conn_num);
		}
	}
}

int get_chunk_body_buffer(struct worker_context *ctx, struct connection *conn, char **p_start, int *p_len)
{
	int flag = 0;
	struct body_buffer *buffer;
	if ( DLIST_EMPTY(&conn->_client._body) )
	{
		flag = 1;
	}
	else
	{
		__dlist_t *cur = DLIST_PRE(&conn->_client._body);
		buffer = GET_OWNER(cur, struct body_buffer, _list);
		if ( buffer->_data_len - buffer->_curpos == 0 )
		{
			flag = 1;
		}
	}
	if ( flag )
	{
		buffer = (struct body_buffer *)mp_alloc(ctx->_body_pool);
		if ( NULL == buffer )
		{
			WARNING("failed to alloc mem for body buffer.");
			return -1;
		}
		DLIST_INIT(&buffer->_list);
		buffer->_curpos = 0;
		buffer->_data_len = ctx->_data_len;
		DLIST_INSERT_B(&buffer->_list, &conn->_client._body);
	}
	*p_start = buffer->_data + buffer->_curpos;
	*p_len = buffer->_data_len - buffer->_curpos;
	return 0;
}

int get_body_buffer(struct worker_context *ctx, struct connection *conn, char **p_start, int *p_len)
{
	int len = conn->_client._body_len - conn->_pos;
	if ( len <= 0 )
	{
		WARNING("should not run to here, len < 0.");
		return -1;
	}
	if ( get_chunk_body_buffer(ctx, conn, p_start, p_len) < 0 )
	{
		return -1;
	}
	*p_len = *p_len < len ? *p_len : len;
	return 0;
}

int append_to_body(struct worker_context *ctx, struct connection *conn, const char *start, int len)
{
	char *from;
	int avail;
	__dlist_t *cur;
	struct body_buffer *buffer;
	while ( len > 0 )
	{
		if ( get_body_buffer(ctx, conn, &from, &avail) < 0 )
		{
			WARNING("failed to get body buffer.");
			return -1;
		}
		if ( avail > len )
		{
			avail = len;
		}
		memcpy(from, start, avail);

		start += avail;
		len -= avail;

		cur = DLIST_PRE(&conn->_client._body);
		buffer = GET_OWNER(cur, struct body_buffer, _list);
		buffer->_curpos += avail;
		conn->_pos += avail;
	}
	return 0;
}

int append_to_chunk_body(struct worker_context *ctx, struct connection *conn, const char *start, int len)
{
	char *from;
	int avail;
	__dlist_t *cur;
	struct body_buffer *buffer;
	while ( len > 0 )
	{
		if ( get_chunk_body_buffer(ctx, conn, &from, &avail) < 0 )
		{
			WARNING("failed to get chunk body buffer.");
			return -1;
		}
		if ( avail > len )
		{
			avail = len;
		}
		memcpy(from, start, avail);

		start += avail;
		len -= avail;

		cur = DLIST_PRE(&conn->_client._body);
		buffer = GET_OWNER(cur, struct body_buffer, _list);
		buffer->_curpos += avail;
		conn->_pos += avail;
	}
	return 0;
}

int do_with_read_body(struct worker_context *ctx, struct connection *conn, int closed)
{
	if ( conn->_pos < conn->_client._body_len )
	{
		if ( closed )
		{
			WARNING("sock is closed by peer before body is transfered.");
			return -1;
		}
		int avail;
		char *from;
		if ( get_body_buffer(ctx, conn, &from, &avail) < 0 )
		{
			WARNING("failed to get body buffer.");
			return -1;
		}
		if ( !epex_read(ctx->_epoll, conn->_client._fd,
					from, avail, NULL, g_conf._client_read_timeout) )
		{
			WARNING("failed to submit read request.");
			return -1;
		}
		DEBUG("worker[%d] continue to read body from connection[%d][%p].",
				ctx->_wid, conn->_client._fd, conn);
	}
	else
	{
		DEBUG("going to write to server.");
		conn->_status = CONN_CONNECTING_SERVER;
	}
	return 0;
}

int do_with_read_chunk_body(struct worker_context *ctx, struct connection *conn, int closed)
{
	if ( parse_chunk_body(ctx, conn) < 0 )
	{
		WARNING("failed to parse chunk body.");
		return -1;
	}
	else if (  REQUEST_CHUNK_BODY_STATE_END == conn->_client._state )
	{
		DEBUG("going to write to server.");
		struct body_buffer *buffer = GET_OWNER(conn->_client._cur_block, struct body_buffer, _list);
		conn->_client._next_len = buffer->_curpos - conn->_client._cur_pos;
		conn->_status = CONN_CONNECTING_SERVER;

		int curpos = 0;
		int ret;

		static const char *const s_version[] = 
		{
			"",
			"HTTP/0.9",
			"HTTP/1.0",
			"HTTP/1.1",
		};
		ret = snprintf(ctx->_header, MAX_HTTP_HEADER_LEN, "POST %.*s %s\r\n",
				(int)(conn->_client._uri_end - conn->_client._uri_start),
				conn->_client._uri_start, s_version[conn->_client._version]);
		if ( ret < 0 )
		{
			WARNING("snprintf error[%s].", strerror_t(errno));
			return -1;
		}
		curpos += ret;

		__dlist_t *cur;
		struct http_header *header;
		for ( cur = DLIST_NEXT(&conn->_client._headers); cur != &conn->_client._headers; cur = DLIST_NEXT(cur) )
		{
			header = GET_OWNER(cur, struct http_header, _list);
			if ( header->_name_end - header->_name_start == sizeof("Transfer-Encoding") - 1
					&& strncasecmp(header->_name_start, "Transfer-Encoding", sizeof("Transfer-Encoding") - 1) == 0 )
			{
				DEBUG("ignore Transfer-Encoding header.");
				continue;
			}
			else if ( header->_name_end - header->_name_start == sizeof("Content-Length") - 1
					&& strncasecmp(header->_name_start, "Content-Length", sizeof("Content-Length") - 1) == 0 )
			{
				DEBUG("ignore Content-Length header.");
				continue;
			}
			ret = snprintf(ctx->_header + curpos, MAX_HTTP_HEADER_LEN - curpos, "%.*s:%.*s\r\n",
					(int)(header->_name_end - header->_name_start), header->_name_start,
					(int)(header->_value_end - header->_value_start), header->_value_start);
			if ( ret < 0 )
			{
				WARNING("snprintf error[%s].", strerror_t(errno));
				return -1;
			}
			curpos += ret;
		}
		ret = snprintf(ctx->_header + curpos, MAX_HTTP_HEADER_LEN - curpos, "Content-Length:%d\r\n\r\n", conn->_client._body_len);
		if ( ret < 0 )
		{
			WARNING("snprintf error[%s].", strerror_t(errno));
			return -1;
		}
		if ( ret >= MAX_HTTP_HEADER_LEN - curpos )
		{
			WARNING("too long header, need[%s].", ret + curpos + 1);
			return -1;
		}
		curpos += ret;
		memcpy(conn->_client._header, ctx->_header, curpos);
		conn->_client._header_len = curpos;
		DEBUG("%s", ctx->_header);
	}
	else if ( closed )
	{
		WARNING("sock is closed by peer before body is transfered.");
		return -1;
	}
	else
	{
		char *from;
		int avail;
		if ( get_chunk_body_buffer(ctx, conn, &from, &avail) < 0 )
		{
			WARNING("failed to get chunk body buffer.");
			return -1;
		}
		if ( !epex_read_any(ctx->_epoll, conn->_client._fd,
					from, avail, NULL, g_conf._client_read_timeout) )
		{
			WARNING("failed to submit read request.");
			return -1;
		}
		DEBUG("worker[%d] continue to read chunk body from connection[%d][%p].",
				ctx->_wid, conn->_client._fd, conn);
	}
	return 0;
}

void do_with_read(struct worker_context *ctx, netresult_t result)
{
	int end_pos;
	int left_len;
	const char *body_start;
	__dlist_t *cur;
	struct body_buffer *buffer;
	struct connection *conn = (struct connection *)result._user_ptr2;

	if ( NET_ETIMEOUT == result._status || NET_EIDLE == result._status )
	{
		WARNING("read from sock[%d] timeout.", result._sock_fd);
		goto FAIL;
	}
        else if ( NET_ERROR == result._status )
	{
		WARNING("read from sock[%d] error[%s].", result._sock_fd, strerror_t(result._errno));
		goto FAIL;
	}

	switch (conn->_status)
	{
		case CONN_CLIENT_READING_HEADER:
			end_pos = ((char *)result._buffer - conn->_client._header) + result._curpos;
			if ( parse_request_header(ctx, conn, end_pos) < 0 )
			{
				WARNING("failed to parse request header.");
				goto FAIL;
			}
			else if ( REQUEST_HEADER_STATE_END == conn->_client._state )
			{
				DEBUG("parse headers ok.");
				conn->_client._header_len = conn->_pos;
				if ( do_with_request_headers(conn) < 0 )
				{
					WARNING("failed to process request headers.");
					goto FAIL;
				}
				else if ( HTTP_METHOD_POST == conn->_client._method )
				{
					conn->_pos = 0;
					body_start = conn->_client._header + conn->_client._header_len;
					if ( conn->_client._chunked )
					{
						DEBUG("going to read client chunk body.");
						conn->_client._body_len = 0;
						conn->_status = CONN_CLIENT_READING_CHUNK_BODY;
						conn->_client._state = REQUEST_CHUNK_BODY_STATE_LWS_SIZE;
						if ( append_to_chunk_body(ctx, conn, body_start,
									end_pos - conn->_client._header_len) < 0 )
						{
							WARNING("failed to append to chunk body.");
							goto FAIL;
						}
						if ( do_with_read_chunk_body(ctx, conn, NET_ECLOSED == result._status) < 0 )
						{
							WARNING("faled to do with read chunk body.");
							goto FAIL;
						}
					}
					else if ( conn->_client._body_len > 0 )
					{
						DEBUG("going to read client body, length[%d].", (int)conn->_client._body_len);
						conn->_status = CONN_CLIENT_READING_LENGTH_BODY;
						if ( conn->_client._header_len + conn->_client._body_len <= end_pos )
						{
							conn->_client._next_len = end_pos - conn->_client._header_len
								- conn->_client._body_len;
						}
						else
						{
							conn->_client._next_len = 0;
						}
						if ( append_to_body(ctx, conn, body_start, end_pos - conn->_client._header_len
									- conn->_client._next_len) < 0 )
						{
							WARNING("failed to append to body.");
							goto FAIL;
						}
						if ( do_with_read_body(ctx, conn, NET_ECLOSED == result._status) < 0 )
						{
							WARNING("faled to do with read body.");
							goto FAIL;
						}
						if ( conn->_client._next_len > 0 )
						{
							DEBUG("move next request data[%.*s].", conn->_client._next_len,
									body_start + conn->_client._body_len);
							memmove((char *)body_start, body_start + conn->_client._body_len,
									conn->_client._next_len);
						}
					}
					else
					{
						WARNING("client use post method, do not specify body len.");
						goto FAIL;
					}
				}
				else
				{
					DEBUG("going to connect server.");
					conn->_client._next_len = end_pos - conn->_client._header_len;
					conn->_status = CONN_CONNECTING_SERVER;
				}
			}
			else
			{
				left_len = MAX_HTTP_HEADER_LEN - end_pos;
				if ( 0 == left_len )
				{
					WARNING("too long request header[> %d].", MAX_HTTP_HEADER_LEN);
					goto FAIL;
				}
				else if ( NET_ECLOSED == result._status )
				{
					WARNING("sock is closed by peer before all headers are transfered.");
					goto FAIL;
				}
				else if ( epex_read_any(ctx->_epoll, result._sock_fd,
							(char *)result._buffer + result._curpos, left_len,
							NULL, g_conf._client_read_timeout) )
				{
					DEBUG("worker[%d] continue to read header from connection[%d][%p].",
							ctx->_wid, result._sock_fd, conn);
				}
				else
				{
					WARNING("failed to submit read request.");
					goto FAIL;
				}
			}
			break;
		case CONN_CLIENT_READING_CHUNK_BODY:
			cur = DLIST_PRE(&conn->_client._body);
			buffer = GET_OWNER(cur, struct body_buffer, _list);
			buffer->_curpos += result._curpos;
			conn->_pos += result._curpos;

			if ( do_with_read_chunk_body(ctx, conn, NET_ECLOSED == result._status) < 0 )
			{
				WARNING("faled to do with read chunk body.");
				goto FAIL;
			}
			break;
		case CONN_CLIENT_READING_LENGTH_BODY:
			cur = DLIST_PRE(&conn->_client._body);
			buffer = GET_OWNER(cur, struct body_buffer, _list);
			buffer->_curpos += result._curpos;
			conn->_pos += result._curpos;

			if ( do_with_read_body(ctx, conn, NET_ECLOSED == result._status) < 0 )
			{
				WARNING("faled to do with read body.");
				goto FAIL;
			}
			break;
	}
	if ( 0 )
	{
FAIL:
		epex_detach(ctx->_epoll, result._sock_fd, NULL);
		free_connection(ctx, conn);
		SAFE_CLOSE(result._sock_fd);
		--ctx->_active_conn_num;
		atomic_add(-1, &g_active_conn_num);
	}
}

void do_with_write(struct worker_context *ctx, netresult_t result)
{

}
void do_with_connect(struct worker_context *ctx, netresult_t result)
{

}
void do_with_notify(struct worker_context *ctx, netresult_t result)
{
	struct connection *conn = (struct connection *)result._user_ptr2;

	if ( NET_EIDLE == result._status )
	{
		WARNING("sock[%d] become idle, so close it.", result._sock_fd);
	}
        else                                    /* must be NET_ERROR */
	{
		WARNING("sock[%d] get error[%s].", result._sock_fd, strerror_t(result._errno));
	}
	free_connection(ctx, conn);
	SAFE_CLOSE(result._sock_fd);
	--ctx->_active_conn_num;
	atomic_add(-1, &g_active_conn_num);
}

void *woker(void *arg)
{
	int i;
	int cnt;
	struct worker_context *ctx = (struct worker_context *)arg;
	netresult_t results[256];
	do
	{
		cnt = epex_poll(ctx->_epoll, results, 256);
		for ( i = 0; i < cnt; ++i )
		{
			if ( results[i]._sock_fd == g_listen_fd )
			{
				do_with_listen(ctx, results[i]);
			}
			else
			{
				switch (results[i]._op_type)
				{
					case NET_OP_NOTIFY:
						do_with_notify(ctx, results[i]);
						break;
					case NET_OP_READ:
						do_with_read(ctx, results[i]);
						break;
					case NET_OP_WRITE:
						do_with_write(ctx, results[i]);
						break;
					case NET_OP_CONNECT:
						do_with_connect(ctx, results[i]);
						break;
					default:
						WARNING("unexpected op type[%d].", (int)results[i]._op_type);
						break;
				}
			}
		}
	} while (1);
	return NULL;
}

int main(int argc, char *argv[])
{
	snprintf(g_conf._addr, sizeof(g_conf._addr), "%s", "any");
	g_conf._port = 12345;
	g_conf._client_connection_timeout = 10000;
	g_conf._client_read_timeout = 100;
	g_conf._client_write_timeout = 100;

	if ( do_listen() < 0 )
	{
		return -1;
	}
	const int worker_thread_num = 10;
	atomic_set(&g_worker_num, worker_thread_num);

	struct worker_context **ctxs = (struct worker_context **)calloc(worker_thread_num, sizeof(struct worker_context*));
	if ( NULL == ctxs )
	{
		WARNING("failed to alloc mem for worker contexts[%d].", worker_thread_num);
		return -1;
	}
	int i;
	int j;
	int ret;
	for ( i = 0; i < worker_thread_num; ++i )
	{
		ctxs[i] = alloc_worker_context(3);
		if ( NULL == ctxs[i] )
		{
			WARNING("failed to init worker context[%d].", i);
			break;
		}
		ctxs[i]->_wid = i;
		if ( !epex_listen(ctxs[i]->_epoll, g_listen_fd, NULL) )
		{
			WARNING("failed to attach listen fd[%d] to epoll[%d].", g_listen_fd, i);
			goto FAIL;
		}
		ret = pthread_create(&ctxs[i]->_tid, NULL, woker, ctxs[i]);
		if ( 0 != ret )
		{
			WARNING("pthread_create[%d] error[%s].", i, strerror_t(errno));
			goto FAIL;
		}
		if (0)
		{
FAIL:
			free_worker_context(ctxs[i]);
			ctxs[i] = NULL;
			break;
		}
	}
	if ( i < worker_thread_num )
	{
		WARNING("want %d, but only %d worker thread has been started.", worker_thread_num, i);
	}
	NOTICE("guarder is working, thread num[%d].", i);
	atomic_set(&g_worker_num, i);
	for ( j = 0; j < i; ++j )
	{
		pthread_join(ctxs[j]->_tid, NULL);
		free_worker_context(ctxs[j]);
		ctxs[i] = NULL;
	}
	free(ctxs);
	NOTICE("guarder is shutdown.");
	return 0;
}
