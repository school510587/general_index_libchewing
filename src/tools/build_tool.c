/**
 * build_tool.c
 *
 * Copyright (c) 2013
 *      libchewing Core Team. See ChangeLog for details.
 *
 * See the file "COPYING" for information on usage and redistribution
 * of this file.
 */

/**
 * @file build_tool.c
 *
 * @brief cin reader and tree constructor.
 *
 *	cin reader reads IM definition file.\n
 *	Tree constructor generates a index-tree file.
 */

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "build_tool.h"
#include "private.h" /* For ALC macro. */

/* Important tokens in a cin file. */
#define ENAME            "%ename"
#define CHARDEF		 "%chardef"
#define BEGIN		   "begin"
#define END		     "end"

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

/* word_data and phrase_data can be referenced from outside (extern). */
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
static int compare_word_by_phone(const void *x, const void *y)
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

static void store_word(const char *line, const int line_num, EncFunct encode)
{
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
	word_data[num_word_data].text.phone[0] = encode(key_buf);

	word_data[num_word_data].index = num_word_data;
	++num_word_data;
}

void read_IM_cin(const char *filename, char *IM_name, EncFunct encode)
{
	FILE *phone_cin;
	char buf[MAX_LINE_LEN];
	char *ret;
	int line_num = 0;
	enum{INIT, HAS_CHARDEF_BEGIN, HAS_CHARDEF_END} status;

	assert( encode );

	phone_cin = fopen(filename, "r");
	if (!phone_cin) {
		fprintf(stderr, "Error opening the file %s\n", filename);
		exit(-1);
	}

	for(status=INIT; status!=HAS_CHARDEF_END; )
	{
		ret = fgets(buf, sizeof(buf), phone_cin);
		++line_num;
		if (!ret) {
			fprintf(stderr, "Cannot find %s %s\n", CHARDEF, BEGIN);
			exit(-1);
		}

		strip( buf );
		if( buf[0]=='%') {
			ret = strtok(buf, " \t");
			if(!strcmp(ret, ENAME)) {
				if( IM_name ) {
					ret = strtok(NULL, " \t");
					if(ret) strcpy(IM_name, ret);
				}
			}
			else if(!strcmp(ret, CHARDEF)) {
				ret = strtok(NULL, " \t");
				switch(status)
				{
					case INIT:
						if(!strcmp(ret, BEGIN))
							status=HAS_CHARDEF_BEGIN;
						else {
							fprintf(stderr, "%s %s is expected.\n", CHARDEF, BEGIN);
							exit(-1);
						}
					break;
					case HAS_CHARDEF_BEGIN:
						if(!strcmp(ret, END))
							status=HAS_CHARDEF_END;
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
		else if(status == HAS_CHARDEF_BEGIN)
			store_word(buf, line_num, encode);
	}
	fclose(phone_cin);

	qsort(word_data, num_word_data, sizeof(word_data[0]), compare_word_by_text);
}

static NODE *new_node( uint32_t key )
{
	NODE *pnew = ALC(NODE, 1);

	if(pnew == NULL){
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
static NODE *find_or_insert(NODE *parent, uint32_t key)
{
	NODE *prev=NULL, *p, *pnew;

	for(p=parent->pFirstChild; p!=NULL && p->data.key <= key; prev = p, p = p->pNextSibling)
		if(p->data.key == key) return p;
	pnew = new_node( key );
	pnew->pNextSibling = p;
	if(prev == NULL)
		parent->pFirstChild = pnew;
	else
		prev->pNextSibling = pnew;
	pnew->pNextSibling = p;
	return pnew;
}

static void insert_leaf(NODE *parent, long phr_pos, int freq)
{
	NODE *prev=NULL, *p, *pnew;

	for(p=parent->pFirstChild; p!=NULL && p->data.key == 0; prev = p, p = p->pNextSibling)
		if(p->data.phrase.freq <= freq) break;

	pnew = new_node(0);
	pnew->data.phrase.pos = (uint32_t)phr_pos;
	pnew->data.phrase.freq = freq;
	if(prev == NULL)
		parent->pFirstChild = pnew;
	else
		prev->pNextSibling = pnew;
	pnew->pNextSibling = p;
}

static void construct_phrase_tree()
{
	NODE *levelPtr;
	int i, j;

	/* First, assume that words are in order of their phones and indices. */
	qsort(word_data, num_word_data, sizeof(word_data[0]), compare_word_by_phone);

	/* Key value of root will become tree_size later. */
	root = new_node( 1 );

	/* Second, insert word_data as the first level of children. */
	for(i = 0; i < num_word_data; i++) {
		if(i == 0 || word_data[i].text.phone[0] != word_data[i-1].text.phone[0]) {
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
	for(i = 0; i < num_phrase_data; ++i)
	{
		levelPtr=root;
		for(j=0; phrase_data[i].phone[j]!=0; ++j)
			levelPtr=find_or_insert(levelPtr, phrase_data[i].phone[j]);
		insert_leaf(levelPtr, phrase_data[i].pos, phrase_data[i].freq);
	}
}

/*
 * This function performs BFS to compute child.begin and child.end of each node.
 * It sponteneously converts tree structure into a linked list. Writing the tree
 * into index file is then implemented by pure sequential traversal.
 */
void write_index_tree(const char *filename)
{
	/* (Circular) queue implementation is hidden within this function. */
	NODE **queue, *p, *pNext;
	int head=0, tail=0, tree_size=1;

	assert( filename );
	FILE *output = fopen(filename, "wb");

	if ( ! output ) {
		fprintf( stderr, "Error opening file " PHONE_TREE_FILE " for output.\n" );
		exit( 1 );
	}

	construct_phrase_tree();

	queue = ALC(NODE*, num_phrase_data+1);
	assert( queue );

	queue[head++]=root;
	while(head!=tail){
		p = queue[tail++];
		if(tail >= num_phrase_data) tail = 0;
		if(p->data.key != 0)
		{
			p->data.child.begin = tree_size;

			/*
			 * The latest inserted element must have a NULL
			 * pNextSibling value, and the following code let
			 * it point to the next child list to serialize
			 * them.
			 */
			if(head == 0)
				queue[num_phrase_data-1]->pNextSibling = p->pFirstChild;
			else
				queue[head-1]->pNextSibling = p->pFirstChild;

			for(pNext=p->pFirstChild; pNext!=NULL; pNext=pNext->pNextSibling)
			{
				queue[head++]=pNext;
				if(head == num_phrase_data) head = 0;
				tree_size++;
			}

			p->data.child.end = tree_size;
		}
	}
	root->data.key = tree_size;

	for(p=root; p!=NULL; p=pNext)
	{
		fwrite(&p->data, sizeof(TreeType), 1, output);
		pNext = p->pNextSibling;
		free(p);
	}
	free(queue);

	fclose( output );
}
