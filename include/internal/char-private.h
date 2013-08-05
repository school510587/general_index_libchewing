/**
 * char-private.h
 *
 * Copyright (c) 2008
 *	libchewing Core Team. See ChangeLog for details.
 *
 * See the file "COPYING" for information on usage and redistribution
 * of this file.
 */

#ifndef _CHEWING_CHAR_PRIVATE_H
#define _CHEWING_CHAR_PRIVATE_H

#include "global.h"
#include "chewing-private.h"

int GetCharFirst( ChewingData *, Phrase *, uint16_t );
int InitChar( ChewingData *pgdata, const char * prefix );
void TerminateChar( ChewingData *pgdata );

#endif
