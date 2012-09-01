/*
 * =====================================================================================
 *
 *       Filename:  structs.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2012年06月01日 18时17分04秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Zilong.Zhu (), zilong.whu@gmail.com
 *        Company:  edu.whu
 *
 * =====================================================================================
 */

#ifndef __GUARGER_STRUCTS_H__
#define __GUARGER_STRUCTS_H__

#include <stdint.h>
#include <pthread.h>
#include "mempool.h"
#include "dlist.h"
#include "exnet.h"

#define MAX_HTTP_HEADER_LEN	(4096)
#define MAX_BUFFER_LEN	(4096)

struct off_len
{
	uint16_t _offset;
	uint16_t _length;
};

struct worker_context
{
	int _wid;
	pthread_t _tid;

	int _active_conn_num;

	epex_t _epoll;

	mempool_t _conn_pool;                   /* connection pool */
	mempool_t _header_pool;
	struct
	{
		int _data_len;                  /* data buffer len */
                mempool_t _body_pool;           /* read request body buffer pool */
	};
	mempool_t _chunk_pool;                  /* read request body_chunk pool */

	char _header[MAX_HTTP_HEADER_LEN];
};

enum
{
	CONN_ERROR = -1,
	CONN_CLIENT_READING_HEADER = 0,
	CONN_CLIENT_READING_CHUNK_BODY,
	CONN_CLIENT_READING_LENGTH_BODY,
	CONN_CONNECTING_SERVER,
	CONN_SERVER_WRITING_HEADER,
	CONN_SERVER_WRITING_BODY,
	CONN_SERVER_CLOSED,
	CONN_DONE,
};

enum
{
	REQUEST_HEADER_STATE_START = 0,
	REQUEST_HEADER_STATE_METHOD,
	REQUEST_HEADER_STATE_LWS_URI,
	REQUEST_HEADER_STATE_URI,
	REQUEST_HEADER_STATE_LWS_VERSION,
	REQUEST_HEADER_STATE_VERSION,
	REQUEST_HEADER_STATE_ENDING_REQUEST_LINE,
	REQUEST_HEADER_STATE_LINE_START,
	REQUEST_HEADER_STATE_LWS_NAME,
	REQUEST_HEADER_STATE_NAME,
	REQUEST_HEADER_STATE_LWS_SEP,
	REQUEST_HEADER_STATE_SEP,
	REQUEST_HEADER_STATE_LWS_VALUE,
	REQUEST_HEADER_STATE_VALUE,
	REQUEST_HEADER_STATE_END,
};

enum
{
	REQUEST_CHUNK_BODY_STATE_LWS_SIZE = 0,
	REQUEST_CHUNK_BODY_STATE_SIZE,
	REQUEST_CHUNK_BODY_STATE_ENDING_LINE,
	REQUEST_CHUNK_BODY_STATE_ENDING_LINE_IN_STR,
	REQUEST_CHUNK_BODY_STATE_PRE_DATA,
	REQUEST_CHUNK_BODY_STATE_DATA,
	REQUEST_CHUNK_BODY_STATE_DATA_CR,
	REQUEST_CHUNK_BODY_STATE_DATA_CRLF,
	REQUEST_CHUNK_BODY_STATE_ENDING_LAST_LINE,
	REQUEST_CHUNK_BODY_STATE_TRAILER_LINE_START,
	REQUEST_CHUNK_BODY_STATE_TRAILER_LWS_NAME,
	REQUEST_CHUNK_BODY_STATE_TRAILER_NAME,
	REQUEST_CHUNK_BODY_STATE_TRAILER_LWS_SEP,
	REQUEST_CHUNK_BODY_STATE_TRAILER_SEP,
	REQUEST_CHUNK_BODY_STATE_TRAILER_LWS_VALUE,
	REQUEST_CHUNK_BODY_STATE_TRAILER_LWS_VALUE_CR,
	REQUEST_CHUNK_BODY_STATE_TRAILER_LWS_VALUE_CRLF,
	REQUEST_CHUNK_BODY_STATE_TRAILER_VALUE,
	REQUEST_CHUNK_BODY_STATE_END,
};

enum
{
	HTTP_METHOD_UNSET = 0,
	HTTP_METHOD_GET,
	HTTP_METHOD_HEAD,
	HTTP_METHOD_POST,
};

enum
{
	HTTP_VERSION_UNSET = 0,
	HTTP_VERSION_0_9,
	HTTP_VERSION_1_0,
	HTTP_VERSION_1_1,
};

struct connection
{
	uint16_t _status;
	uint16_t _errno;

	int _pos;

	struct
	{
		int _fd;
		char _ip_str[INET_ADDRSTRLEN];

		char _header[MAX_HTTP_HEADER_LEN];
		uint16_t _header_len;
		uint16_t _next_len;

		const char *_uri_start;
		const char *_uri_end;
		union
		{
			const char *_start;
			const char *_method_start;
			const char *_version_start;
			const char *_name_start;
			const char *_value_start;
		};
		union
		{
			const char *_end;
			const char *_method_end;
			const char *_version_end;
			const char *_name_end;
			const char *_value_end;
		};

		uint8_t _state;

		uint8_t _method;
		uint8_t _version;
		uint8_t _keep_alive: 1;
		uint8_t _chunked: 1;

		__dlist_t _headers;

		__dlist_t _body;
		int _body_len;

		__dlist_t *_cur_block;
		int _cur_pos;
		int _chunk_size;
		int _trailer_pos;

		__dlist_t _chunks;
	} _client;

	struct
	{
		int _fd;
	} _server;

	struct
	{
		char _buffer[MAX_BUFFER_LEN + 1];
		uint16_t _get_pos;
		uint16_t _put_pos;
	};
};

struct body_buffer
{
	__dlist_t _list;
	uint16_t _curpos;
	uint16_t _data_len;
	char _data[0];
};

struct body_chunk
{
	__dlist_t _list;
	__dlist_t *_block;
	int _curpos;
	int _size;
};

struct http_header
{
	__dlist_t _list;
	const char *_name_start;
	const char *_name_end;
	const char *_value_start;
	const char *_value_end;
};

#endif
