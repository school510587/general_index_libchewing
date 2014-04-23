/**
 * build_tool.h
 *
 * Copyright (c) 2014
 *      libchewing Core Team. See ChangeLog for details.
 *
 * See the file "COPYING" for information on usage and redistribution
 * of this file.
 */

/* *INDENT-OFF* */
#ifndef _CHEWING_BUILD_TOOL_PRIVATE_H
#define _CHEWING_BUILD_TOOL_PRIVATE_H
/* *INDENT-ON* */

#include "chewing-private.h"

#define MAX_LINE_LEN          (1024)
#define MAX_WORD_DATA         (60000)
#define MAX_PHRASE_BUF_LEN    (149)
#define MAX_PHRASE_DATA       (420000)

/**
 * @struct PhraseData
 * @brief Record of each phrase in tsi.src
 * An additional pos helps avoid duplicate Chinese strings.
 */
typedef struct {
    char phrase[MAX_PHRASE_BUF_LEN];
    uint32_t freq;
    uint16_t phone[MAX_PHRASE_LEN + 1];
    long pos;
} PhraseData;

/**
 * @struct WordData
 * @brief Record of each word in a cin file.
 * Text of each element in word_data points to one element in phrase_data.
 */
typedef struct {
    /** @brief Common part shared with PhraseData. */
    PhraseData *text;
    /** @brief For stable sorting. */
    int index;
} WordData;

extern WordData word_data[MAX_WORD_DATA];
extern int num_word_data;

extern PhraseData phrase_data[MAX_PHRASE_DATA];
extern int num_phrase_data;
extern int top_phrase_data;

/**
 * @brief Strip leading whitespace and trailing comment/whitespace.
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
 */
void read_IM_cin(const char *filename);

/**
 * @brief Index tree writer.
 */
void write_index_tree();

/* *INDENT-OFF* */
#endif
/* *INDENT-ON* */
