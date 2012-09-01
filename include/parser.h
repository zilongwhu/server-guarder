/*
 * =====================================================================================
 *
 *       Filename:  parser.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2012年06月02日 11时06分59秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Zilong.Zhu (), zilong.whu@gmail.com
 *        Company:  edu.whu
 *
 * =====================================================================================
 */
#ifndef __GUARDER_PARSER_H__
#define __GUARDER_PARSER_H__

#include "structs.h"

int parse_request_header(struct worker_context *ctx, struct connection *conn, int end_pos);

int do_with_request_headers(struct connection *conn);

int parse_chunk_body(struct worker_context *ctx, struct connection *conn);

#endif
