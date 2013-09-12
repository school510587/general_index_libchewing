/*
 * global.h
 *
 * Copyright (c) 1999, 2000, 2001
 *	Lu-chuan Kung and Kang-pen Chen.
 *	All rights reserved.
 *
 * Copyright (c) 2004, 2005, 2006, 2008, 2011
 *	libchewing Core Team. See ChangeLog for details.
 *
 * See the file "COPYING" for information on usage and redistribution
 * of this file.
 */

#ifndef _CHEWING_GLOBAL_H
#define _CHEWING_GLOBAL_H

/*! \file global.h
 *  \brief Chewing Global Definitions
 *  \author libchewing Core Team
*/

#define CHINESE_MODE 1
#define SYMBOL_MODE 0
#define FULLSHAPE_MODE 1
#define HALFSHAPE_MODE 0

/* specified to Chewing API */
#if defined(_WIN32) || defined(_WIN64) || defined(_WIN32_WCE)
#   define CHEWING_DLL_IMPORT __declspec(dllimport)
#   define CHEWING_DLL_EXPORT __declspec(dllexport)
#   ifdef CHEWINGDLL_EXPORTS
#      define CHEWING_API CHEWING_DLL_EXPORT
#      define CHEWING_PRIVATE
#   elif CHEWINGDLL_IMPORTS
#      define CHEWING_API CHEWING_DLL_IMPORT
#      define CHEWING_PRIVATE
#   else
#      define CHEWING_API
#      define CHEWING_PRIVATE
#   endif
#elif (__GNUC__ > 3) && (defined(__ELF__) || defined(__PIC__))
#   define CHEWING_API __attribute__((__visibility__("default")))
#   define CHEWING_PRIVATE __attribute__((__visibility__("hidden")))
#else
#   define CHEWING_API
#   define CHEWING_PRIVATE
#endif

#ifndef UNUSED
#if defined(__GNUC__) /* gcc specific */
#   define UNUSED __attribute__((unused))
#else
#   define UNUSED
#endif
#endif

#define MIN_SELKEY 1
#define MAX_SELKEY 10

#define CHEWING_LOG_VERBOSE 1
#define CHEWING_LOG_DEBUG   2
#define CHEWING_LOG_INFO    3
#define CHEWING_LOG_WARN    4
#define CHEWING_LOG_ERROR   5

/**
 * @deprecated Use chewing_set_ series of functions to set parameters instead.
 */
typedef struct {
	int candPerPage;
	int maxChiSymbolLen;
	int selKey[ MAX_SELKEY ];
	int bAddPhraseForward;
	int bSpaceAsSelection;
	int bEscCleanAllBuf;
	int bAutoShiftCur;
	int bEasySymbolInput;
	int bPhraseChoiceRearward;
	int hsuSelKeyType; // Deprecated.
} ChewingConfigData;

typedef struct {
	/*@{*/
	int from;	/**< starting position of certain interval */
	int to;		/**< ending position of certain interval */
	/*@}*/
} IntervalType;

/** @brief context handle used for Chewing IM APIs
 */
typedef struct _ChewingContext ChewingContext;

/** @brief use "asdfjkl789" as selection key
 */
#define HSU_SELKEY_TYPE1 1

/** @brief use "asdfzxcv89" as selection key
 */
#define HSU_SELKEY_TYPE2 2

/**
 * @brief Record of a key-in sequence corresponding to a word.
 *        To enable multi-IM functions, use --enable-multiple-IM.
 */
#ifdef SUPPORT_MULTI_IM
typedef unsigned long KeySeqWord;
#else
typedef unsigned short KeySeqWord;
#endif

#endif
