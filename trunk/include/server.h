/*
 * =====================================================================================
 *
 *       Filename:  server.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2012年06月07日 20时15分08秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Zilong.Zhu (), zilong.whu@gmail.com
 *        Company:  edu.whu
 *
 * =====================================================================================
 */

#ifndef __GUARGER_SERVER_H__
#define __GUARGER_SERVER_H__

#include <netinet/in.h>

#define MAX_SERVER_NAME_LEN	(256)

struct server
{
	const char _name[MAX_SERVER_NAME_LEN];
	struct sockaddr_in _addr;

	time_t _fail_time;
	struct
	{
		uint32_t _total_err_num;

		uint32_t _connect_err_num;
		uint32_t _read_err_num;
		uint32_t _write_err_num;
	};
};

#endif
