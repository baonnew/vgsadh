#include "miner.h"

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "ar2/argon2.h"
#include "ar2/cores.h"
#include "ar2/ar2-scrypt-jane.h"

#define T_COSTS 2
#define M_COSTS 16
#define MASK 8
#define ZERO 0

inline void argon_call(void *out, void *in, void *salt, int type)
{
	argon2_context context;

	context.out = (uint8_t *)out;
	context.pwd = (uint8_t *)in;
	context.salt = (uint8_t*)salt;
	context.pwdlen = 0;
	context.allocate_cbk = NULL;
	context.free_cbk = NULL;

	argon2_core(&context, type);
}

void argon2hash(void *output, const void *input)
{
	uint32_t _ALIGN(64) hashA[8], hashB[8];

	my_scrypt((const unsigned char *)input, 80,
		(const unsigned char *)input, 80,
		(unsigned char *)hashA);

	argon_call(hashB, hashA, hashA, (hashA[0] & MASK) == ZERO);

	my_scrypt((const unsigned char *)hashB, 32,
		(const unsigned char *)hashB, 32,
		(unsigned char *)output);
}

int scanhash_argon2(int thr_id, struct work* work, uint32_t max_nonce, uint64_t *hashes_done)
{
	uint32_t _ALIGN(64) endiandata[20];
	uint32_t _ALIGN(64) hash[8];
	uint32_t *pdata = work->data;
	uint32_t *ptarget = work->target;

	const uint32_t first_nonce = pdata[19];
	const uint32_t Htarg = ptarget[7];
	uint32_t nonce = first_nonce;

	for (int k=0; k < 19; k++)
		be32enc(&endiandata[k], pdata[k]);

	do {
		be32enc(&endiandata[19], nonce);
		argon2hash(hash, endiandata);
		if (hash[7] <= Htarg && fulltest(hash, ptarget)) {
			pdata[19] = nonce;
			*hashes_done = pdata[19] - first_nonce;
			work_set_target_ratio(work, hash);
			return 1;
		}
		nonce++;
	} while (nonce < max_nonce && !work_restart[thr_id].restart);

	pdata[19] = nonce;
	*hashes_done = pdata[19] - first_nonce + 1;
	return 0;
}
