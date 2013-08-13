#ifndef BUILD_TOOL_H
#define BUILD_TOOL_H

#include "chewing-private.h"
#include "chewing-utf8-util.h"
#include "global-private.h"
#include "key2pho-private.h"
#include "zuin-private.h"

#define MAX_LINE_LEN        (1024)
#define MAX_WORD_DATA      (60000)
#define MAX_PHRASE_BUF_LEN      (149)
#define MAX_PHRASE_DATA  (420000)

/* An additional pos helps avoid duplicate Chinese strings. */
typedef struct {
	char phrase[MAX_PHRASE_BUF_LEN];
	int freq;
	uint32_t phone[MAX_PHRASE_LEN + 1];
	long pos;
} PhraseData;
typedef struct {
	PhraseData text; // Common part shared with PhraseData. */
	int index; /* For stable sorting. */
} WordData;

extern WordData word_data[MAX_WORD_DATA];
extern int num_word_data;

extern PhraseData phrase_data[MAX_PHRASE_DATA];
extern int num_phrase_data;

void strip(char *line);
void read_IM_cin(const char *filename);
void write_index_tree();

#endif
