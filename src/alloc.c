/*
 * =====================================================================================
 *
 *       Filename:  alloc.c
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2012年06月01日 18时18分29秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Zilong.Zhu (), zilong.whu@gmail.com
 *        Company:  edu.whu
 *
 * =====================================================================================
 */

#include <stdlib.h>
#include "log.h"
#include "utils.h"
#include "alloc.h"

struct worker_context *alloc_worker_context(int buffer_len)
{
	if ( buffer_len <= 0 || buffer_len > UINT16_MAX )
	{
		WARNING("invalid arg: buffer len[%d].", buffer_len);
		return NULL;
	}
	struct worker_context *res = (struct worker_context *)calloc(1, sizeof(struct worker_context));
	if ( NULL == res )
	{
		WARNING("failed to alloc worker_context.");
		return NULL;
	}
	res->_active_conn_num = 0;
	res->_epoll = epex_open(1024);
	if ( NULL == res->_epoll )
	{
		WARNING("failed to init epex.");
		goto FAIL;
	}
	res->_data_len = buffer_len;
	res->_conn_pool = mp_init(sizeof(struct connection));
	if ( NULL == res->_conn_pool )
	{
		WARNING("failed to init connection pool.");
		goto FAIL;
	}
	res->_header_pool = mp_init(sizeof(struct http_header));
	if ( NULL == res->_header_pool )
	{
		WARNING("failed to init header pool.");
		goto FAIL;
	}
	res->_body_pool = mp_init(sizeof(struct body_buffer) + buffer_len);
	if ( NULL == res->_body_pool )
	{
		WARNING("failed to init buffer pool.");
		goto FAIL;
	}
	res->_chunk_pool = mp_init(sizeof(struct body_chunk));
	if ( NULL == res->_chunk_pool )
	{
		WARNING("failed to init chunk pool.");
		goto FAIL;
	}
	return res;
FAIL:
	free_worker_context(res);
	return NULL;
}

void free_worker_context(struct worker_context *ctx)
{
	if ( NULL == ctx )
	{
		return ;
	}
	if ( NULL != ctx->_epoll )
	{
		epex_close(ctx->_epoll);
		ctx->_epoll = NULL;
	}
	if ( NULL != ctx->_conn_pool )
	{
		mp_close(ctx->_conn_pool);
		ctx->_conn_pool = NULL;
	}
	if ( NULL != ctx->_header_pool )
	{
		mp_close(ctx->_header_pool);
		ctx->_header_pool = NULL;
	}
	if ( NULL != ctx->_body_pool )
	{
		mp_close(ctx->_body_pool);
		ctx->_body_pool = NULL;
	}
	if ( NULL != ctx->_chunk_pool )
	{
		mp_close(ctx->_chunk_pool);
		ctx->_chunk_pool = NULL;
	}
	free(ctx);
}

struct connection *alloc_connection(struct worker_context *ctx)
{
	if ( NULL == ctx )
	{
		WARNING("invalid arg, ctx is NULL.");
		return NULL;
	}
	struct connection *conn = (struct connection *)mp_alloc(ctx->_conn_pool);
	if ( NULL == conn )
	{
		WARNING("failed to alloc conn.");
		return NULL;
	}
	conn->_status = CONN_CLIENT_READING_HEADER;
	conn->_errno = 0;
	conn->_pos = 0;
	conn->_client._fd = -1;
	conn->_client._ip_str[0] = '\0';
	conn->_client._header_len = 0;
	conn->_client._next_len = 0;
	conn->_client._uri_start = NULL;
	conn->_client._uri_end = NULL;
	conn->_client._start = NULL;
	conn->_client._end = NULL;
	conn->_client._state = REQUEST_HEADER_STATE_START;
	conn->_client._method = HTTP_METHOD_UNSET;
	conn->_client._version = HTTP_VERSION_UNSET;
	conn->_client._keep_alive = 0;
	conn->_client._chunked = 0;
	DLIST_INIT(&conn->_client._headers);
	DLIST_INIT(&conn->_client._body);
	conn->_client._body_len = 0;
	conn->_client._cur_block = NULL;
	conn->_client._cur_pos = 0;
	conn->_client._chunk_size = 0;
	conn->_client._trailer_pos = 0;
	DLIST_INIT(&conn->_client._chunks);
	conn->_server._fd = -1;
	conn->_get_pos = 0;
	conn->_put_pos = 0;
	return conn;
}

void free_connection(struct worker_context *ctx, struct connection *conn)
{
	if ( NULL == ctx || NULL == conn )
	{
		WARNING("invalid args: worker context[%p], connection[%p].", ctx, conn);
		return ;
	}
	__dlist_t *cur;
	struct http_header *header;
	while ( !DLIST_EMPTY(&conn->_client._headers) )
	{
		cur = DLIST_NEXT(&conn->_client._headers);
		DLIST_REMOVE(cur);
		header = GET_OWNER(cur, struct http_header, _list);
		mp_free(ctx->_header_pool, header);
	}
	struct body_buffer *buf;
	while ( !DLIST_EMPTY(&conn->_client._body) )
	{
		cur = DLIST_NEXT(&conn->_client._body);
		DLIST_REMOVE(cur);
		buf = GET_OWNER(cur, struct body_buffer, _list);
		mp_free(ctx->_body_pool, buf);
	}
	struct body_chunk *chunk;
	while ( !DLIST_EMPTY(&conn->_client._chunks) )
	{
		cur = DLIST_NEXT(&conn->_client._chunks);
		DLIST_REMOVE(cur);
		chunk = GET_OWNER(cur, struct body_chunk, _list);
		mp_free(ctx->_chunk_pool, chunk);
	}
	mp_free(ctx->_conn_pool, conn);
}
