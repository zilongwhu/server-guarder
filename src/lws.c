/*
 * =====================================================================================
 *
 *       Filename:  lws.c
 *
 *    Description:  lws parser
 *
 *        Version:  1.0
 *        Created:  2012年06月01日 12时39分29秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Zilong.Zhu (), zilong.whu@gmail.com
 *        Company:  edu.whu
 *
 * =====================================================================================
 */
#include "lws.h"

int parse_lws(const char *start, const char *end, const char **pos)
{
	while ( start < end )
	{
		if ( '\r' == start[0] )
		{
			/* meet \r */
			if ( start + 1 < end )
			{
				if ( '\n' == start[1] )
				{
					/* meet \r\n */
					if ( start + 2 < end )
					{
						if ( ' ' == start[2] || '\t' == start[2] )
						{
							/* meet \r\n SP */
							start += 3;
						}
						else
						{
							break;
						}
					}
					else
					{
						goto NEED_MORE_DATA;
					}
				}
				else
				{
					/* meet \r... */
					break;
				}
			}
			else
			{
				goto NEED_MORE_DATA;
			}
		}
		else if ( ' ' == start[0] || '\t' == start[0] )
		{
			++start;
		}
		else
		{
			break;
		}
	}
	if ( start < end )
	{
		*pos = start;
		return 1;
	}
NEED_MORE_DATA:
	*pos = start;
	return 0;
}
