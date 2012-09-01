/*
 * =====================================================================================
 *
 *       Filename:  token.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2012年06月01日 12时36分38秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Zilong.Zhu (), zilong.whu@gmail.com
 *        Company:  edu.whu
 *
 * =====================================================================================
 */
#ifndef __GUARGER_TOKEN_H__
#define __GUARGER_TOKEN_H__

#include <stdint.h>

extern const uint8_t s_token_arr[256];

#define IS_TOKEN_CHAR(ch)	(s_token_arr[(unsigned char)(ch)])

int parse_token(const char *start, const char *end, const char **pos);

#endif
