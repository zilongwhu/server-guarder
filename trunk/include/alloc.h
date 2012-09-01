/*
 * =====================================================================================
 *
 *       Filename:  alloc.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2012年06月01日 18时22分08秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Zilong.Zhu (), zilong.whu@gmail.com
 *        Company:  edu.whu
 *
 * =====================================================================================
 */

#ifndef __GUARGER_ALLOC_H__
#define __GUARGER_ALLOC_H__

#include "structs.h"

struct worker_context *alloc_worker_context(int buffer_len);

void free_worker_context(struct worker_context *ctx);

struct connection *alloc_connection(struct worker_context *ctx);

void free_connection(struct worker_context *ctx, struct connection *conn);

#endif
