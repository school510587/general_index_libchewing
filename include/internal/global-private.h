/**
 * @file global-private.h
 *
 * @brief internal definitions such as data filename.
 *
 * Copyright (c) 2008
 *	libchewing Core Team. See ChangeLog for details.
 *
 * See the file "COPYING" for information on usage and redistribution
 * of this file.
 */

#ifndef _CHEWING_GLOBAL_PRIVATE_H
#define _CHEWING_GLOBAL_PRIVATE_H

#ifdef SUPPORT_MULTI_IM
#  define FREQ_FILE     	"total_freq.dat"
#  define PHONETIC      	"Phonetic"
#  define INDEX_TREE_FILE	"_index_tree.dat"
#  define PHONE_TREE_FILE	PHONETIC INDEX_TREE_FILE
#else
#  define PHONE_TREE_FILE	"index_tree.dat"
#endif
#define DICT_FILE		"dictionary.dat"
#define SYMBOL_TABLE_FILE	"symbols.dat"
#define SOFTKBD_TABLE_FILE	"swkb.dat"
#define PINYIN_TAB_NAME         "pinyin.tab"

#endif
