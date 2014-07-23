/*********************************  Base64 Ëã·¨ begin *****************************/
/*
 ** Translation Table as described in RFC1113
 */
#include "base64.h"
#include <string.h>

static const char base64_str[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static const char base64_pad = '=';

static const signed char base64_table[256] = {
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63,
	52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1,
	-1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
	15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1,
	-1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
	41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
};

void encode_base64(const char *src, int sz, char *dst)
{
	if (!src || !(sz > 0)) return;
	int outlen = (sz / 3) * 4;
	unsigned int n = 0;
	unsigned int m = 0;
	unsigned char input[3];
	unsigned int output[4];
	while (n < sz) {
		input[0] = src[n];
		input[1] = (n+1 < sz) ? src[n+1] : 0;
		input[2] = (n+2 < sz) ? src[n+2] : 0;
		output[0] = input[0] >> 2;
		output[1] = ((input[0] & 3) << 4) + (input[1] >> 4);
		output[2] = ((input[1] & 15) << 2) + (input[2] >> 6);
		output[3] = input[2] & 63;
		dst[m++] = base64_str[(int)output[0]];
		dst[m++] = base64_str[(int)output[1]];
		dst[m++] = (n+1 < sz) ? base64_str[(int)output[2]] : base64_pad;
		dst[m++] = (n+2 < sz) ? base64_str[(int)output[3]] : base64_pad;
		n+=3;
	}
	dst[m] = 0; // 0-termination!
	//*sz = m;
	return;
}

static int base64decode_block(unsigned char *target, const char *data, unsigned int data_size)
{
	int w1,w2,w3,w4;
	int n,i;

	if (!data || (data_size <= 0)) {
		return 0;
	}

	n = 0;
	i = 0;
	while (n < data_size-3) {
		w1 = base64_table[(int)data[n]];
		w2 = base64_table[(int)data[n+1]];
		w3 = base64_table[(int)data[n+2]];
		w4 = base64_table[(int)data[n+3]];

		if (w2 >= 0) {
			target[i++] = (char)((w1*4 + (w2 >> 4)) & 255);
		}
		if (w3 >= 0) {
			target[i++] = (char)((w2*16 + (w3 >> 2)) & 255);
		}
		if (w4 >= 0) {
			target[i++] = (char)((w3*64 + w4) & 255);
		}
		n+=4;
	}
	return i;
}

int decode_base64(char *src, int src_len, char *dst)
{
	if (!src) return 0;
	if (src_len <= 0) return 0;
	int ret_len = 0;
	unsigned char *line;
	int p = 0;

	line = (unsigned char*)strtok((char*)src, "\r\n\t ");
	while (line) {
		p+=base64decode_block((unsigned char*)(dst+p), (char *)line, strlen((char*)line));

		// get next line of base64 encoded block
		line = (unsigned char*)strtok(NULL, "\r\n\t ");
	}
	dst[p] = 0;
	return p;
}
// end of part1: Base64 Ëã·¨ end 
