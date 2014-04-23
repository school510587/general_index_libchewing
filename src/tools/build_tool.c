/**
 * build_tool.c
 *
 * Copyright (c) 2014
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
 *      cin reader reads IM definition file.\n
 *      Tree constructor generates a index-tree file.
 */

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "build_tool.h"

#include "global-private.h"
#include "key2pho-private.h"
#include "memory-private.h"
#include "bopomofo-private.h"

/* For ALC macro */
#include "private.h"

#define CHARDEF               "%chardef"
#define BEGIN                 "begin"
#define END                   "end"

/*
 * Please see TreeType for data field. pFirstChild points to the first of its
 * child list. pNextSibling points to its right sibling, where it and its right
 * sibling are both in the child list of its parent. However, pNextSibling will
 * become next-pointer like linked list, which makes writing of index-tree file
 * become a sequential traversal rather than BFS.
 */
typedef struct _tNODE {
    TreeType data;
    struct _tNODE *pFirstChild;
    struct _tNODE *pNextSibling;
} NODE;

/* word_data and phrase_data can be referenced from outside (extern). */
WordData word_data[MAX_WORD_DATA];
int num_word_data = 0;

PhraseData phrase_data[MAX_PHRASE_DATA];
int num_phrase_data = 0;
int top_phrase_data = MAX_PHRASE_DATA;

static NODE *root;

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
    while (end >= line && isspace((unsigned char) *end)) {
        *end = 0;
        --end;
    }
}

/* word_data is sorted reversely, for stack-like push operation. */
int compare_word_by_phone(const void *x, const void *y)
{
    const WordData *a = (const WordData *) x;
    const WordData *b = (const WordData *) y;

    if (a->text->phone[0] != b->text->phone[0])
        return b->text->phone[0] - a->text->phone[0];

    /* Compare original index for stable sort */
    return b->index - a->index;
}

int compare_word_by_text(const void *x, const void *y)
{
    const WordData *a = (const WordData *) x;
    const WordData *b = (const WordData *) y;
    int ret = strcmp(a->text->phrase, b->text->phrase);

    if (ret != 0)
        return ret;

    if (a->text->phone[0] != b->text->phone[0])
        return a->text->phone[0] - b->text->phone[0];

    return 0;
}

int compare_word_no_duplicated(const void *x, const void *y)
{
    int ret = compare_word_by_text(x, y);

    if (!ret) {
        const WordData *a = (const WordData *) x;

        fprintf(stderr, "Duplicated word found (`%s', %d).\n", a->text->phrase, a->text->phone[0]);
        exit(-1);
    }

    return ret;
}

void store_word(const char *line, const int line_num)
{
    char phone_buf[MAX_UTF8_SIZE * BOPOMOFO_SIZE + 1];
    char key_buf[BOPOMOFO_SIZE + 1];
    char buf[MAX_LINE_LEN + 1] = {0};

    strncpy(buf, line, sizeof(buf) - 1);

    strip(buf);
    if (strlen(buf) == 0)
        return;

    if (num_word_data >= MAX_WORD_DATA) {
        fprintf(stderr, "Need to increase MAX_WORD_DATA to process\n");
        exit(-1);
    }
    if (top_phrase_data <= num_phrase_data) {
        fprintf(stderr, "Need to increase MAX_PHRASE_DATA to process\n");
        exit(-1);
    }
    word_data[num_word_data].text = &phrase_data[--top_phrase_data];

#define UTF8_FORMAT_STRING(len1, len2) \
    "%" __stringify(len1) "[^ ]" " " \
    "%" __stringify(len2) "[^ ]"
    sscanf(buf, UTF8_FORMAT_STRING(BOPOMOFO_SIZE, MAX_UTF8_SIZE), key_buf, word_data[num_word_data].text->phrase);

    if (strlen(key_buf) > BOPOMOFO_SIZE) {
        fprintf(stderr, "Error reading line %d, `%s'\n", line_num, line);
        exit(-1);
    }
    PhoneFromKey(phone_buf, key_buf, KB_DEFAULT, 1);
    word_data[num_word_data].text->phone[0] = UintFromPhone(phone_buf);

    word_data[num_word_data].index = num_word_data;
    ++num_word_data;
}

void read_IM_cin(const char *filename)
{
    FILE *IM_cin;
    char buf[MAX_LINE_LEN];
    char *ret;
    int line_num = 0;
    enum { INIT, HAS_CHARDEF_BEGIN, HAS_CHARDEF_END } status;

    IM_cin = fopen(filename, "r");
    if (!IM_cin) {
        fprintf(stderr, "Error opening the file %s\n", filename);
        exit(-1);
    }

    for (status = INIT; status != HAS_CHARDEF_BEGIN;) {
        ret = fgets(buf, sizeof(buf), IM_cin);
        ++line_num;
        if (!ret) {
            fprintf(stderr, "%s: No expected %s %s\n", filename, CHARDEF, BEGIN);
            exit(-1);
        }

        strip(buf);
        ret = strtok(buf, " \t");
        if (!strcmp(ret, CHARDEF)) {
            ret = strtok(NULL, " \t");
            if (!strcmp(ret, BEGIN))
                status = HAS_CHARDEF_BEGIN;
            else {
                fprintf(stderr, "%s:%d: Unexpected %s %s\n", filename, line_num, CHARDEF, ret);
                exit(-1);
            }
        }
    }

    while (status != HAS_CHARDEF_END) {
        ret = fgets(buf, sizeof(buf), IM_cin);
        ++line_num;
        if (!ret) {
            fprintf(stderr, "%s: No expected %s %s\n", filename, CHARDEF, END);
            exit(-1);
        }

        strip(buf);
        if (!strncmp(buf, CHARDEF, strlen(CHARDEF))) {
            strtok(buf, " \t");
            ret = strtok(NULL, " \t");
            if (!strcmp(ret, END))
                status = HAS_CHARDEF_END;
            else {
                fprintf(stderr, "%s:%d: Unexpected %s %s\n", filename, line_num, CHARDEF, ret);
                exit(-1);
            }
        } else
            store_word(buf, line_num);
    }

    fclose(IM_cin);

    qsort(word_data, num_word_data, sizeof(word_data[0]), compare_word_no_duplicated);
}

static NODE *new_node(uint32_t key)
{
    NODE *pnew = ALC(NODE, 1);

    if (pnew == NULL) {
        fprintf(stderr, "Memory allocation failed on constructing phrase tree.\n");
        exit(-1);
    }

    memset(&pnew->data, 0, sizeof(pnew->data));
    PutUint16(key, pnew->data.key);
    pnew->pFirstChild = NULL;
    pnew->pNextSibling = NULL;
    return pnew;
}

/*
 * This function puts FindKey() and Insert() together. It first searches for the
 * specified key and performs FindKey() on hit. Otherwise, it inserts a new node
 * at proper position and returns the newly inserted child.
 */
NODE *find_or_insert(NODE * parent, uint32_t key)
{
    NODE *prev = NULL;
    NODE *p;
    NODE *pnew;

    for (p = parent->pFirstChild; p && GetUint16(p->data.key) <= key; prev = p, p = p->pNextSibling)
        if (GetUint16(p->data.key) == key)
            return p;

    pnew = new_node(key);
    pnew->pNextSibling = p;
    if (prev == NULL)
        parent->pFirstChild = pnew;
    else
        prev->pNextSibling = pnew;
    pnew->pNextSibling = p;
    return pnew;
}

static void insert_leaf(NODE * parent, long phr_pos, uint32_t freq)
{
    NODE *prev = NULL;
    NODE *p;
    NODE *pnew;

    for (p = parent->pFirstChild; p && GetUint16(p->data.key) == 0; prev = p, p = p->pNextSibling)
        if (GetUint16(p->data.phrase.freq) <= freq)
            break;

    pnew = new_node(0);
    PutUint24((uint32_t) phr_pos, pnew->data.phrase.pos);
    PutUint24(freq, pnew->data.phrase.freq);
    if (prev == NULL)
        parent->pFirstChild = pnew;
    else
        prev->pNextSibling = pnew;
    pnew->pNextSibling = p;
}

static void construct_phrase_tree()
{
    NODE *levelPtr;
    int i;
    int j;

    /* First, assume that words are in order of their phones and indices. */
    qsort(word_data, num_word_data, sizeof(word_data[0]), compare_word_by_phone);

    /* Key value of root will become tree_size later. */
    root = new_node(1);

    /* Second, insert word_data as the first level of children. */
    for (i = 0; i < num_word_data; i++) {
        if (i == 0 || word_data[i].text->phone[0] != word_data[i - 1].text->phone[0]) {
            levelPtr = new_node(word_data[i].text->phone[0]);
            levelPtr->pNextSibling = root->pFirstChild;
            root->pFirstChild = levelPtr;
        }
        levelPtr = new_node(0);
        PutUint24((uint32_t) word_data[i].text->pos, levelPtr->data.phrase.pos);
        PutUint24(word_data[i].text->freq, levelPtr->data.phrase.freq);
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

/*
 * This function performs BFS to compute child.begin and child.end of each node.
 * It sponteneously converts tree structure into a linked list. Writing the tree
 * into index file is then implemented by pure sequential traversal.
 */
void write_index_tree()
{
    /* (Circular) queue implementation is hidden within this function. */
    NODE **queue;
    NODE *p;
    NODE *pNext;
    size_t head = 0, tail = 0;
    size_t tree_size = 1;
    size_t q_len = num_word_data + num_phrase_data + 1;

    FILE *output = fopen(PHONE_TREE_FILE, "wb");

    if (!output) {
        fprintf(stderr, "Error opening file " PHONE_TREE_FILE " for output.\n");
        exit(-1);
    }

    construct_phrase_tree();

    queue = ALC(NODE *, q_len);
    assert(queue);

    queue[head++] = root;
    while (head != tail) {
        p = queue[tail++];
        if (tail >= q_len)
            tail = 0;
        if (GetUint16(p->data.key) != 0) {
            PutUint24(tree_size, p->data.child.begin);

            /*
             * The latest inserted element must have a NULL
             * pNextSibling value, and the following code let
             * it point to the next child list to serialize
             * them.
             */
            if (head == 0)
                queue[q_len - 1]->pNextSibling = p->pFirstChild;
            else
                queue[head - 1]->pNextSibling = p->pFirstChild;

            for (pNext = p->pFirstChild; pNext; pNext = pNext->pNextSibling) {
                queue[head++] = pNext;
                if (head == q_len)
                    head = 0;
                tree_size++;
            }

            PutUint24(tree_size, p->data.child.end);
        }
    }
    PutUint16(tree_size, root->data.key);

    for (p = root; p; p = pNext) {
        fwrite(&p->data, sizeof(TreeType), 1, output);
        pNext = p->pNextSibling;
        free(p);
    }
    free(queue);

    fclose(output);
}
