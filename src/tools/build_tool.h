#ifndef BUILD_TOOL_H
#define BUILD_TOOL_H

#include "chewing-private.h"
#include "global.h"
#include "key2pho-private.h"

#define MAX_LINE_LEN        (1024)
#define MAX_WORD_DATA      (60000)
#define MAX_PHRASE_BUF_LEN      (149)
#define MAX_PHRASE_DATA  (420000)

/**
 * @brief Record of each phrase in tsi.src
 *
 *      An additional pos helps avoid duplicate Chinese strings.
 */
typedef struct {
	char phrase[MAX_PHRASE_BUF_LEN];
	int freq;
	KeySeqWord phone[MAX_PHRASE_LEN + 1];
	long pos;
} PhraseData;

/**
 * @brief Record of each word in a cin file.
 *
 *      text of each element in word_data points to one element in phrase_data.
 */
typedef struct {
	PhraseData *text; /* Common part shared with PhraseData. */
	int index; /* For stable sorting. */
} WordData;

extern WordData word_data[MAX_WORD_DATA];
extern int num_word_data;

extern PhraseData phrase_data[MAX_PHRASE_DATA];
extern int num_phrase_data;
extern int top_phrase_data;

/**
 * @brief Strip reading whitespace and trailing comment / whitespace.
 * @param line A buffer containing the line.
 */
void strip(char *line);

/**
 * @brief Comparison between 2 words first by string and second by phone.
 * @param x, y Two compared words.
 */
int compare_word_by_text(const void *x, const void *y);

/**
 * @brief IM cin reader. Result word_data is sorted by compare_word_by_text.
 * @param filename The path of cin file.
 * @param IM_name  Buffer for name of the IM. It can be NULL.
 */
#ifdef SUPPORT_MULTI_IM
void read_IM_cin(const char *filename, char *IM_name, EncFunct encode);
#else
void read_IM_cin(const char *filename);
#endif

/**
 * @brief Index tree writer.
 * @param filename Path for output file.
 */
void write_index_tree(const char *filename);

#endif
