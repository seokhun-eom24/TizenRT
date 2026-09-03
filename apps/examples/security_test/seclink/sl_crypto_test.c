/****************************************************************************
 *
 * Copyright 2021 Samsung Electronics All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific
 * language governing permissions and limitations under the License.
 *
 ****************************************************************************/

#include <tinyara/config.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <tinyara/seclink.h>
#include <tinyara/seclink_drv.h>
#include <stress_tool/st_perf.h>
#include "sl_test.h"
#include "sl_test_usage.h"

ST_SET_PACK_GLOBAL(sl_crypto);

static char *g_command[] = {
#ifdef SL_CRYPTO_TEST_POOL
#undef SL_CRYPTO_TEST_POOL
#endif
#define SL_CRYPTO_TEST_POOL(command, type, func) command,
#include "sl_crypto_table.h"
};

#ifdef SL_CRYPTO_TEST_POOL
#undef SL_CRYPTO_TEST_POOL
#endif
#define SL_CRYPTO_TEST_POOL(command, type, func) \
	extern void func(sl_options *opt);
#include "sl_crypto_table.h"

static sl_test_func g_func_list[] = {
#ifdef SL_CRYPTO_TEST_POOL
#undef SL_CRYPTO_TEST_POOL
#endif
#define SL_CRYPTO_TEST_POOL(command, type, func) func,
#include "sl_crypto_table.h"
};

typedef enum {
#ifdef SL_CRYPTO_TEST_POOL
#undef SL_CRYPTO_TEST_POOL
#endif
#define SL_CRYPTO_TEST_POOL(command, type, func) type,
#include "sl_crypto_table.h"
	SL_CRYPTO_TYPE_MAX,
	SL_CRYPTO_TYPE_ERR = -1
} sl_crypto_type_e;

#define ST_AES_ENC_KEY_IDX 32
#define ST_AES_DEC_KEY_IDX 33
#define ST_AES_BLOCK_SIZE 16
#define ST_AES_FACTORY_KEY_IDX 1
#define ST_AES_STG_INPUT_LEN 5120
#define ST_AES_STG_BUFFER_LEN (ST_AES_STG_INPUT_LEN + ST_AES_BLOCK_SIZE)
sl_ctx g_hnd;
char g_key_128[16] = {0,};
char g_key_192[24] = {0,};
char g_key_256[32] = {0,};
unsigned char g_plaintext[128] = {0,};
unsigned char g_ciphertext[128] = {0,};
unsigned char g_iv[16] = {
	0,
};

TESTCASE_SETUP(sl_crypto_global)
{
	ST_EXPECT_EQ(SECLINK_OK, sl_init(&g_hnd));
}
END_TESTCASE

TESTCASE_TEARDOWN(sl_crypto_global)
{
	ST_EXPECT_EQ(SECLINK_OK, sl_deinit(g_hnd));
}
END_TESTCASE

START_TEST_F(aes_ecb)
{
	hal_data aes_key = HAL_DATA_INITIALIZER;
	hal_data enc = HAL_DATA_INITIALIZER;
	hal_data dec = HAL_DATA_INITIALIZER;
	HAL_INIT_AES_PARAM(param);

	aes_key.data = g_key_128;
	aes_key.data_len = 16;
	param.mode = HAL_AES_ECB_NOPAD;
	enc.data = g_plaintext;
	enc.data_len = 16;
	dec.data = g_ciphertext;
	dec.data_len = 16;
	ST_EXPECT_EQ(SECLINK_OK, sl_set_key(g_hnd, HAL_KEY_AES_128, ST_AES_ENC_KEY_IDX, &aes_key, NULL));
	ST_EXPECT_EQ(SECLINK_OK, sl_aes_encrypt(g_hnd, &dec, &param, ST_AES_ENC_KEY_IDX, &enc));
	sl_test_print_buffer(enc.data, enc.data_len, "AES-ECB plaintext");
	sl_test_print_buffer(dec.data, dec.data_len, "AES-ECB ciphertext");

	ST_EXPECT_EQ(SECLINK_OK, sl_set_key(g_hnd, HAL_KEY_AES_128, ST_AES_DEC_KEY_IDX, &aes_key, NULL));
	ST_EXPECT_EQ(SECLINK_OK, sl_aes_decrypt(g_hnd, &dec, &param, ST_AES_DEC_KEY_IDX, &enc));

	ST_EXPECT_EQ(SECLINK_OK, sl_remove_key(g_hnd, HAL_KEY_AES_128, ST_AES_ENC_KEY_IDX));
	ST_EXPECT_EQ(SECLINK_OK, sl_remove_key(g_hnd, HAL_KEY_AES_128, ST_AES_DEC_KEY_IDX));
}
END_TEST_F

/**
 * @testcase         aes_long_input
 * @brief            decrypt STG-sized AES NOPAD inputs through seclink
 * @scenario         cover aligned and unaligned inputs around 5120 bytes
 * @apicovered       sl_aes_decrypt
 *
 * The public security API has a 4096-byte scratch output buffer.  Use the
 * direct seclink API so the test isolates the secure-engine result mapping.
 * ECB_NOPAD accepts non-aligned input on this target, while CBC_NOPAD maps
 * non-aligned input to SECLINK_INVALID_ARGS.
 */
START_TEST_F(aes_long_input)
{
	static const hal_aes_algo mode_table[] = {
		HAL_AES_ECB_NOPAD,
		HAL_AES_CBC_NOPAD,
	};
	static const unsigned int input_len_table[] = {
		ST_AES_STG_INPUT_LEN,
		ST_AES_STG_BUFFER_LEN,
		ST_AES_STG_INPUT_LEN - 1,
		ST_AES_STG_INPUT_LEN + 1,
		ST_AES_STG_BUFFER_LEN - 1,
	};
	unsigned char iv[ST_AES_BLOCK_SIZE] = {0, };
	hal_data input = HAL_DATA_INITIALIZER;
	hal_data output = HAL_DATA_INITIALIZER;

	if (sl_test_malloc_buffer(&input, ST_AES_STG_BUFFER_LEN) != 0 ||
		sl_test_malloc_buffer(&output, ST_AES_STG_BUFFER_LEN) != 0) {
		printf("[AES long input] buffer allocation failed\n");
		st_res = STRESS_TC_FAIL;
		goto cleanup;
	}

	for (unsigned int i = 0; i < ST_AES_STG_BUFFER_LEN; i++) {
		((unsigned char *)input.data)[i] = (unsigned char)('A' + (i % 26));
	}

	for (unsigned int mode_idx = 0;
		 mode_idx < sizeof(mode_table) / sizeof(mode_table[0]);
		 mode_idx++) {
		for (unsigned int len_idx = 0;
			 len_idx < sizeof(input_len_table) / sizeof(input_len_table[0]);
			 len_idx++) {
			unsigned int input_len = input_len_table[len_idx];
			int expected_res = (mode_table[mode_idx] == HAL_AES_CBC_NOPAD &&
							input_len % ST_AES_BLOCK_SIZE != 0) ?
							SECLINK_INVALID_ARGS : SECLINK_OK;
			int res;

			HAL_INIT_AES_PARAM(param);
			param.mode = mode_table[mode_idx];
			param.iv = iv;
			param.iv_len = sizeof(iv);
			input.data_len = input_len;
			output.data_len = ST_AES_STG_BUFFER_LEN;
			memset(iv, 0, sizeof(iv));

			printf("[AES long input] begin mode=%d length=%u mod16=%u\n",
				   param.mode, input_len, input_len % ST_AES_BLOCK_SIZE);
			res = sl_aes_decrypt(g_hnd, &input, &param,
							 ST_AES_FACTORY_KEY_IDX, &output);
			printf("[AES long input] result mode=%d length=%u mod16=%u result=%d\n",
				   param.mode, input_len, input_len % ST_AES_BLOCK_SIZE, res);
			if (res != expected_res) {
				printf("[AES long input] expected=%d result=%d\n",
					   expected_res, res);
				st_res = STRESS_TC_FAIL;
				goto cleanup;
			}
		}
	}

cleanup:
	sl_test_free_buffer(&input);
	sl_test_free_buffer(&output);
}
END_TEST_F

START_TEST_F(aes_cbc)
{
	hal_data aes_key = HAL_DATA_INITIALIZER;
	hal_data enc = HAL_DATA_INITIALIZER;
	hal_data dec = HAL_DATA_INITIALIZER;
	HAL_INIT_AES_PARAM(param);

	aes_key.data = g_key_128;
	aes_key.data_len = 16;
	param.mode = HAL_AES_CBC_NOPAD;
	param.iv = (unsigned char *)g_iv;
	param.iv_len = 16;

	enc.data = g_plaintext;
	enc.data_len = 16;
	dec.data = g_ciphertext;
	dec.data_len = 16;

	ST_EXPECT_EQ(SECLINK_OK, sl_set_key(g_hnd, HAL_KEY_AES_128, ST_AES_ENC_KEY_IDX, &aes_key, NULL));
	ST_EXPECT_EQ(SECLINK_OK, sl_aes_encrypt(g_hnd, &dec, &param, ST_AES_ENC_KEY_IDX, &enc));
	sl_test_print_buffer(enc.data, enc.data_len, "AES-ECB plaintext");
	sl_test_print_buffer(dec.data, dec.data_len, "AES-ECB ciphertext");
	sl_test_print_buffer((char *)g_iv, 16, "IV");

	ST_EXPECT_EQ(SECLINK_OK, sl_aes_decrypt(g_hnd, &dec, &param, ST_AES_ENC_KEY_IDX, &enc));

	ST_EXPECT_EQ(SECLINK_OK, sl_remove_key(g_hnd, HAL_KEY_AES_128, ST_AES_ENC_KEY_IDX));
}
END_TEST_F

START_TEST_F(aes_cfb128)
{
	hal_data aes_key = HAL_DATA_INITIALIZER;
	hal_data enc = HAL_DATA_INITIALIZER;
	hal_data dec = HAL_DATA_INITIALIZER;
	HAL_INIT_AES_PARAM(param);
	unsigned int iv_offset = 0;

	aes_key.data = g_key_128;
	aes_key.data_len = 16;
	param.mode = HAL_AES_CFB128;
	param.iv = g_iv;
	param.iv_len = 16;
	param.iv_offset = &iv_offset;

	enc.data = g_plaintext;
	enc.data_len = 64;
	dec.data = g_ciphertext;
	dec.data_len = 64;

	ST_EXPECT_EQ(SECLINK_OK, sl_set_key(g_hnd, HAL_KEY_AES_128, ST_AES_ENC_KEY_IDX, &aes_key, NULL));
	ST_EXPECT_EQ(SECLINK_OK, sl_aes_encrypt(g_hnd, &dec, &param, ST_AES_ENC_KEY_IDX, &enc));
	sl_test_print_buffer(enc.data, enc.data_len, "AES-CFB128 plaintext");
	sl_test_print_buffer(dec.data, dec.data_len, "AES-CFB128 ciphertext");
	sl_test_print_buffer((char *)g_iv, 16, "IV");
	printf("iv offset %d\n", iv_offset);

	ST_EXPECT_EQ(SECLINK_OK, sl_aes_decrypt(g_hnd, &dec, &param, ST_AES_ENC_KEY_IDX, &enc));
	ST_EXPECT_EQ(SECLINK_OK, sl_remove_key(g_hnd, HAL_KEY_AES_128, ST_AES_ENC_KEY_IDX));
}
END_TEST_F

START_TEST_F(aes_ctr)
{
	hal_data aes_key = HAL_DATA_INITIALIZER;
	hal_data enc = HAL_DATA_INITIALIZER;
	hal_data dec = HAL_DATA_INITIALIZER;
	HAL_INIT_AES_PARAM(param);
	unsigned int nc_offset = 0;
	unsigned char nonce_counter[16] = {0,};
	unsigned char stream_block[16] = {0,};
	aes_key.data = g_key_128;
	aes_key.data_len = 16;
	param.mode = HAL_AES_CTR;
	param.nc_off = &nc_offset;
	param.nonce_counter = nonce_counter;
	param.stream_block = stream_block;

	enc.data = g_plaintext;
	enc.data_len = 64;
	dec.data = g_ciphertext;
	dec.data_len = 64;

	ST_EXPECT_EQ(SECLINK_OK, sl_set_key(g_hnd, HAL_KEY_AES_128, ST_AES_ENC_KEY_IDX, &aes_key, NULL));
	ST_EXPECT_EQ(SECLINK_OK, sl_aes_encrypt(g_hnd, &dec, &param, ST_AES_ENC_KEY_IDX, &enc));
	sl_test_print_buffer(enc.data, enc.data_len, "AES-CTR plaintext");
	sl_test_print_buffer(dec.data, dec.data_len, "AES-CTR ciphertext");
	sl_test_print_buffer((char *)nonce_counter, 16, "nonce counter");
	sl_test_print_buffer((char *)stream_block, 16, "stream block");
	printf("nc offset %d\n", nc_offset);

	ST_EXPECT_EQ(SECLINK_OK, sl_aes_decrypt(g_hnd, &dec, &param, ST_AES_ENC_KEY_IDX, &enc));
	ST_EXPECT_EQ(SECLINK_OK, sl_remove_key(g_hnd, HAL_KEY_AES_128, ST_AES_ENC_KEY_IDX));
}
END_TEST_F

START_TEST_F(gcm_aes)
{
	hal_data aes_key = HAL_DATA_INITIALIZER;
	hal_data enc = HAL_DATA_INITIALIZER;
	hal_data dec = HAL_DATA_INITIALIZER;
	HAL_INIT_GCM_PARAM(param);
	unsigned char aad[16] = {0,};
	unsigned char tag[16] = {0,};
	aes_key.data = g_key_128;
	aes_key.data_len = 16;
	param.cipher = HAL_GCM_AES;
	param.iv = (unsigned char *)g_iv;
	param.iv_len = 16;
	param.aad = aad;
	param.aad_len = 16;
	param.tag = tag;
	param.tag_len = 16;

	enc.data = g_plaintext;
	enc.data_len = 64;
	dec.data = g_ciphertext;
	dec.data_len = 64;

	ST_EXPECT_EQ(SECLINK_OK, sl_set_key(g_hnd, HAL_KEY_AES_128, ST_AES_ENC_KEY_IDX, &aes_key, NULL));
	ST_EXPECT_EQ(SECLINK_OK, sl_gcm_encrypt(g_hnd, &dec, &param, ST_AES_ENC_KEY_IDX, &enc));
	sl_test_print_buffer(enc.data, enc.data_len, "GCM-AES plaintext");
	sl_test_print_buffer(dec.data, dec.data_len, "GCM-AES ciphertext");
	sl_test_print_buffer((char *)g_iv, 16, "IV");
	sl_test_print_buffer((char *)aad, 16, "AAD");
	sl_test_print_buffer((char *)param.tag, 16, "TAG");

	ST_EXPECT_EQ(SECLINK_OK, sl_gcm_decrypt(g_hnd, &dec, &param, ST_AES_ENC_KEY_IDX, &enc));
	sl_test_print_buffer(enc.data, enc.data_len, "GCM-AES plaintext (decrypted text)");

	ST_EXPECT_EQ(SECLINK_OK, sl_remove_key(g_hnd, HAL_KEY_AES_128, ST_AES_ENC_KEY_IDX));
}
END_TEST_F

void sl_handle_crypto_aes_ecb(sl_options *opt)
{
	ST_SET_SMOKE1(sl_crypto, opt->count, 0, "aes test", aes_ecb);
}

void sl_handle_crypto_aes_long_input(sl_options *opt)
{
	ST_SET_SMOKE1(sl_crypto, opt->count, 0, "aes long input test", aes_long_input);
}

void sl_handle_crypto_aes_cbc(sl_options *opt)
{
	ST_SET_SMOKE1(sl_crypto, opt->count, 0, "aes test", aes_cbc);
}

void sl_handle_crypto_aes_cfb128(sl_options *opt)
{
	ST_SET_SMOKE1(sl_crypto, opt->count, 0, "aes test", aes_cfb128);
}

void sl_handle_crypto_aes_ctr(sl_options *opt)
{
	ST_SET_SMOKE1(sl_crypto, opt->count, 0, "aes test", aes_ctr);
}

void sl_handle_crypto_gcm_aes(sl_options *opt)
{
	ST_SET_SMOKE1(sl_crypto, opt->count, 0, "gcm test", gcm_aes);
}

void sl_handle_crypto(sl_options *opt)
{
	ST_TC_SET_GLOBAL(sl_crypto, sl_crypto_global);

	SL_PARSE_MESSAGE(opt, g_command, sl_crypto_type_e,
					 g_func_list, SL_CRYPTO_TYPE_MAX, SL_CRYPTO_TYPE_ERR);
	ST_RUN_TEST(sl_crypto);
	ST_RESULT_TEST(sl_crypto);
}
