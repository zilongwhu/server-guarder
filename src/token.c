/*
 * =====================================================================================
 *
 *       Filename:  token.c
 *
 *    Description:  token helper
 *
 *        Version:  1.0
 *        Created:  2012年06月01日 12时33分02秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Zilong.Zhu (), zilong.whu@gmail.com
 *        Company:  edu.whu
 *
 * =====================================================================================
 */
#include "token.h"

const uint8_t s_token_arr[256] = 
{
	/*   0 */ 0x00,	/*   1 */ 0x00,	/*   2 */ 0x00,	/*   3 */ 0x00,	/*   4 */ 0x00,	/*   5 */ 0x00,	/*   6 */ 0x00,	/*   7 */ 0x00,
	/*   8 */ 0x00,	/*   9 */ 0x00,	/*  10 */ 0x00,	/*  11 */ 0x00,	/*  12 */ 0x00,	/*  13 */ 0x00,	/*  14 */ 0x00,	/*  15 */ 0x00,
	/*  16 */ 0x00,	/*  17 */ 0x00,	/*  18 */ 0x00,	/*  19 */ 0x00,	/*  20 */ 0x00,	/*  21 */ 0x00,	/*  22 */ 0x00,	/*  23 */ 0x00,
	/*  24 */ 0x00,	/*  25 */ 0x00,	/*  26 */ 0x00,	/*  27 */ 0x00,	/*  28 */ 0x00,	/*  29 */ 0x00,	/*  30 */ 0x00,	/*  31 */ 0x00,
	/*     */ 0x00,	/*   ! */ 0x01,	/*   " */ 0x00,	/*   # */ 0x01,	/*   $ */ 0x01,	/*   % */ 0x01,	/*   & */ 0x01,	/*   ' */ 0x01,
	/*   ( */ 0x00,	/*   ) */ 0x00,	/*   * */ 0x01,	/*   + */ 0x01,	/*   , */ 0x00,	/*   - */ 0x01,	/*   . */ 0x01,	/*   / */ 0x00,
	/*   0 */ 0x01,	/*   1 */ 0x01,	/*   2 */ 0x01,	/*   3 */ 0x01,	/*   4 */ 0x01,	/*   5 */ 0x01,	/*   6 */ 0x01,	/*   7 */ 0x01,
	/*   8 */ 0x01,	/*   9 */ 0x01,	/*   : */ 0x00,	/*   ; */ 0x00,	/*   < */ 0x00,	/*   = */ 0x00,	/*   > */ 0x00,	/*   ? */ 0x00,
	/*   @ */ 0x00,	/*   A */ 0x01,	/*   B */ 0x01,	/*   C */ 0x01,	/*   D */ 0x01,	/*   E */ 0x01,	/*   F */ 0x01,	/*   G */ 0x01,
	/*   H */ 0x01,	/*   I */ 0x01,	/*   J */ 0x01,	/*   K */ 0x01,	/*   L */ 0x01,	/*   M */ 0x01,	/*   N */ 0x01,	/*   O */ 0x01,
	/*   P */ 0x01,	/*   Q */ 0x01,	/*   R */ 0x01,	/*   S */ 0x01,	/*   T */ 0x01,	/*   U */ 0x01,	/*   V */ 0x01,	/*   W */ 0x01,
	/*   X */ 0x01,	/*   Y */ 0x01,	/*   Z */ 0x01,	/*   [ */ 0x00,	/*   \ */ 0x00,	/*   ] */ 0x00,	/*   ^ */ 0x01,	/*   _ */ 0x01,
	/*   ` */ 0x01,	/*   a */ 0x01,	/*   b */ 0x01,	/*   c */ 0x01,	/*   d */ 0x01,	/*   e */ 0x01,	/*   f */ 0x01,	/*   g */ 0x01,
	/*   h */ 0x01,	/*   i */ 0x01,	/*   j */ 0x01,	/*   k */ 0x01,	/*   l */ 0x01,	/*   m */ 0x01,	/*   n */ 0x01,	/*   o */ 0x01,
	/*   p */ 0x01,	/*   q */ 0x01,	/*   r */ 0x01,	/*   s */ 0x01,	/*   t */ 0x01,	/*   u */ 0x01,	/*   v */ 0x01,	/*   w */ 0x01,
	/*   x */ 0x01,	/*   y */ 0x01,	/*   z */ 0x01,	/*   { */ 0x00,	/*   | */ 0x01,	/*   } */ 0x00,	/*   ~ */ 0x01,	/* 127 */ 0x00,
	/* 128 */ 0x01,	/* 129 */ 0x01,	/* 130 */ 0x01,	/* 131 */ 0x01,	/* 132 */ 0x01,	/* 133 */ 0x01,	/* 134 */ 0x01,	/* 135 */ 0x01,
	/* 136 */ 0x01,	/* 137 */ 0x01,	/* 138 */ 0x01,	/* 139 */ 0x01,	/* 140 */ 0x01,	/* 141 */ 0x01,	/* 142 */ 0x01,	/* 143 */ 0x01,
	/* 144 */ 0x01,	/* 145 */ 0x01,	/* 146 */ 0x01,	/* 147 */ 0x01,	/* 148 */ 0x01,	/* 149 */ 0x01,	/* 150 */ 0x01,	/* 151 */ 0x01,
	/* 152 */ 0x01,	/* 153 */ 0x01,	/* 154 */ 0x01,	/* 155 */ 0x01,	/* 156 */ 0x01,	/* 157 */ 0x01,	/* 158 */ 0x01,	/* 159 */ 0x01,
	/* 160 */ 0x01,	/* 161 */ 0x01,	/* 162 */ 0x01,	/* 163 */ 0x01,	/* 164 */ 0x01,	/* 165 */ 0x01,	/* 166 */ 0x01,	/* 167 */ 0x01,
	/* 168 */ 0x01,	/* 169 */ 0x01,	/* 170 */ 0x01,	/* 171 */ 0x01,	/* 172 */ 0x01,	/* 173 */ 0x01,	/* 174 */ 0x01,	/* 175 */ 0x01,
	/* 176 */ 0x01,	/* 177 */ 0x01,	/* 178 */ 0x01,	/* 179 */ 0x01,	/* 180 */ 0x01,	/* 181 */ 0x01,	/* 182 */ 0x01,	/* 183 */ 0x01,
	/* 184 */ 0x01,	/* 185 */ 0x01,	/* 186 */ 0x01,	/* 187 */ 0x01,	/* 188 */ 0x01,	/* 189 */ 0x01,	/* 190 */ 0x01,	/* 191 */ 0x01,
	/* 192 */ 0x01,	/* 193 */ 0x01,	/* 194 */ 0x01,	/* 195 */ 0x01,	/* 196 */ 0x01,	/* 197 */ 0x01,	/* 198 */ 0x01,	/* 199 */ 0x01,
	/* 200 */ 0x01,	/* 201 */ 0x01,	/* 202 */ 0x01,	/* 203 */ 0x01,	/* 204 */ 0x01,	/* 205 */ 0x01,	/* 206 */ 0x01,	/* 207 */ 0x01,
	/* 208 */ 0x01,	/* 209 */ 0x01,	/* 210 */ 0x01,	/* 211 */ 0x01,	/* 212 */ 0x01,	/* 213 */ 0x01,	/* 214 */ 0x01,	/* 215 */ 0x01,
	/* 216 */ 0x01,	/* 217 */ 0x01,	/* 218 */ 0x01,	/* 219 */ 0x01,	/* 220 */ 0x01,	/* 221 */ 0x01,	/* 222 */ 0x01,	/* 223 */ 0x01,
	/* 224 */ 0x01,	/* 225 */ 0x01,	/* 226 */ 0x01,	/* 227 */ 0x01,	/* 228 */ 0x01,	/* 229 */ 0x01,	/* 230 */ 0x01,	/* 231 */ 0x01,
	/* 232 */ 0x01,	/* 233 */ 0x01,	/* 234 */ 0x01,	/* 235 */ 0x01,	/* 236 */ 0x01,	/* 237 */ 0x01,	/* 238 */ 0x01,	/* 239 */ 0x01,
	/* 240 */ 0x01,	/* 241 */ 0x01,	/* 242 */ 0x01,	/* 243 */ 0x01,	/* 244 */ 0x01,	/* 245 */ 0x01,	/* 246 */ 0x01,	/* 247 */ 0x01,
	/* 248 */ 0x01,	/* 249 */ 0x01,	/* 250 */ 0x01,	/* 251 */ 0x01,	/* 252 */ 0x01,	/* 253 */ 0x01,	/* 254 */ 0x01,	/* 255 */ 0x01,
};

int parse_token(const char *start, const char *end, const char **pos)
{
	while (start < end)
	{
		if ( IS_TOKEN_CHAR(start[0]) )
		{
			++start;
		}
		else
		{
			*pos = start;
			return 1;
		}
	}
	*pos = start;
	return 0;
}