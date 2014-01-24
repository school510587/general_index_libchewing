/**
 * init_database.c
 *
 * Copyright (c) 2013, 2014
 *      libchewing Core Team. See ChangeLog for details.
 *
 * See the file "COPYING" for information on usage and redistribution
 * of this file.
 */

/**
 * @file init_database.c
 *
 * @brief Initialization of system dictionary and phone phrase tree.\n
 *
 *	This program reads in source of dictionary.\n
 *	Output a database file containing a phone phrase tree, and a dictionary file\n
 * filled with non-duplicate phrases.\n
 *	Each node represents a single phone.\n
 *	The output file contains a random access array, where each record includes:\n
 *	\code{
 *	       uint32_t key; may be phone data or record of input keys
 *	       int32_t child.begin, child.end; for internal nodes (key != 0)
 *	       uint32_t phrase.pos; for leaf nodes (key == 0), position of phrase in dictionary
 *	       int32_t phrase.freq; for leaf nodes (key == 0), frequency of the phrase
 *	}\endcode
 */

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "build-tool.h"
#include "chewing-utf8-util.h"
#include "global-private.h"
#include "key2pho-private.h"

/* For ALC macro */
#include "private.h"

char word_matched[MAX_WORD_DATA];

const char USAGE[] =
	"Usage: %s <phone.cin> <tsi.src>\n"
	"This program creates the following new files:\n"
	"* " PHONE_TREE_FILE "\n\tindex to phrase file (dictionary)\n"
	"* " DICT_FILE "\n\tmain phrase file\n"
;

const PhraseData EXCEPTION_PHRASE[] = {
	{ "\xE5\xA5\xBD\xE8\x90\x8A\xE5\xA1\xA2" /* 好萊塢 */ , 0, { 5691, 4138, 256 } /* ㄏㄠˇ ㄌㄞˊ ㄨ */, 0 },
	{ "\xE6\x88\x90\xE6\x97\xA5\xE5\xAE\xB6" /* 成日家 */ , 0, { 8290, 9220, 6281 } /* ㄔㄥˊ ㄖˋ ㄐㄧㄚ˙ */, 0 },
	{ "\xE4\xBF\xBE\xE5\x80\xAA" /* 俾倪 */ , 0, { 644, 3716 } /* ㄅㄧˋ ㄋㄧˋ */, 0 },
	{ "\xE6\x8F\xA9\xE6\xB2\xB9" /* 揩油 */ , 0, { 5128, 194 } /* ㄎㄚ ㄧㄡˊ */, 0 },
	{ "\xE6\x95\x81\xE6\x95\xAA" /* 敁敪 */ , 0, { 2760, 2833 } /* ㄉㄧㄢ ㄉㄨㄛ˙ */, 0 },
	{ "\xE4\xB8\x80\xE9\xAA\xA8\xE7\xA2\x8C" /* 一骨碌 */ , 0, { 128, 4866, 4353 } /* ㄧ ㄍㄨˊ ㄌㄨ˙ */, 0 },
	{ "\xE9\x82\x8B\xE9\x81\xA2" /* 邋遢 */ , 0, { 4106, 3081 } /* ㄌㄚˊ ㄊㄚ˙ */, 0 },

	{ "\xE6\xBA\x9C\xE9\x81\x94" /* 溜達 */ , 0, { 4292, 2569 } /* ㄌㄧㄡˋ ㄉㄚ˙ */, 0 },
	{ "\xE9\x81\x9B\xE9\x81\x94" /* 遛達 */ , 0, { 4292, 2569 } /* ㄌㄧㄡˋ ㄉㄚ˙ */, 0 },
};

/*
 * Some word changes its phone in certain phrases. If it is difficult to list
 * all the phrases to exception phrase list, put the word here so that this
 * won't cause check error.
 */
const PhraseData EXCEPTION_WORD[] = {
	{ "\xE5\x97\xA6" /* 嗦 */, 0, { 11025 } /* ㄙㄨㄛ˙ */, 0 },
	{ "\xE5\xB7\xB4" /* 巴 */, 0, { 521 } /* ㄅㄚ˙ */, 0 },
	{ "\xE4\xBC\x99" /* 伙 */, 0, { 5905 } /* ㄏㄨㄛ˙ */, 0 },
};

int is_exception_phrase(PhraseData *phrase, int pos)
{
	size_t i;
	char word[MAX_UTF8_SIZE + 1];

	ueStrNCpy(word, ueStrSeek(phrase->phrase, pos), 1, 1);

	/*
	 * Check if the phrase is an exception phrase.
	 */
	for (i = 0; i < sizeof(EXCEPTION_PHRASE) / sizeof(EXCEPTION_PHRASE[0]); ++i) {
		if (strcmp(phrase->phrase, EXCEPTION_PHRASE[i].phrase) == 0 &&
			memcmp(phrase->phone, EXCEPTION_PHRASE[i].phone, sizeof(phrase->phone)) == 0) {
			return 1;
		}
	}

	/*
	 * Check if the word in phrase is an exception word.
	 */
	for (i = 0; i < sizeof(EXCEPTION_WORD) / sizeof(EXCEPTION_WORD[0]); ++i) {
		if (strcmp(word, EXCEPTION_WORD[i].phrase) == 0 &&
			phrase->phone[pos] == EXCEPTION_WORD[i].phone[0]) {
			return 1;
		}
	}

	/*
	 * If the same word appears continuous in a phrase (疊字), the second
	 * word can change to light tone.
	 * ex:
	 * 爸爸 -> ㄅㄚˋ ㄅㄚ˙
	 */
	if (pos > 0) {
		char previous[MAX_UTF8_SIZE + 1];

		ueStrNCpy(previous, ueStrSeek(phrase->phrase, pos - 1), 1, 1);

		if (strcmp(previous, word) == 0) {
			if (((phrase->phone[pos - 1] & ~0x7) | 0x1) == phrase->phone[pos]) {
				return 1;
			}
		}
	}

	return 0;
}

void store_phrase(const char *line, int line_num)
{
	const char DELIM[] = " \t\n";
	char buf[MAX_LINE_LEN];
	char *phrase;
	char *freq;
	char *bopomofo;
	char bopomofo_buf[MAX_UTF8_SIZE * ZUIN_SIZE + 1];
	size_t phrase_len;
	WordData word; /* For check. */
	WordData *found_word = NULL;
	int i;
	int j;

	strncpy(buf, line, sizeof(buf));
	strip(buf);
	if (strlen(buf) == 0)
		return;

	if (num_phrase_data >= top_phrase_data) {
		fprintf(stderr, "Need to increase MAX_PHRASE_DATA to process\n");
		exit(-1);
	}

	/* read phrase */
	phrase = strtok(buf, DELIM);
	if (!phrase) {
		fprintf(stderr, "Error reading line %d, `%s'\n", line_num, line);
		exit(-1);
	}
	strncpy(phrase_data[num_phrase_data].phrase, phrase, sizeof(phrase_data[0].phrase));

	/* read frequency */
	freq = strtok(NULL, DELIM);
	if (!freq) {
		fprintf(stderr, "Error reading line %d, `%s'\n", line_num, line);
		exit(-1);
	}

	errno = 0;
	phrase_data[num_phrase_data].freq = strtoul(freq, 0, 0);
	if (errno) {
		fprintf(stderr, "Error reading frequency `%s' in line %d, `%s'\n", freq, line_num, line);
		exit(-1);
	}

	/* read bopomofo */
	for (bopomofo = strtok(NULL, DELIM), phrase_len = 0;
		bopomofo && phrase_len < MAX_PHRASE_LEN;
		bopomofo = strtok(NULL, DELIM), ++phrase_len) {

		phrase_data[num_phrase_data].phone[phrase_len] = UintFromPhone(bopomofo);
		if (phrase_data[num_phrase_data].phone[phrase_len] == 0) {
			fprintf(stderr, "Error reading bopomofo `%s' in line %d, `%s'\n", bopomofo, line_num, line);
			exit(-1);
		}
	}
	if (bopomofo) {
		fprintf(stderr, "Phrase `%s' too long in line %d\n", phrase, line_num);
	}

	/* check phrase length & bopomofo length */
	if ((size_t)ueStrLen(phrase_data[num_phrase_data].phrase) != phrase_len) {
		fprintf(stderr, "Phrase length and bopomofo length mismatch in line %d, `%s'\n", line_num, line);
		exit(-1);
	}

	/* Check that each word in phrase can be found in word list. */
	word.text = ALC(PhraseData, 1);
	assert(word.text);
	for (i = 0; i < phrase_len; ++i) {
		ueStrNCpy(word.text->phrase, ueStrSeek(phrase_data[num_phrase_data].phrase, i), 1, 1);
		word.text->phone[0] = phrase_data[num_phrase_data].phone[i];
		found_word = bsearch(&word, word_data, num_word_data, sizeof(word), compare_word_by_text);
		if ((found_word == NULL ||
			(phrase_len == 1 &&
			word_matched[found_word - word_data])) &&
			!is_exception_phrase(&phrase_data[num_phrase_data], i)) {

			PhoneFromUint(bopomofo_buf, sizeof(bopomofo_buf), word.text->phone[0]);

			fprintf(stderr, "Error in phrase `%s'. Word `%s' has no phone %d (%s) in line %d\n", phrase_data[num_phrase_data].phrase ,word.text->phrase, word.text->phone[0], bopomofo_buf, line_num);
			fprintf(stderr, "\tAdd the following struct to EXCEPTION_PHRASE if this is good phrase\n\t{ \"");
			for (j = 0; j < strlen(phrase_data[num_phrase_data].phrase); ++j) {
				fprintf(stderr, "\\x%02X", (unsigned char)phrase_data[num_phrase_data].phrase[j]);
			}
			fprintf(stderr, "\" /* %s */ , 0, { %d", phrase_data[num_phrase_data].phrase, phrase_data[num_phrase_data].phone[0]);
			for (j = 1; j < phrase_len; ++j) {
				fprintf(stderr, ", %d", phrase_data[num_phrase_data].phone[j]);
			}
			fprintf(stderr, " } /* ");
			for (j = 0; j < phrase_len; ++j) {
				PhoneFromUint(bopomofo_buf, sizeof(bopomofo_buf), phrase_data[num_phrase_data].phone[j]);
				fprintf(stderr, "%s ", bopomofo_buf);
			}
			fprintf(stderr, "*/, 0 },\n");
			exit(-1);
		}
	}
	free(word.text);

	if (phrase_len >= 2)
		++num_phrase_data;
	else
		word_matched[found_word - word_data] = 1;
}

int compare_phrase(const void *x, const void *y)
{
	const PhraseData *a = (const PhraseData*)x;
	const PhraseData *b = (const PhraseData*)y;
	int cmp = strcmp(a->phrase, b->phrase);

	/* If phrases are different, it returns the result of strcmp(); else it
	 * reports an error when the same phone sequence is found.
	 */
	if (cmp) return cmp;
	if (!memcmp(a->phone, b->phone, sizeof(a->phone))) {
		fprintf(stderr, "Duplicated phrase `%s' found.\n", a->phrase);
		exit(-1);
	}
		return b->freq - a->freq;
}

void read_tsi_src(const char *filename)
{
	FILE *tsi_src;
	char buf[MAX_LINE_LEN];
	int line_num = 0;

	tsi_src = fopen(filename, "r");
	if (!tsi_src) {
		fprintf(stderr, "Error opening the file %s\n", filename);
		exit(-1);
	}

	while (fgets(buf, sizeof(buf), tsi_src)) {
		++line_num;
		store_phrase(buf, line_num);
	}

	qsort(phrase_data, num_phrase_data, sizeof(phrase_data[0]), compare_phrase);
}

void write_phrase_data()
{
	FILE *dict_file;
	PhraseData *cur_phr;
	PhraseData *last_phr = NULL;
	int i;
	int j;

	dict_file = fopen(DICT_FILE, "wb");

	if (!dict_file) {
		fprintf(stderr, "Cannot open output file.\n");
		exit(-1);
	}

	/*
	 * Duplicate Chinese strings with common pronunciation are detected and
	 * not written into system dictionary. Written phrases are separated by
	 * '\0', for convenience of mmap usage.
	 * Note: word_data and phrase_data have been qsorted by strcmp in
	 *       reading.
	 */
	for (i = j = 0; i < num_word_data || j < num_phrase_data; last_phr = cur_phr) {
		if (i == num_word_data)
			cur_phr = &phrase_data[j++];
		else if (j == num_phrase_data)
			cur_phr = word_data[i++].text;
		else if (strcmp(word_data[i].text->phrase, phrase_data[j].phrase) < 0)
			cur_phr = word_data[i++].text;
		else
			cur_phr = &phrase_data[j++];

		if (last_phr && !strcmp(cur_phr->phrase, last_phr->phrase))
			cur_phr->pos = last_phr->pos;
		else {
			cur_phr->pos = ftell(dict_file);
			fwrite(cur_phr->phrase, strlen(cur_phr->phrase) + 1, 1, dict_file);
		}
	}

	fclose(dict_file);
}

int main(int argc, char *argv[])
{
	if (argc != 3) {
		printf(USAGE, argv[0]);
		return -1;
	}

	read_IM_cin(argv[1]);
	read_tsi_src(argv[2]);
	write_phrase_data();
	write_index_tree();
	return 0;
}
