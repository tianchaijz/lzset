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
 *   * Neither the name of Redis nor the names of its contributors may be used
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

/* This skip list implementation is almost a C translation of the original
 * algorithm described by William Pugh in "Skip Lists: A Probabilistic
 * Alternative to Balanced Trees", modified in three ways:
 * a) this implementation allows for repeated scores.
 * b) the comparison is not just by key (our 'score') but by satellite data.
 * c) there is a back pointer, so it's a doubly linked list with the back
 * pointers being only at "level 1". This allows to traverse the list
 * from tail to head, useful to model certain problems.
 *
 * This implementation originates from the Redis code base but was modified
 * in different ways. */


/* commit: 6e15592 */


#include <math.h>
#include <stdlib.h>

#include "skiplist.h"


/* Create a skip list node with the specified number of levels, pointing to
 * the specified object. */
skiplistNode *skiplistCreateNode(int level, double score, void *obj) {
    skiplistNode *zn = malloc(sizeof(*zn)+level*sizeof(struct skiplistLevel));
    zn->obj = obj;
    zn->score = score;
    return zn;
}


void skiplistInit(skiplist *sl, int (*compare)(const void *, const void *), void (*release)(void *)) {
    int j;

    sl->level = 1;
    sl->length = 0;
    sl->header = skiplistCreateNode(SKIPLIST_MAXLEVEL,0,NULL);
    for (j = 0; j < SKIPLIST_MAXLEVEL; j++) {
        sl->header->level[j].forward = NULL;
        sl->header->level[j].span = 0;
    }
    sl->header->backward = NULL;
    sl->tail = NULL;
    sl->compare = compare;
    sl->release = release;
}

/* Create a new skip list with the specified function used in order to
 * compare elements. The function return value is the same as strcmp(). */
skiplist *skiplistCreate(int (*compare)(const void *, const void *), void (*release)(void *)) {
    skiplist *sl = malloc(sizeof(*sl));
    skiplistInit(sl,compare,release);
    return sl;
}

/* Free a skiplist node. */
static inline void skiplistDoFreeNode(skiplistNode *node) {
    free(node);
}

/* Free a skiplist node and the node's pointed object when needed. */
void skiplistFreeNode(skiplist *sl, skiplistNode *node) {
    if (sl->release)
        sl->release(node->obj);
    skiplistDoFreeNode(node);
}

/* Free an skiplist nodes. */
void skiplistFreeNodes(skiplist *sl) {
    skiplistNode *node = sl->header->level[0].forward, *next;

    free(sl->header);
    while(node) {
        next = node->level[0].forward;
        skiplistFreeNode(sl,node);
        node = next;
    }
}

/* Free an entire skiplist. */
void skiplistFree(skiplist *sl) {
    skiplistFreeNodes(sl);
    free(sl);
}

/* Returns a random level for the new skiplist node we are going to create.
 * The return value of this function is between 1 and SKIPLIST_MAXLEVEL
 * (both inclusive), with a powerlaw-alike distribution where higher
 * levels are less likely to be returned. */
int skiplistRandomLevel(void) {
    int level = 1;
    while ((random()&0xFFFF) < (SKIPLIST_P * 0xFFFF))
        level += 1;
    return (level<SKIPLIST_MAXLEVEL) ? level : SKIPLIST_MAXLEVEL;
}

/* Insert the specified object, return NULL if the element already
 * exists. */
skiplistNode *skiplistInsert(skiplist *sl, double score, void *obj) {
    skiplistNode *update[SKIPLIST_MAXLEVEL], *x;
    unsigned int rank[SKIPLIST_MAXLEVEL];
    int i, level;

    x = sl->header;
    for (i = sl->level-1; i >= 0; i--) {
        /* store rank that is crossed to reach the insert position */
        rank[i] = i == (sl->level-1) ? 0 : rank[i+1];
        while (x->level[i].forward &&
            (x->level[i].forward->score < score ||
               (x->level[i].forward->score == score &&
                sl->compare(x->level[i].forward->obj,obj) < 0)))
        {
            rank[i] += x->level[i].span;
            x = x->level[i].forward;
        }
        update[i] = x;
    }

    /* we assume the key is not already inside, since we allow duplicated
     * scores, and the re-insertion of score and redis object should never
     * happen since the caller of slInsert() should test in the hash table
     * if the element is already inside or not. */

    /* If the element is already inside, return NULL. */
    if (x->level[0].forward &&
        sl->compare(x->level[0].forward->obj,obj) == 0) return NULL;

    /* Add a new node with a random number of levels. */
    level = skiplistRandomLevel();
    if (level > sl->level) {
        for (i = sl->level; i < level; i++) {
            rank[i] = 0;
            update[i] = sl->header;
            update[i]->level[i].span = sl->length;
        }
        sl->level = level;
    }
    x = skiplistCreateNode(level,score,obj);
    for (i = 0; i < level; i++) {
        x->level[i].forward = update[i]->level[i].forward;
        update[i]->level[i].forward = x;

        /* update span covered by update[i] as x is inserted here */
        x->level[i].span = update[i]->level[i].span - (rank[0] - rank[i]);
        update[i]->level[i].span = (rank[0] - rank[i]) + 1;
    }

    /* increment span for untouched levels */
    for (i = level; i < sl->level; i++) {
        update[i]->level[i].span++;
    }

    x->backward = (update[0] == sl->header) ? NULL : update[0];
    if (x->level[0].forward)
        x->level[0].forward->backward = x;
    else
        sl->tail = x;
    sl->length++;
    return x;
}

/* Internal function used by skiplistDelete, it needs an array of other
 * skiplist nodes that point to the node to delete in order to update
 * all the references of the node we are going to remove. */
void skiplistDeleteNode(skiplist *sl, skiplistNode *x, skiplistNode **update) {
    int i;
    for (i = 0; i < sl->level; i++) {
        if (update[i]->level[i].forward == x) {
            update[i]->level[i].span += x->level[i].span - 1;
            update[i]->level[i].forward = x->level[i].forward;
        } else {
            update[i]->level[i].span -= 1;
        }
    }
    if (x->level[0].forward) {
        x->level[0].forward->backward = x->backward;
    } else {
        sl->tail = x->backward;
    }
    while(sl->level > 1 && sl->header->level[sl->level-1].forward == NULL)
        sl->level--;
    sl->length--;
}

/* Delete an element with matching score/object from the skiplist.
 * 1 is returned, otherwise if the element was not there, 0 is returned. */
int skiplistDelete(skiplist *sl, double score, void *obj) {
    skiplistNode *update[SKIPLIST_MAXLEVEL], *x;
    int i;

    x = sl->header;
    for (i = sl->level-1; i >= 0; i--) {
        while (x->level[i].forward &&
            (x->level[i].forward->score < score ||
                (x->level[i].forward->score == score &&
                 sl->compare(x->level[i].forward->obj,obj) < 0)))
        {
            x = x->level[i].forward;
        }
        update[i] = x;
    }
    x = x->level[0].forward;
    if (x && sl->compare(x->obj,obj) == 0) {
        skiplistDeleteNode(sl,x,update);
        skiplistFreeNode(sl,x);
        return 1;
    }
    return 0; /* not found */
}

/* Update the score of an object inside the sorted set skiplist.
 * Note that the object must exist and must match 'score'.
 * This function does not update the score in the hash table side, the
 * caller should take care of it.
 *
 * Note that this function attempts to just update the node, in case after
 * the score update, the node would be exactly at the same position.
 * Otherwise the skiplist is modified by removing and re-adding a new
 * object, which is more costly.
 *
 * The function returns the updated object skiplist node pointer. */
skiplistNode *skiplistUpdateScore(skiplist *sl, double curscore, void *obj, double newscore) {
    skiplistNode *update[SKIPLIST_MAXLEVEL], *x;
    int i;

    /* We need to seek to object to update to start: this is useful anyway,
     * we'll have to update or remove it. */
    x = sl->header;
    for (i = sl->level-1; i >= 0; i--) {
        while (x->level[i].forward &&
                (x->level[i].forward->score < curscore ||
                    (x->level[i].forward->score == curscore &&
                     sl->compare(x->level[i].forward->obj,obj) < 0)))
        {
            x = x->level[i].forward;
        }
        update[i] = x;
    }

    /* Jump to our object: note that this function assumes that the
     * object with the matching score exists. */
    x = x->level[0].forward;
    // serverAssert(x && curscore == x->score && sl->compare(x->obj,obj) == 0);

    /* If the node, after the score update, would be still exactly
     * at the same position, we can just update the score without
     * actually removing and re-inserting the object in the skiplist. */
    if ((x->backward == NULL || x->backward->score < newscore) &&
        (x->level[0].forward == NULL || x->level[0].forward->score > newscore))
    {
        x->score = newscore;
        return x;
    }

    /* No way to reuse the old node: we need to remove and insert a new
     * one at a different place. */
    skiplistDeleteNode(sl,x,update);
    skiplistNode *newnode = skiplistInsert(sl,newscore,x->obj);
    /* We reused the old node x->obj object, free the node now
     * since zslInsert created a new one. */
    x->obj = NULL;
    /* Only free the node self, since its pointed object is moved. */
    skiplistDoFreeNode(x);
    return newnode;
}


/* Search for the element in the skip list, if found the
 * node pointer is returned, otherwise NULL is returned. */
void *skiplistFind(skiplist *sl, void *obj) {
    skiplistNode *x;
    int i;

    x = sl->header;
    for (i = sl->level-1; i >= 0; i--) {
        while (x->level[i].forward &&
               sl->compare(x->level[i].forward->obj,obj) < 0)
        {
            x = x->level[i].forward;
        }
    }
    x = x->level[0].forward;
    if (x && sl->compare(x->obj,obj) == 0) {
        return x;
    } else {
        return NULL;
    }
}

/* If the skip list is empty, NULL is returned, otherwise the element
 * at head is removed and its pointed object returned. */
void *skiplistPopHead(skiplist *sl) {
    skiplistNode *x = sl->header;

    x = x->level[0].forward;
    if (!x) return NULL;
    void *obj = x->obj;
    skiplistDelete(sl,x->score,obj);
    return obj;
}

/* If the skip list is empty, NULL is returned, otherwise the element
 * at tail is removed and its pointed object returned. */
void *skiplistPopTail(skiplist *sl) {
    skiplistNode *x = sl->tail;

    if (!x) return NULL;
    void *obj = x->obj;
    skiplistDelete(sl,x->score,obj);
    return obj;
}

unsigned long skiplistLength(skiplist *sl) {
    return sl->length;
}

/* Delete all the elements with rank between start and end from the skiplist.
 * Start and end are inclusive. Note that start and end need to be 1-based */
unsigned long skiplistDeleteRangeByRank(skiplist *sl, unsigned int start, unsigned int end, skiplistDeleteCb cb, void *ctx) {
    skiplistNode *update[SKIPLIST_MAXLEVEL], *x;
    unsigned long traversed = 0, removed = 0;
    int i;

    if (start > sl->length || end < 1)
        return 0;

    x = sl->header;
    for (i = sl->level-1; i >= 0; i--) {
        while (x->level[i].forward && (traversed + x->level[i].span) < start) {
            traversed += x->level[i].span;
            x = x->level[i].forward;
        }
        update[i] = x;
    }

    traversed++;
    x = x->level[0].forward;
    while (x && traversed <= end) {
        skiplistNode *next = x->level[0].forward;
        skiplistDeleteNode(sl,x,update);
        cb(ctx,x->obj);
        skiplistFreeNode(sl,x);
        removed++;
        traversed++;
        x = next;
    }
    return removed;
}

/* Find the rank for an element by both score and key.
 * Returns 0 when the element cannot be found, rank otherwise.
 * Note that the rank is 1-based due to the span of sl->header to the
 * first element. */
unsigned long skiplistGetRank(skiplist *sl, double score, void *obj) {
    skiplistNode *x;
    unsigned long rank = 0;
    int i;

    x = sl->header;
    for (i = sl->level-1; i >= 0; i--) {
        while (x->level[i].forward &&
            (x->level[i].forward->score < score ||
                (x->level[i].forward->score == score &&
                 sl->compare(x->level[i].forward->obj,obj) <= 0))) {
            rank += x->level[i].span;
            x = x->level[i].forward;
        }

        /* x might be equal to sl->header, so test if obj is non-NULL */
        if (x->obj && sl->compare(x->obj,obj) == 0) {
            return rank;
        }
    }
    return 0;
}

static inline int skiplistValueGteMin(double value, double min, int minex) {
    return minex ? (value > min) : (value >= min);
}

static inline int skiplistValueLteMax(double value, double max, int maxex) {
    return maxex ? (value < max) : (value <= max);
}

/* Return the rank of given score. */
unsigned long skiplistGetScoreRank(skiplist *sl, double score, int ex) {
    skiplistNode *x;
    unsigned long rank = 0;
    int i;

    x = sl->header;
    for (i = sl->level-1; i >= 0; i--) {
        while (x->level[i].forward &&
               skiplistValueLteMax(x->level[i].forward->score,score,ex)) {
            rank += x->level[i].span;
            x = x->level[i].forward;
        }
    }
    return rank;
}

/* Finds an element by its rank. The rank argument needs to be 1-based. */
skiplistNode* skiplistGetNodeByRank(skiplist *sl, unsigned long rank) {
    skiplistNode *x;
    unsigned long traversed = 0;
    int i;

    if (rank == 0 || rank > sl->length)
        return NULL;

    x = sl->header;
    for (i = sl->level-1; i >= 0; i--) {
        while (x->level[i].forward && (traversed + x->level[i].span) <= rank)
        {
            traversed += x->level[i].span;
            x = x->level[i].forward;
        }
        if (traversed == rank) {
            return x;
        }
    }
    return NULL;
}

/* Returns if there is a part of the zset is in range. */
int skiplistIsInRange(skiplist *sl, double min, double max, int minex, int maxex) {
    skiplistNode *x;

    /* Test for ranges that will always be empty. */
    if (min > max || (min == max && (minex || maxex)))
        return 0;
    x = sl->tail;
    if (x == NULL || !skiplistValueGteMin(x->score,min,minex))
        return 0;
    x = sl->header->level[0].forward;
    if (x == NULL || !skiplistValueLteMax(x->score,max,maxex))
        return 0;
    return 1;
}

/* Find the first node that is contained in the specified range.
 * Returns NULL when no element is contained in the range. */
skiplistNode *skiplistFirstInRange(skiplist *sl, double min, double max, int minex, int maxex) {
    skiplistNode *x;
    int i;

    /* If everything is out of range, return early. */
    if (!skiplistIsInRange(sl,min,max,minex,maxex)) return NULL;

    x = sl->header;
    for (i = sl->level-1; i >= 0; i--) {
        /* Go forward while *OUT* of range. */
        while (x->level[i].forward &&
            !skiplistValueGteMin(x->level[i].forward->score,min,minex))
                x = x->level[i].forward;
    }

    /* This is an inner range, so the next node cannot be NULL. */
    x = x->level[0].forward;
    // serverAssert(x != NULL);

    /* Check if score <= max. */
    if (!skiplistValueLteMax(x->score,max,maxex)) return NULL;
    return x;
}

/* Find the last node that is contained in the specified range.
 * Returns NULL when no element is contained in the range. */
skiplistNode *skiplistLastInRange(skiplist *sl, double min, double max, int minex, int maxex) {
    skiplistNode *x;
    int i;

    /* If everything is out of range, return early. */
    if (!skiplistIsInRange(sl,min,max,minex,maxex)) return NULL;

    x = sl->header;
    for (i = sl->level-1; i >= 0; i--) {
        /* Go forward while *IN* range. */
        while (x->level[i].forward &&
            skiplistValueLteMax(x->level[i].forward->score,max,maxex))
                x = x->level[i].forward;
    }

    /* This is an inner range, so this node cannot be NULL. */
    // serverAssert(x != NULL);

    /* Check if score >= min. */
    if (!skiplistValueGteMin(x->score,min,minex)) return NULL;
    return x;
}

void skiplistIterate(skiplist *sl, void *ctx, int (*iterator)(void *ctx, int index, double score, void *obj)) {
    skiplistNode *x;
    int i;

    x = sl->header;
    i = 0;
    while (x->level[0].forward) {
        x = x->level[0].forward;
        i++;
        if (!iterator(ctx,i,x->score,x->obj))
            return;
    }
}
