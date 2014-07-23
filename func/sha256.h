#if defined(__x86_64__)
typedef unsigned ulong32;
#else
typedef unsigned long ulong32;
#endif

/* Helpful macros */
#define STORE32H(x, y)							\
{ (y)[0] = (unsigned char)(((x)>>24)&255);		\
	(y)[1] = (unsigned char)(((x)>>16)&255);	\
	(y)[2] = (unsigned char)(((x)>>8)&255);		\
	(y)[3] = (unsigned char)((x)&255); }

#define LOAD32H(x, y)							\
{ x = ((ulong32)((y)[0] & 255)<<24) |			\
	((ulong32)((y)[1] & 255)<<16) |				\
	((ulong32)((y)[2] & 255)<<8) |				\
	((ulong32)((y)[3] & 255)); }

#define ROR(x, y)								\
	((((ulong32)(x)>>(ulong32)((y)&31)) |		\
	  (((ulong32)(x)&0xFFFFFFFFUL)<<			\
	   (ulong32)(32-((y)&31)))) & 0xFFFFFFFFUL)

#define Ch(x,y,z)  (z ^ (x & (y ^ z)))
#define Maj(x,y,z) (((x | y) & z) | (x & y))

#define S(x, n)    ROR((x),(n))
#define R(x, n)    (((x)&0xFFFFFFFFUL)>>(n))

#define Sigma0(x)  (S(x, 2) ^ S(x, 13) ^ S(x, 22))
#define Sigma1(x)  (S(x, 6) ^ S(x, 11) ^ S(x, 25))

#define Gamma0(x)  (S(x, 7) ^ S(x, 18) ^ R(x, 3))
#define Gamma1(x)  (S(x, 17) ^ S(x, 19) ^ R(x, 10))

typedef struct {
	unsigned char buf[64];
	unsigned long buflen, msglen;
	ulong32       S[8];
} sha256_state;

static const ulong32 K[64] = {
	0x428a2f98UL, 0x71374491UL, 0xb5c0fbcfUL, 0xe9b5dba5UL,
	0x3956c25bUL, 0x59f111f1UL, 0x923f82a4UL, 0xab1c5ed5UL,
	0xd807aa98UL, 0x12835b01UL, 0x243185beUL, 0x550c7dc3UL,
	0x72be5d74UL, 0x80deb1feUL, 0x9bdc06a7UL, 0xc19bf174UL,
	0xe49b69c1UL, 0xefbe4786UL, 0x0fc19dc6UL, 0x240ca1ccUL,
	0x2de92c6fUL, 0x4a7484aaUL, 0x5cb0a9dcUL, 0x76f988daUL,
	0x983e5152UL, 0xa831c66dUL, 0xb00327c8UL, 0xbf597fc7UL,
	0xc6e00bf3UL, 0xd5a79147UL, 0x06ca6351UL, 0x14292967UL,
	0x27b70a85UL, 0x2e1b2138UL, 0x4d2c6dfcUL, 0x53380d13UL,
	0x650a7354UL, 0x766a0abbUL, 0x81c2c92eUL, 0x92722c85UL,
	0xa2bfe8a1UL, 0xa81a664bUL, 0xc24b8b70UL, 0xc76c51a3UL,
	0xd192e819UL, 0xd6990624UL, 0xf40e3585UL, 0x106aa070UL,
	0x19a4c116UL, 0x1e376c08UL, 0x2748774cUL, 0x34b0bcb5UL,
	0x391c0cb3UL, 0x4ed8aa4aUL, 0x5b9cca4fUL, 0x682e6ff3UL,
	0x748f82eeUL, 0x78a5636fUL, 0x84c87814UL, 0x8cc70208UL,
	0x90befffaUL, 0xa4506cebUL, 0xbef9a3f7UL, 0xc67178f2UL
};

void sha256_init(sha256_state *md)
{
	md->S[0] = 0x6A09E667UL;
	md->S[1] = 0xBB67AE85UL;
	md->S[2] = 0x3C6EF372UL;
	md->S[3] = 0xA54FF53AUL;
	md->S[4] = 0x510E527FUL;
	md->S[5] = 0x9B05688CUL;
	md->S[6] = 0x1F83D9ABUL;
	md->S[7] = 0x5BE0CD19UL;
	md->buflen = md->msglen = 0;
}

static void sha256_compress(sha256_state *md)
{
	ulong32 W[64], a, b, c, d, e, f, g, h, t, t0, t1;
	unsigned x;

	/* load W[0..15] */
	for (x = 0; x < 16; x++) {
		LOAD32H(W[x], md->buf + 4 * x);
	}

	/* compute W[16...63] */
	for (x = 16; x < 64; x++) {
		W[x] = Gamma1(W[x - 2]) + W[x - 7] +
			Gamma0(W[x - 15]) + W[x - 16];
	}

	/* load a copy of the state */
	a = md->S[0]; b = md->S[1]; c = md->S[2];
	d = md->S[3]; e = md->S[4]; f = md->S[5];
	g = md->S[6]; h = md->S[7];

	/* perform 64 rounds */
	for (x = 0; x < 64; x++) {
		t0 = h + Sigma1(e) + Ch(e, f, g) + K[x] + W[x];
		t1 = Sigma0(a) + Maj(a, b, c);
		d += t0;
		h = t0 + t1;

		/* swap */
		t = h; h = g; g = f; f = e; e = d;
		d = c; c = b; b = a; a = t;
	}

	/* update state */
	md->S[0] += a;
	md->S[1] += b;
	md->S[2] += c;
	md->S[3] += d;
	md->S[4] += e;
	md->S[5] += f;
	md->S[6] += g;
	md->S[7] += h;
}

void sha256_process(sha256_state *md,
		const unsigned char *buf,
		unsigned long len)
{
	unsigned long x, y;

	while (len) {
		x = (64 - md->buflen) < len ? 64 - md->buflen : len;
		len -= x;

		/* copy x bytes from buf to the buffer */
		for (y = 0; y < x; y++) {
			md->buf[md->buflen++] = *buf++;
		}

		if (md->buflen == 64) {
			sha256_compress(md);
			md->buflen = 0;
			md->msglen += 64;
		}
	}
}
void sha256_done(sha256_state *md,
		unsigned char *dst)
{
	ulong32 l1, l2, i;

	/* compute final length as 8*md->msglen */
	md->msglen += md->buflen;
	l2 = md->msglen >> 29;
	l1 = (md->msglen << 3) & 0xFFFFFFFF;

	/* add the padding bit */
	md->buf[md->buflen++] = 0x80;

	/* if the current len > 56 we have to finish this block */
	if (md->buflen > 56) {
		while (md->buflen < 64) {
			md->buf[md->buflen++] = 0x00;
		}
		sha256_compress(md);
		md->buflen = 0;
	}

	/* now pad until we are at pos 56 */
	while (md->buflen < 56) {
		md->buf[md->buflen++] = 0x00;
	}

	/* store the length */
	STORE32H(l2, md->buf + 56);
	STORE32H(l1, md->buf + 60);

	/* compress */
	sha256_compress(md);

	/* extract the state */
	for (i = 0; i < 8; i++) {
		STORE32H(md->S[i], dst + i*4);
	}
}

void sha256_memory(const unsigned char *in,
		unsigned long len,
		unsigned char *dst)
{
	sha256_state md;
	sha256_init(&md);
	sha256_process(&md, in, len);
	sha256_done(&md, dst);
}