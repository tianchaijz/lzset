/*
 * Copyright (c) 2009-2014, Salvatore Sanfilippo <antirez at gmail dot com>
 * Copyright (c) 2009-2014, Pieter Noordhuis <pcnoordhuis at gmail dot com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Disque nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/* http://blog.wjin.org/posts/redis-internal-data-structure-skiplist.html
 * Redis adds a backward pointer for each skip list node to traverse reversely.
 * Also there is a span variable in level entry to record how many nodes must be
 * crossed when reaching to next node. Actually, when traverse list, we can
 * accumulate span to get the rank of a node in sorted set. */


#ifndef __SKIPLIST_H
#define __SKIPLIST_H

#define SKIPLIST_MAXLEVEL 32 /* Should be enough for 2^32 elements */
#define SKIPLIST_P 0.25      /* Skiplist P = 1/4 */

typedef struct skiplistNode {
    void *obj;
    double score;
    struct skiplistNode *backward; // backward pointer, only exist in level zero list
    struct skiplistLevel {
        struct skiplistNode *forward; // next node, may skip a lot of nodes
        unsigned int span; // number of nodes need be crossed to reach to next node
    } level[];
} skiplistNode;

typedef struct skiplist {
    struct skiplistNode *header, *tail;
    int (*compare)(const void *, const void *);
    void (*release)(void *);
    unsigned long length; // number of nodes
    int level; // current level
} skiplist;

typedef void (*skiplistDeleteCb) (void *ctx, void *obj);

skiplist *skiplistCreate(int (*compare)(const void *, const void *), void (*release)(void *));
void skiplistInit(skiplist *sl, int (*compare)(const void *, const void *), void (*release)(void *));
void skiplistFree(skiplist *sl);
void skiplistFreeNodes(skiplist *sl);
skiplistNode *skiplistInsert(skiplist *sl, double score, void *obj);
int skiplistDelete(skiplist *sl, double score, void *obj);
skiplistNode *skiplistUpdateScore(skiplist *sl, double curscore, void *obj, double newscore);
void *skiplistFind(skiplist *sl, void *obj);
void *skiplistPopHead(skiplist *sl);
void *skiplistPopTail(skiplist *sl);
unsigned long skiplistLength(skiplist *sl);
unsigned long skiplistDeleteRangeByRank(skiplist *sl, unsigned int start, unsigned int end, skiplistDeleteCb cb, void *ctx);
unsigned long skiplistGetRank(skiplist *sl, double score, void *obj);
unsigned long skiplistGetScoreRank(skiplist *sl, double score, int ex);
skiplistNode* skiplistGetNodeByRank(skiplist *sl, unsigned long rank);
skiplistNode *skiplistFirstInRange(skiplist *sl, double min, double max, int minex, int maxex);
skiplistNode *skiplistLastInRange(skiplist *sl, double min, double max, int minex, int maxex);
void skiplistIterate(skiplist *sl, void *ctx, int (*iterator)(void *ctx, int index, double score, void *obj));


#endif
