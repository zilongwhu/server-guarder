/*
 * =====================================================================================
 *
 *       Filename:  helper.c
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2012年06月01日 12时25分06秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Zilong.Zhu (), zilong.whu@gmail.com
 *        Company:  edu.whu
 *
 * =====================================================================================
 */

#include <stdio.h>
#include <ctype.h>

static void print_metrix(void)
{
	int i;
	int v;
	printf("\n{\n");
	for ( i = 0; i < 256; ++i )
	{
		v = 0;
		if ( i >= 0 && i <= 31 || 127 == i )
		{

		}
		else if ( '(' == i || ')' == i || '<' == i || '>' == i || '@' == i
				|| ',' == i || ';' == i || ':' == i || '\\' == i || '"' == i
				|| '/' == i || '[' == i || ']' == i || '?' == i || '=' == i
				|| '{' == i || '}' == i || ' ' == i || '\t' == i )
		{

		}
		else
		{
			v = 1;
		}
		if ( isprint(i) )
			printf("\t/* %3c */ 0x%02X,", (char)i, v);
		else
			printf("\t/* %3d */ 0x%02X,", i, v);
		if ( i % 8 == 7 )
			printf("\n");
	}
	printf("}\n");
}

int main(int argc, char *argv[])
{
	print_metrix();
	return 0;
}
