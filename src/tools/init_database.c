/**
 * init_database.c
 *
 * Copyright (c) 2013
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
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chewing-private.h"
#include "chewing-utf8-util.h"
#include "global-private.h"
#include "key2pho-private.h"
#include "zuin-private.h"

/* For ALC macro */
#include "private.h"

#define CHARDEF		 "%chardef"
#define BEGIN		   "begin"
#define END		     "end"
#define MAX_LINE_LEN	    (1024)
#define MAX_WORD_DATA	   (60000)
#define MAX_PHRASE_BUF_LEN      (149)
#define MAX_PHRASE_DATA	 (420000)

const char USAGE[] =
	"Usage: %s <phone.cin> <tsi.src>\n"
	"This program creates the following new files:\n"
	"* " PHONE_TREE_FILE "\n\tindex to phrase file (dictionary)\n"
	"* " DICT_FILE "\n\tmain phrase file\n"
;

/* An additional pos helps avoid duplicate Chinese strings. */
typedef struct {
	char phrase[MAX_PHRASE_BUF_LEN];
	int freq;
	uint16_t phone[MAX_PHRASE_LEN + 1];
	long pos;
} PhraseData;

typedef struct {
	PhraseData text; // Common part shared with PhraseData. */
	int index; /* For stable sorting. */
} WordData;

/*
 * Please see TreeType for data field. pFirstChild points to the first of its
 * child list. pNextSibling points to its right sibling, where it and its right
 * sibling are both in the child list of its parent. However, pNextSibling will
 * become next-pointer like linked list, which makes writing of index-tree file
 * become a sequential traversal rather than BFS.
 */
typedef struct _tNODE {
	TreeType data;
	struct _tNODE *pFirstChild, *pNextSibling;
} NODE;

WordData word_data[MAX_WORD_DATA];
int num_word_data = 0;

PhraseData phrase_data[MAX_PHRASE_DATA];
int num_phrase_data = 0;

NODE *root;

void strip(char *line)
{
	char *end;
	size_t i;

	/* remove comment */
	for (i = 0; i < strlen(line); ++i) {
		if (line[i] == '#') {
			line[i] = '\0';
			break;
		}
	}

	/* remove tailing space */
	end = line + strlen(line) - 1;
	while (end >= line && isspace((unsigned char)*end)) {
		*end = 0;
		--end;
	}
}

/* word_data is sorted reversely, for stack-like push operation. */
int compare_word_by_phone(const void *x, const void *y)
{
	const WordData *a = (const WordData *)x;
	const WordData *b = (const WordData *)y;

	if (a->text.phone[0] != b->text.phone[0])
		return b->text.phone[0] - a->text.phone[0];

	/* Compare original index for stable sort */
	return b->index - a->index;
}

int compare_word_by_text(const void *x, const void *y)
{
	const WordData *a = (const WordData *)x;
	const WordData *b = (const WordData *)y;
	int ret;

	ret = strcmp(a->text.phrase, b->text.phrase);
	if (ret != 0)
		return ret;

	if (a->text.phone[0] != b->text.phone[0])
		return a->text.phone[0] - b->text.phone[0];

	return 0;
}

void store_phrase(const char *line, int line_num)
{
	const char DELIM[] = " \t\n";
	char buf[MAX_LINE_LEN];
	char *phrase;
	char *freq;
	char *bopomofo, bopomofo_buf[MAX_UTF8_SIZE*ZUIN_SIZE+1];
	size_t phrase_len;
	WordData word; /* For check. */
	int i;

	strncpy(buf, line, sizeof(buf));
	strip(buf);
	if (strlen(buf) == 0)
		return;

	if (num_phrase_data >= MAX_PHRASE_DATA) {
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
	phrase_data[num_phrase_data].freq = strtol(freq, 0, 0);
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

	/* Please delete this #if after resoving exception phrase cases. It causes some unused variables. */
#if 0
	/* Check that each word in phrase can be found in word list. */
	for (i = 0; i < phrase_len; ++i) {
		ueStrNCpy(word.text.phrase, ueStrSeek(phrase_data[num_phrase_data].phrase, i), 1, 1);
		word.text.phone[0] = phrase_data[num_phrase_data].phone[i];
		if (bsearch(&word, word_data, num_word_data, sizeof(word), compare_word_by_text) == NULL) {
			PhoneFromUint(bopomofo_buf, sizeof(bopomofo_buf), word.text.phone[0]);
			fprintf(stderr, "Phrase:%d:error:`%s': `%s' has no phone `%s'\n", line_num, phrase_data[num_phrase_data].phrase, word.text.phrase, bopomofo_buf);
			exit(-1);
		}
	}
#endif

	if (phrase_len >= 2)
		++num_phrase_data;
}

int compare_phrase(const void *x, const void *y)
{
	const PhraseData *a = (const PhraseData *) x, *b = (const PhraseData *) y;
	int cmp = strcmp(a->phrase, b->phrase);

	/* If phrases are different, it returns the result of strcmp(); else it
	 * reports an error when the same phone sequence is found.
	 */
	if (cmp) return cmp;
	else {
		if (!memcmp(a->phone, b->phone, sizeof(a->phone))) {
			fprintf(stderr, "Duplicated phrase `%s' found.\n", a->phrase);
			exit(-1);
		}
		else
			return b->freq - a->freq;
	}
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

void store_word(const char *line, const int line_num)
{
	char phone_buf[MAX_UTF8_SIZE * ZUIN_SIZE + 1];
	char key_buf[ZUIN_SIZE + 1];
	char buf[MAX_LINE_LEN];

	strncpy(buf, line, sizeof(buf));

	strip(buf);
	if (strlen(buf) == 0)
		return;

	if (num_word_data >= MAX_WORD_DATA) {
		fprintf(stderr, "Need to increase MAX_WORD_DATA to process\n");
		exit(-1);
	}

#define UTF8_FORMAT_STRING(len1, len2) \
	"%" __stringify(len1) "[^ ]" " " \
	"%" __stringify(len2) "[^ ]"
	sscanf(buf, UTF8_FORMAT_STRING(ZUIN_SIZE, MAX_UTF8_SIZE),
		key_buf, word_data[num_word_data].text.phrase);

	if (strlen(key_buf) > ZUIN_SIZE) {
		fprintf(stderr, "Error reading line %d, `%s'\n", line_num, line);
		exit(-1);
	}
	PhoneFromKey(phone_buf, key_buf, KB_DEFAULT, 1);
	word_data[num_word_data].text.phone[0] = UintFromPhone(phone_buf);

	word_data[num_word_data].index = num_word_data;
	++num_word_data;
}

void read_phone_cin(const char *filename)
{
	FILE *phone_cin;
	char buf[MAX_LINE_LEN];
	char *ret;
	int line_num = 0;
	enum{INIT, HAS_CHARDEF_BEGIN, HAS_CHARDEF_END} status;

	phone_cin = fopen(filename, "r");
	if (!phone_cin) {
		fprintf(stderr, "Error opening the file %s\n", filename);
		exit(-1);
	}

	for (status = INIT; status != HAS_CHARDEF_END; ) {
		ret = fgets(buf, sizeof(buf), phone_cin);
		++line_num;
		if (!ret) {
			fprintf(stderr, "Cannot find %s %s\n", CHARDEF, BEGIN);
			exit(-1);
		}

		strip( buf );
		if (buf[0] == '%') {
			ret = strtok(buf, " \t");
			if (!strcmp(ret, CHARDEF)) {
				ret = strtok(NULL, " \t");
				switch (status) {
					case INIT:
						if (!strcmp(ret, BEGIN))
							status = HAS_CHARDEF_BEGIN;
						else {
							fprintf(stderr, "%s %s is expected.\n", CHARDEF, BEGIN);
							exit(-1);
						}
					break;
					case HAS_CHARDEF_BEGIN:
						if (!strcmp(ret, END))
							status = HAS_CHARDEF_END;
						else {
							fprintf(stderr, "%s %s is expected.\n", CHARDEF, END);
							exit(-1);
						}
					break;
					case HAS_CHARDEF_END:
						assert(!"NOTREACHED");
					break;
				}
			}
		}
		else if (status == HAS_CHARDEF_BEGIN)
			store_word(buf, line_num);
	}
	fclose(phone_cin);

	qsort(word_data, num_word_data, sizeof(word_data[0]), compare_word_by_text);
}

NODE *new_node( uint32_t key )
{
	NODE *pnew = ALC(NODE, 1);

	if (pnew == NULL) {
		fprintf(stderr, "Memory allocation failed on constructing phrase tree.\n");
		exit(-1);
	}

	memset(&pnew->data, 0, sizeof(pnew->data));
	pnew->data.key = key;
	pnew->pFirstChild = NULL;
	pnew->pNextSibling=NULL;
	return pnew;
}

/*
 * This function puts FindKey() and Insert() together. It first searches for the
 * specified key and performs FindKey() on hit. Otherwise, it inserts a new node
 * at proper position and returns the newly inserted child.
 */
NODE *find_or_insert(NODE *parent, uint32_t key)
{
	NODE *prev=NULL, *p, *pnew;

	for (p = parent->pFirstChild; p && p->data.key <= key; prev = p, p = p->pNextSibling)
		if (p->data.key == key)
			return p;

	pnew = new_node( key );
	pnew->pNextSibling = p;
	if (prev == NULL)
		parent->pFirstChild = pnew;
	else
		prev->pNextSibling = pnew;
	pnew->pNextSibling = p;
	return pnew;
}

void insert_leaf(NODE *parent, long phr_pos, int freq)
{
	NODE *prev=NULL, *p, *pnew;

	for (p = parent->pFirstChild; p && p->data.key == 0; prev = p, p = p->pNextSibling)
		if (p->data.phrase.freq <= freq)
			break;

	pnew = new_node(0);
	pnew->data.phrase.pos = (uint32_t)phr_pos;
	pnew->data.phrase.freq = freq;
	if (prev == NULL)
		parent->pFirstChild = pnew;
	else
		prev->pNextSibling = pnew;
	pnew->pNextSibling = p;
}

void construct_phrase_tree()
{
	NODE *levelPtr;
	int i, j;

	/* First, assume that words are in order of their phones and indices. */
	qsort(word_data, num_word_data, sizeof(word_data[0]), compare_word_by_phone);

	/* Key value of root will become tree_size later. */
	root = new_node( 1 );

	/* Second, insert word_data as the first level of children. */
	for (i = 0; i < num_word_data; i++) {
		if (i == 0 || word_data[i].text.phone[0] != word_data[i-1].text.phone[0]) {
			levelPtr = new_node(word_data[i].text.phone[0]);
			levelPtr->pNextSibling = root->pFirstChild;
			root->pFirstChild = levelPtr;
		}
		levelPtr = new_node( 0 );
		levelPtr->data.phrase.pos = (uint32_t)word_data[i].text.pos;
		levelPtr->data.phrase.freq = word_data[i].text.freq;
		levelPtr->pNextSibling = root->pFirstChild->pFirstChild;
		root->pFirstChild->pFirstChild = levelPtr;
	}

	/* Third, insert phrases having length at least 2. */
	for (i = 0; i < num_phrase_data; ++i) {
		levelPtr = root;
		for (j = 0; phrase_data[i].phone[j] != 0; ++j)
			levelPtr = find_or_insert(levelPtr, phrase_data[i].phone[j]);
		insert_leaf(levelPtr, phrase_data[i].pos, phrase_data[i].freq);
	}
}

void write_phrase_data()
{
	FILE *dict_file;
	PhraseData *cur_phr, *last_phr;
	int i, j;

	dict_file = fopen(DICT_FILE, "wb");

	if (!dict_file) {
		fprintf(stderr, "Cannot open output file.\n");
		exit(-1);
	}

	/*
	 * Duplicate Chinese strings are detected and not written into system
	 * dictionary. Written phrases are separated by '\0', for convenience of
	 * mmap usage.
	 */
	for (i = j = 0; i < num_word_data || j < num_phrase_data; last_phr = cur_phr) {
		if (i == num_word_data)
			cur_phr = &phrase_data[j++];
		else if (j == num_phrase_data)
			cur_phr = &word_data[i++].text;
		else
			cur_phr = strcmp(word_data[i].text.phrase, phrase_data[j].phrase)<0
				? &word_data[i++].text
				: &phrase_data[j++];

		if (last_phr && !strcmp(cur_phr->phrase, last_phr->phrase))
			cur_phr->pos = last_phr->pos;
		else {
			cur_phr->pos = ftell(dict_file);
			fwrite(cur_phr->phrase, strlen(cur_phr->phrase)+1, 1, dict_file);
		}
	}

	fclose(dict_file);
}

/*
 * This function performs BFS to compute child.begin and child.end of each node.
 * It sponteneously converts tree structure into a linked list. Writing the tree
 * into index file is then implemented by pure sequential traversal.
 */
void write_index_tree()
{
	/* (Circular) queue implementation is hidden within this function. */
	NODE **queue, *p, *pNext;
	int head=0, tail=0, tree_size=1;

	FILE *output = fopen(PHONE_TREE_FILE, "wb");

	if ( ! output ) {
		fprintf( stderr, "Error opening file " PHONE_TREE_FILE " for output.\n" );
		exit( 1 );
	}

	queue = ALC(NODE*, num_phrase_data+1);
	assert( queue );

	queue[head++]=root;
	while (head != tail) {
		p = queue[tail++];
		if (tail >= num_phrase_data)
			tail = 0;
		if (p->data.key != 0) {
			p->data.child.begin = tree_size;

			/*
			 * The latest inserted element must have a NULL
			 * pNextSibling value, and the following code let
			 * it point to the next child list to serialize
			 * them.
			 */
			if (head == 0)
				queue[num_phrase_data-1]->pNextSibling = p->pFirstChild;
			else
				queue[head-1]->pNextSibling = p->pFirstChild;

			for (pNext = p->pFirstChild; pNext; pNext = pNext->pNextSibling) {
				queue[head++] = pNext;
				if (head == num_phrase_data)
					head = 0;
				tree_size++;
			}

			p->data.child.end = tree_size;
		}
	}
	root->data.key = tree_size;

	for (p = root; p; p = pNext) {
		fwrite(&p->data, sizeof(TreeType), 1, output);
		pNext = p->pNextSibling;
		free(p);
	}
	free(queue);

	fclose( output );
}

int main(int argc, char *argv[])
{
	if (argc != 3) {
		printf(USAGE, argv[0]);
		return -1;
	}

	read_phone_cin(argv[1]);
	read_tsi_src(argv[2]);
	write_phrase_data();
	construct_phrase_tree();
	write_index_tree();
	return 0;
}

