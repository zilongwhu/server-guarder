/*
 * =====================================================================================
 *
 *       Filename:  parser.c
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2012年06月02日 11时04分35秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Zilong.Zhu (), zilong.whu@gmail.com
 *        Company:  edu.whu
 *
 * =====================================================================================
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "log.h"
#include "utils.h"
#include "uri.h"
#include "lws.h"
#include "token.h"
#include "dlist.h"
#include "mempool.h"
#include "parser.h"

int parse_request_header(struct worker_context *ctx, struct connection *conn, int end_pos)
{
	int flag;
	void *tmp;
	struct http_header *header;
	const char *start = conn->_client._header + conn->_pos;
	const char *end = conn->_client._header + end_pos;
	while (start < end)
	{
		switch (conn->_client._state)
		{
			case REQUEST_HEADER_STATE_START:
				if ( parse_lws(start, end, &start) )
				{
					conn->_client._state = REQUEST_HEADER_STATE_METHOD;
					conn->_client._method_start = start;
				}
				else
				{
					goto RETURN;
				}
				break;
			case REQUEST_HEADER_STATE_METHOD:
				if ( start + 3 <= end && strncasecmp(start, "GET", 3) == 0 )
				{
					DEBUG("Method: [GET].");
					start += 3;
					conn->_client._method = HTTP_METHOD_GET;
				}
				else if ( start + 4 <= end && strncasecmp(start, "HEAD", 4) == 0 )
				{
					DEBUG("Method: [HEAD].");
					start += 4;
					conn->_client._method = HTTP_METHOD_HEAD;
				}
				else if ( start + 4 <= end && strncasecmp(start, "POST", 4) == 0 )
				{
					DEBUG("Method: [POST].");
					start += 4;
					conn->_client._method = HTTP_METHOD_POST;
				}
				else if ( start + 4 <= end )
				{
					WARNING("invalid method[%.*s].", 4, start);
					return -1;
				}
                                else
				{
					goto RETURN;
				}
				conn->_client._state = REQUEST_HEADER_STATE_LWS_URI;
				conn->_client._method_end = start;
				break;
			case REQUEST_HEADER_STATE_LWS_URI:
				if ( parse_lws(start, end, &start) )
				{
					if ( start > conn->_client._method_end )
					{
						conn->_client._state = REQUEST_HEADER_STATE_URI;
						conn->_client._uri_start = start;
					}
					else
					{
						WARNING("invalid request format: space is needed between method and uri.");
						return -1;
					}
				}
				else
				{
					goto RETURN;
				}
				break;
			case REQUEST_HEADER_STATE_URI:
				if ( ' ' == start[0] || '\t' == start[0] || '\r' == start[0] )
				{
					conn->_client._uri_end = parse_uri_ref(conn->_client._uri_start);
					if ( conn->_client._uri_end != start )
					{
						WARNING("invalid uri[%.*s].",
								(int)(start - conn->_client._uri_start),
								conn->_client._uri_start);
						return -1;
					}
					conn->_client._state = REQUEST_HEADER_STATE_LWS_VERSION;
					DEBUG("URI: [%.*s].",
							(int)(conn->_client._uri_end - conn->_client._uri_start),
							conn->_client._uri_start);
				}
				else
				{
					++start;
				}
				break;
			case REQUEST_HEADER_STATE_LWS_VERSION:
				if ( parse_lws(start, end, &start) )
				{
					if ( start > conn->_client._uri_end )
					{
						conn->_client._state = REQUEST_HEADER_STATE_VERSION;
						conn->_client._version_start = start;
					}
					else
					{
						WARNING("invalid request format: space is needed between uri and version.");
						return -1;
					}
				}
				else
				{
					goto RETURN;
				}
				break;
			case REQUEST_HEADER_STATE_VERSION:
				if ( ' ' == start[0] || '\t' == start[0] || '\r' == start[0] )
				{
					flag = 0;
					conn->_client._version_end = start;
					if ( conn->_client._version_end - conn->_client._version_start != 8 )
					{
						flag = 1;
					}
					else if ( strncasecmp(conn->_client._version_start, "HTTP/0.9", 8) == 0 )
					{
						conn->_client._version = HTTP_VERSION_0_9;
						DEBUG("Version: [HTTP/0.9].");
					}
					else if ( strncasecmp(conn->_client._version_start, "HTTP/1.0", 8) == 0 )
					{
						conn->_client._version = HTTP_VERSION_1_0;
						DEBUG("Version: [HTTP/1.0].");
					}
					else if ( strncasecmp(conn->_client._version_start, "HTTP/1.1", 8) == 0 )
					{
						conn->_client._version = HTTP_VERSION_1_1;
						conn->_client._keep_alive = 1;
						DEBUG("Version: [HTTP/1.1].");
					}
					else
					{
						flag = 1;
					}
					if ( flag )
					{
						WARNING("invalid http version[%.*s].",
								(int)(conn->_client._version_end - conn->_client._version_start),
								conn->_client._version_start);
						return -1;
					}
					conn->_client._state = REQUEST_HEADER_STATE_ENDING_REQUEST_LINE;
				}
				else
				{
					++start;
				}
				break;
			case REQUEST_HEADER_STATE_ENDING_REQUEST_LINE:
				if ( parse_lws(start, end, &start) )
				{
					if ( '\r' == start[0] && '\n' == start[1] )
					{
						/* end of request line */
						DEBUG("Request line: [%.*s].",
								(int)(start - conn->_client._header),
								conn->_client._header);
						start += 2;
						conn->_client._state = REQUEST_HEADER_STATE_LINE_START;
					}
					else
					{
						WARNING("invalid request format: there is something after version.");
						return -1;
					}
				}
				else
				{
					goto RETURN;
				}
				break;
			case REQUEST_HEADER_STATE_LINE_START:
				if ( end - start < 2 )
				{
					goto RETURN;
				}
				if ( '\r' == start[0] && '\n' == start[1] )
				{
					start += 2;
					conn->_client._state = REQUEST_HEADER_STATE_END;
				}
				else
				{
					conn->_client._state = REQUEST_HEADER_STATE_LWS_NAME;
				}
				break;
			case REQUEST_HEADER_STATE_LWS_NAME:
				if ( parse_lws(start, end, &start) )
				{
					if ( '\r' == start[0] && '\n' == start[1] )
					{
						WARNING("empty header line.");
						return -1;
					}
					else
					{
						conn->_client._state = REQUEST_HEADER_STATE_NAME;
						conn->_client._name_start = start;
					}
				}
				else
				{
					goto RETURN;
				}
				break;
			case REQUEST_HEADER_STATE_NAME:
				if ( parse_token(start, end, &start) )
				{
					conn->_client._state = REQUEST_HEADER_STATE_LWS_SEP;
					conn->_client._name_end = start;
					if ( conn->_client._name_end - conn->_client._name_start == 0 )
					{
						WARNING("empty header name.");
						return -1;
					}
					DEBUG("name[%.*s].",
							(int)(conn->_client._name_end - conn->_client._name_start),
							conn->_client._name_start);
					header = (struct http_header *)mp_alloc(ctx->_header_pool);
					if ( NULL == header )
					{
						WARNING("failed to alloc mem for header.");
						return -1;
					}
					header->_name_start = conn->_client._name_start;
					header->_name_end = conn->_client._name_end;
					header->_value_start = NULL;
					header->_value_end = NULL;
					DLIST_INIT(&header->_list);
					DLIST_INSERT_B(&header->_list, &conn->_client._headers);
				}
				else
				{
					goto RETURN;
				}
				break;
			case REQUEST_HEADER_STATE_LWS_SEP:
				if ( parse_lws(start, end, &start) )
				{
					conn->_client._state = REQUEST_HEADER_STATE_SEP;
				}
				else
				{
					goto RETURN;
				}
				break;
			case REQUEST_HEADER_STATE_SEP:
				if ( ':' != start[0] )
				{
					WARNING("invalid request format: header name must be followed by a : sign.");
					return -1;
				}
				++start;
				conn->_client._state = REQUEST_HEADER_STATE_LWS_VALUE;
				conn->_client._value_start = NULL;
				conn->_client._value_end = NULL;
				break;
			case REQUEST_HEADER_STATE_LWS_VALUE:
				if ( parse_lws(start, end, &start) )
				{
					if ( '\r' == start[0] && '\n' == start[1] )
					{
						/* end of header line */
						start += 2;
						conn->_client._state = REQUEST_HEADER_STATE_LINE_START;
						DEBUG("value[%.*s].",
								(int)(conn->_client._value_end - conn->_client._value_start),
								conn->_client._value_start);
						tmp = DLIST_PRE(&conn->_client._headers);
						header = GET_OWNER(tmp, struct http_header, _list);
						header->_value_start = conn->_client._value_start;
						header->_value_end = conn->_client._value_end;
					}
					else if ( '\r' == start[0] )
					{
						WARNING("invalid request format: \\r is not followed by \\n.");
						return -1;
					}
					else
					{
						conn->_client._state = REQUEST_HEADER_STATE_VALUE;
						if ( NULL == conn->_client._value_start )
						{
							conn->_client._value_start = start;
						}
					}
				}
				else
				{
					goto RETURN;
				}
				break;
			case REQUEST_HEADER_STATE_VALUE:
				if ( ' ' == start[0] || '\t' == start[0] || '\r' == start[0] )
				{
					conn->_client._state = REQUEST_HEADER_STATE_LWS_VALUE;
					conn->_client._value_end = start;
				}
				else
				{
					++start;
				}
				break;
			case REQUEST_HEADER_STATE_END:
				DEBUG("end of headers.");
				goto RETURN;
				break;
		}
	}
RETURN:
	conn->_pos = start - conn->_client._header;
	return 0;
}

typedef int (*HEADER_PARSER_t)(struct connection *conn, const char *value, int len);

int parse_transfer_encoding(struct connection *conn, const char *value, int len)
{
	if ( sizeof("chunked") - 1 == len && strncasecmp(value, "chunked", len) == 0 )
	{
		conn->_client._chunked = 1;
		return 0;
	}
	else if ( sizeof("identity") - 1 == len && strncasecmp(value, "identity", len) == 0 )
	{
		conn->_client._chunked = 0;
		return 0;
	}
	if ( HTTP_VERSION_1_1 == conn->_client._version )
	{
		WARNING("invalid transfer encoding[%.*s].", len, value);
		return -1;
	}
	return 0;
}

int parse_content_length(struct connection *conn, const char *value, int len)
{
	if ( HTTP_METHOD_POST != conn->_client._method )
	{
		return 0;
	}
	char *endptr;
	long val = strtol(value, &endptr, 10);
	if ( val < 0 || (LONG_MAX == val && ERANGE == errno) || value + len != endptr )
	{
		WARNING("invalid content length[%.*s].", len, value);
		return -1;
	}
	if ( val > INT_MAX )
	{
		WARNING("too long body len[%.*s].", len, value);
		return -1;
	}
	conn->_client._body_len = val;
	return 0;
}

int parse_connection(struct connection *conn, const char *value, int len)
{
	if ( sizeof("close") - 1 == len && strncasecmp(value, "close", len) == 0 )
	{
		conn->_client._keep_alive = 0;
	}
	return 0;
}

struct
{
	const char *const _name;
	const int _name_len;
	HEADER_PARSER_t _parser_fun;
} g_headers[] = 
{
	{
		"Transfer-Encoding",
		sizeof("Transfer-Encoding") - 1,
		parse_transfer_encoding
	},
	{
		"Content-Length",
		sizeof("Content-Length") - 1,
		parse_content_length
	},
	{
		"Connection",
		sizeof("Connection") - 1,
		parse_connection
	},
};

int do_with_request_headers(struct connection *conn)
{
	int i;
	int ret;
	struct http_header *header;
	__dlist_t *cur = DLIST_NEXT(&conn->_client._headers);;
	while (cur != &conn->_client._headers)
	{
		header = GET_OWNER(cur, struct http_header, _list);
		DEBUG("[%.*s : %.*s]",
				(int)(header->_name_end - header->_name_start),
				header->_name_start,
				(int)(header->_value_end - header->_value_start),
				header->_value_start);
		for ( i = 0; i < sizeof(g_headers)/sizeof(g_headers[0]); ++i )
		{
			if ( g_headers[i]._name_len == header->_name_end - header->_name_start )
			{
				if ( strncasecmp(header->_name_start, g_headers[i]._name, g_headers[i]._name_len) == 0 )
				{
					ret = g_headers[i]._parser_fun(conn, header->_value_start, header->_value_end - header->_value_start);
					if ( ret < 0 )
					{
						WARNING("failed to process header[%.*s][%.*s].",
								g_headers[i]._name_len,
								g_headers[i]._name,
								(int)(header->_value_end - header->_value_start),
								header->_value_start);
						return -1;
					}
				}
			}
		}
		cur = DLIST_NEXT(cur);
	}
	return 0;
}

enum
{
	PARSE_LWS_NEXT = 0,
	PARSE_LWS_CONTINUE,
	PARSE_LWS_NEED_MORE,
	PARSE_LWS_CRLF,
	PARSE_LWS_CR,
};

int parse_lws_wrapper(struct connection *conn, int next_state)
{
	struct body_buffer *buffer = GET_OWNER(conn->_client._cur_block, struct body_buffer, _list);

	const char *pos;
	const char *start = buffer->_data + conn->_client._cur_pos;
	const char *end = buffer->_data + buffer->_curpos;

	int ret = parse_lws(start, end, &pos);
	conn->_client._cur_pos += pos - start;
	if ( ret )
	{
		if ( '\r' == pos[0] && '\n' == pos[1] ) /* \r\n... */
		{
			return PARSE_LWS_CRLF;
		}
		else if ( '\r' == pos[0] )      /* \r... */
		{
			return PARSE_LWS_CR;
		}
		else
		{
			conn->_client._state = next_state;
			return PARSE_LWS_NEXT;
		}
	}
	else if ( DLIST_NEXT(conn->_client._cur_block) != &conn->_client._body )
	{
		__dlist_t *list = DLIST_NEXT(conn->_client._cur_block);
		struct body_buffer *next = GET_OWNER(list, struct body_buffer, _list);
		if ( '\r' == *pos ) /* may be \r or \r\n */
		{
			if ( pos + 2 == end ) /* \r\n */
			{
				if ( ' ' == next->_data[0] || '\t' == next->_data[0] )
				{
					conn->_client._cur_block = list;
					conn->_client._cur_pos = 1;
					return PARSE_LWS_CONTINUE;
				}
				else /* \r\n... */
				{
					return PARSE_LWS_CRLF;
				}
			}
			else if ( next->_curpos >= 2 ) /* \r */
			{
				if ( '\n' == next->_data[0] )
				{
					if ( ' ' == next->_data[1] || '\t' == next->_data[1] )
					{
						conn->_client._cur_block = list;
						conn->_client._cur_pos = 2;
						return PARSE_LWS_CONTINUE;
					}
					else    /* \r\n... */
					{
						return PARSE_LWS_CRLF;
					}
				}
				else /* \r... */
				{
					return PARSE_LWS_CR;
				}
			}
			else /* \r */
			{
				return PARSE_LWS_NEED_MORE;
			}
		}
		else    /* must be ' ' or '\t' */
		{
			conn->_client._cur_block = list;
			conn->_client._cur_pos = 0;
			return PARSE_LWS_CONTINUE;
		}
	}
	else
	{
		return PARSE_LWS_NEED_MORE;
	}
}

int append_trailer(struct connection *conn, const char *start, int len)
{
	if ( len <= 0 )
	{
		return 0;
	}
	if ( len + conn->_client._trailer_pos > MAX_HTTP_HEADER_LEN )
	{
		WARNING("too long header len.");
		return -1;
	}
	memcpy(conn->_client._header + conn->_client._trailer_pos, start, len);
	conn->_client._trailer_pos += len;
	return 0;
}

int parse_chunk_body(struct worker_context *ctx, struct connection *conn)
{
	if ( DLIST_EMPTY(&conn->_client._body) )
	{
		return 0;
	}
	if ( NULL == conn->_client._cur_block )
	{
		conn->_client._cur_block = DLIST_NEXT(&conn->_client._body);
		conn->_client._cur_pos = 0;
	}
	int ret;
	int flag;
	int avail;
	int val;
	uint64_t lval;
	const char *start;
	const char *end;
	const char *pos;
	struct body_buffer *buffer;
	struct body_buffer *next;
	struct body_chunk *chunk;
	struct http_header *header;
	__dlist_t *list;
	while (1)
	{
		buffer = GET_OWNER(conn->_client._cur_block, struct body_buffer, _list);
		if ( buffer->_curpos == conn->_client._cur_pos ) /* current buffer is consumed */
		{
			if ( DLIST_NEXT(conn->_client._cur_block) != &conn->_client._body )
			{
				/* has data, update to next buffer */
				conn->_client._cur_block = DLIST_NEXT(conn->_client._cur_block);
				conn->_client._cur_pos = 0;
				continue;
			}
			else
			{
				/* need more data */
				goto RETURN;
			}
		}
		start = buffer->_data + conn->_client._cur_pos;
		end = buffer->_data + buffer->_curpos;
		switch (conn->_client._state)
		{
			case REQUEST_CHUNK_BODY_STATE_LWS_SIZE:
				ret = parse_lws_wrapper(conn, REQUEST_CHUNK_BODY_STATE_SIZE);
				if ( PARSE_LWS_NEXT == ret ) /* state changed */
				{
					conn->_client._chunk_size = -1;
					DEBUG("start to parse chunk size.");
				}
				else if ( PARSE_LWS_NEED_MORE == ret )
				{
					goto RETURN;
				}
				else if ( PARSE_LWS_CR == ret )
				{
					WARNING("invalid request format: \\r is not followed by \\n.");
					return -1;
				}
				else if ( PARSE_LWS_CRLF == ret )
				{
					WARNING("empty chunk size line.");
					return -1;
				}
				break;
			case REQUEST_CHUNK_BODY_STATE_SIZE:
				for ( pos = start; pos < end; ++pos )
				{
					if ( '0' <= *pos && *pos <= '9' )
					{
						val = *pos - '0';
					}
					else if ( 'a' <= *pos && *pos <= 'f' )
					{
						val = *pos - 'a' + 10;
					}
					else if ( 'A' <= *pos && *pos <= 'F' )
					{
						val = *pos - 'A' + 10;
					}
					else
					{
						break;
					}
					if ( conn->_client._chunk_size < 0 )
					{
						conn->_client._chunk_size = 0;
					}
					lval = conn->_client._chunk_size;
					lval <<= 4;
					lval += val;
					if ( lval > INT_MAX )
					{
						WARNING("too long chunk size > INT_MAX.");
						return -1;
					}
					conn->_client._chunk_size = lval;
				}
				conn->_client._cur_pos += pos - start;
				if ( pos < end )
				{
					if ( conn->_client._chunk_size < 0 )
					{
						WARNING("cannot get chunk size.");
						return -1;
					}
					DEBUG("current chunk size: %d.", conn->_client._chunk_size);
					lval = ((uint64_t)conn->_client._body_len) + conn->_client._chunk_size;
					if ( lval > INT_MAX )
					{
						WARNING("too long body len > INT_MAX.");
						return -1;
					}
					conn->_client._body_len += conn->_client._chunk_size;
					conn->_client._state = REQUEST_CHUNK_BODY_STATE_ENDING_LINE;
				}
				break;
			case REQUEST_CHUNK_BODY_STATE_ENDING_LINE:
				flag = 0;
				for ( pos = start; pos < end; )
				{
					if ( '"' == *pos )
					{
						++pos;
						conn->_client._state = REQUEST_CHUNK_BODY_STATE_ENDING_LINE_IN_STR;
						break;
					}
					else if ( '\r' == *pos )
					{
						if ( pos + 1 < end )
						{
							if ( '\n' == pos[1] ) /* \r\n */
							{
								pos += 2;
								conn->_client._state = REQUEST_CHUNK_BODY_STATE_PRE_DATA;
								break;
							}
							else
							{
								WARNING("invalid request format: \\r is not followed by \\n.");
								return -1;
							}
						}
						else if ( DLIST_NEXT(conn->_client._cur_block) != &conn->_client._body )
						{
							list = DLIST_NEXT(conn->_client._cur_block);
							next = GET_OWNER(list, struct body_buffer, _list);
							if ( '\n' == next->_data[0] ) /* \r\n */
							{
								conn->_client._cur_block = list;
								conn->_client._cur_pos = 1;
								conn->_client._state = REQUEST_CHUNK_BODY_STATE_PRE_DATA;
								flag = 1;
								break;
							}
							else
							{
								WARNING("invalid request format: \\r is not followed by \\n.");
								return -1;
							}
						}
						else
						{
							conn->_client._cur_pos += pos - start;
							goto RETURN;
						}
					}
					else
					{
						++pos;
					}
				}
				if ( 0 == flag )
				{
					conn->_client._cur_pos += pos - start;
				}
				break;
			case REQUEST_CHUNK_BODY_STATE_ENDING_LINE_IN_STR:
				flag = 0;
				for ( pos = start; pos < end; )
				{
					if ( '\\' == *pos ) /* quoted pair */
					{
						if ( pos + 1 < end )
						{
							pos += 2;
						}
						else if ( DLIST_NEXT(conn->_client._cur_block) != &conn->_client._body )
						{
							conn->_client._cur_block = DLIST_NEXT(conn->_client._cur_block);
							conn->_client._cur_pos = 1;

							flag = 1;
							break;
						}
						else
						{
							conn->_client._cur_pos += pos - start;
							goto RETURN;
						}
					}
					else if ( '"' == *pos )
					{
						++pos;
						conn->_client._state = REQUEST_CHUNK_BODY_STATE_ENDING_LINE;
						break;
					}
					else
					{
						++pos;
					}
				}
				if ( 0 == flag )
				{
					conn->_client._cur_pos += pos - start;
				}
				break;
			case REQUEST_CHUNK_BODY_STATE_PRE_DATA:
				if ( 0 == conn->_client._chunk_size )
				{
					conn->_client._trailer_pos = conn->_client._header_len;
					conn->_client._state = REQUEST_CHUNK_BODY_STATE_TRAILER_LINE_START;
					DEBUG("last chunk is meet, start to parse trailers.");
					break;
				}
				chunk = mp_alloc(ctx->_chunk_pool);
				if ( NULL == chunk )
				{
					WARNING("failed to alloc body chunk.");
					return -1;
				}
				DLIST_INIT(&chunk->_list);
				chunk->_block = conn->_client._cur_block;
				chunk->_curpos = conn->_client._cur_pos;
				chunk->_size = conn->_client._chunk_size;
				DLIST_INSERT_B(&chunk->_list, &conn->_client._chunks);
				conn->_client._state = REQUEST_CHUNK_BODY_STATE_DATA;
				/* fall through */
			case REQUEST_CHUNK_BODY_STATE_DATA:
				avail = end - start;
				if ( conn->_client._chunk_size > 0 )
				{
					if ( avail > conn->_client._chunk_size )
					{
						avail = conn->_client._chunk_size;
					}
					conn->_client._cur_pos += avail;
					conn->_client._chunk_size -= avail;
				}
				if ( 0 == conn->_client._chunk_size )
				{
					conn->_client._state = REQUEST_CHUNK_BODY_STATE_DATA_CR;
				}
				break;
			case REQUEST_CHUNK_BODY_STATE_DATA_CR:
				if ( '\r' == *start )
				{
					++conn->_client._cur_pos;
					conn->_client._state = REQUEST_CHUNK_BODY_STATE_DATA_CRLF;
				}
				else
				{
					WARNING("chunk data must be flowed by \\r\\n.");
					return -1;
				}
				break;
			case REQUEST_CHUNK_BODY_STATE_DATA_CRLF:
				if ( '\n' == *start )
				{
					++conn->_client._cur_pos;
					conn->_client._state = REQUEST_CHUNK_BODY_STATE_LWS_SIZE;
					DEBUG("current chunk data has been recieved.");
				}
				else
				{
					WARNING("chunk data must be flowed by \\r\\n.");
					return -1;
				}
				break;
			case REQUEST_CHUNK_BODY_STATE_TRAILER_LINE_START:
				if ( start + 1 < end )
				{
					if ( '\r' == start[0] && '\n' == start[1] )
					{
						conn->_client._cur_pos += 2;
						conn->_client._state = REQUEST_CHUNK_BODY_STATE_END;
						DEBUG("meet end of chunks.");
					}
					else
					{
						conn->_client._state = REQUEST_CHUNK_BODY_STATE_TRAILER_LWS_NAME;
					}
				}
				else if ( DLIST_NEXT(conn->_client._cur_block) != &conn->_client._body )
				{
					list = DLIST_NEXT(conn->_client._cur_block);
					next = GET_OWNER(list, struct body_buffer, _list);

					if ( '\r' == start[0] && '\n' == next->_data[0] )
					{
						conn->_client._cur_block = list;
						conn->_client._cur_pos = 1;
						conn->_client._state = REQUEST_CHUNK_BODY_STATE_END;
						DEBUG("meet end of chunks.");
					}
					else
					{
						conn->_client._state = REQUEST_CHUNK_BODY_STATE_TRAILER_LWS_NAME;
					}
				}
				else
				{
					goto RETURN;
				}
				break;
			case REQUEST_CHUNK_BODY_STATE_TRAILER_LWS_NAME:
				ret = parse_lws_wrapper(conn, REQUEST_CHUNK_BODY_STATE_TRAILER_NAME);
				if ( PARSE_LWS_NEXT == ret )
				{
					DEBUG("start to parse trailer name.");
					conn->_client._name_start = conn->_client._header + conn->_client._trailer_pos;
				}
				else if ( PARSE_LWS_NEED_MORE == ret )
				{
					goto RETURN;
				}
				else if ( PARSE_LWS_CRLF == ret ) /* \r\n... */
				{
					WARNING("empty trailer line.");
					return -1;
				}
				else if ( PARSE_LWS_CR == ret ) /* \r... */
				{
					WARNING("invalid request format: \\r is not followed by \\n.");
					return -1;
				}
				break;
			case REQUEST_CHUNK_BODY_STATE_TRAILER_NAME:
				ret = parse_token(start, end, &pos);
				conn->_client._cur_pos += pos - start;
				if ( append_trailer(conn, start, pos - start) < 0 )
				{
					WARNING("failed to append[%.*s] to trailer.", (int)(pos - start), start);
					return -1;
				}
				if ( ret )
				{
					conn->_client._name_end = conn->_client._header + conn->_client._trailer_pos;
					if ( conn->_client._name_end - conn->_client._name_start == 0 )
					{
						WARNING("empty trailer name.");
						return -1;
					}
					DEBUG("end to parse trailer name[%.*s].",
							(int)(conn->_client._name_end - conn->_client._name_start),
							conn->_client._name_start);
					if ( (int)(conn->_client._name_end - conn->_client._name_start) == sizeof("Transfer-Encoding") - 1
							&& strncasecmp(conn->_client._name_start, "Transfer-Encoding",
								sizeof("Transfer-Encoding") - 1) == 0 )
					{
						WARNING("trailer name is Transfer-Encoding.");
						return -1;
					}
					else if ( (int)(conn->_client._name_end - conn->_client._name_start) == sizeof("Content-Length") - 1
							&& strncasecmp(conn->_client._name_start, "Content-Length",
								sizeof("Content-Length") - 1) == 0 )
					{
						WARNING("trailer name is Content-Length.");
						return -1;
					}
					else if ( (int)(conn->_client._name_end - conn->_client._name_start) == sizeof("Trailer") - 1
							&& strncasecmp(conn->_client._name_start, "Trailer",
								sizeof("Trailer") - 1) == 0 )
					{
						WARNING("trailer name is Trailer.");
						return -1;
					}
					header = (struct http_header *)mp_alloc(ctx->_header_pool);
					if ( NULL == header )
					{
						WARNING("failed to alloc mem for header.");
						return -1;
					}
					header->_name_start = conn->_client._name_start;
					header->_name_end = conn->_client._name_end;
					header->_value_start = NULL;
					header->_value_end = NULL;
					DLIST_INIT(&header->_list);
					DLIST_INSERT_B(&header->_list, &conn->_client._headers);

					conn->_client._state = REQUEST_CHUNK_BODY_STATE_TRAILER_LWS_SEP;
				}
				else if ( DLIST_NEXT(conn->_client._cur_block) != &conn->_client._body )
				{
					conn->_client._cur_block = DLIST_NEXT(conn->_client._cur_block);
					conn->_client._cur_pos = 0;
				}
				else
				{
					goto RETURN;
				}
				break;
			case REQUEST_CHUNK_BODY_STATE_TRAILER_LWS_SEP:
				ret = parse_lws_wrapper(conn, REQUEST_CHUNK_BODY_STATE_TRAILER_SEP);
				if ( PARSE_LWS_NEED_MORE == ret )
				{
					goto RETURN;
				}
				else if ( PARSE_LWS_CRLF == ret /* \r\n... */
						|| PARSE_LWS_CR == ret ) /* \r... */
				{
					conn->_client._state = REQUEST_CHUNK_BODY_STATE_TRAILER_SEP;
				}
				break;
			case REQUEST_CHUNK_BODY_STATE_TRAILER_SEP:
				if ( ':' != start[0] )
				{
					WARNING("invalid request format: header name must be followed by a : sign.");
					return -1;
				}
				++conn->_client._cur_pos;
				conn->_client._state = REQUEST_CHUNK_BODY_STATE_TRAILER_LWS_VALUE;
				conn->_client._value_start = NULL;
				conn->_client._value_end = NULL;
				DEBUG("start to parse trailer value.");
				break;
			case REQUEST_CHUNK_BODY_STATE_TRAILER_LWS_VALUE:
				ret = parse_lws_wrapper(conn, REQUEST_CHUNK_BODY_STATE_TRAILER_VALUE);
				if ( PARSE_LWS_NEXT == ret )
				{
					if ( NULL == conn->_client._value_start )
					{
						conn->_client._value_start = conn->_client._header + conn->_client._trailer_pos;
					}
					else if ( append_trailer(conn, " ", 1) < 0 )
					{
						WARNING("failed to append[ ] to trailer.");
						return -1;
					}
				}
				else if ( PARSE_LWS_NEED_MORE == ret )
				{
					goto RETURN;
				}
				else if ( PARSE_LWS_CRLF == ret ) /* \r\n... , end of trailer line */
				{
					DEBUG("end to parse trailer value[%.*s].",
							(int)(conn->_client._value_end - conn->_client._value_start),
							conn->_client._value_start);
					list = DLIST_PRE(&conn->_client._headers);
					header = GET_OWNER(list, struct http_header, _list);
					header->_value_start = conn->_client._value_start;
					header->_value_end = conn->_client._value_end;
					conn->_client._state = REQUEST_CHUNK_BODY_STATE_TRAILER_LWS_VALUE_CR;
				}
				else if ( PARSE_LWS_CR == ret ) /* \r... */
				{
					WARNING("invalid request format: \\r is not followed by \\n.");
					return -1;
				}
				break;
			case REQUEST_CHUNK_BODY_STATE_TRAILER_LWS_VALUE_CR:
				++conn->_client._cur_pos;
				conn->_client._state = REQUEST_CHUNK_BODY_STATE_TRAILER_LWS_VALUE_CRLF;
				break;
			case REQUEST_CHUNK_BODY_STATE_TRAILER_LWS_VALUE_CRLF:
				++conn->_client._cur_pos;
				conn->_client._state = REQUEST_CHUNK_BODY_STATE_TRAILER_LINE_START;
				break;
			case REQUEST_CHUNK_BODY_STATE_TRAILER_VALUE:
				for ( pos = start; pos < end; )
				{
					if ( ' ' == pos[0] || '\t' == pos[0] || '\r' == pos[0] )
					{
						break;
					}
					else
					{
						++pos;
					}
				}
				conn->_client._cur_pos += pos - start;
				if ( append_trailer(conn, start, pos - start) < 0 )
				{
					WARNING("failed to append[%.*s] to trailer.", (int)(pos - start), start);
					return -1;
				}
				if ( pos < end )
				{
					conn->_client._value_end = conn->_client._header + conn->_client._trailer_pos;
					conn->_client._state = REQUEST_CHUNK_BODY_STATE_TRAILER_LWS_VALUE;
				}
				break;
			case REQUEST_CHUNK_BODY_STATE_END:
				goto RETURN;
				break;
		}
	}
RETURN:
	return 0;
}
