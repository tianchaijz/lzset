#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lauxlib.h"
#include "lua.h"
#include "skiplist.h"

#define lzset_lua_newlibtable(L, l) \
    lua_createtable(L, 0, sizeof(l) / sizeof((l)[0]) - 1)

typedef struct lzset_string {
    size_t len;
    char *data;
} lzset_string;

static lzset_string *lzset_string_create(const char *data, size_t len) {
    lzset_string *s = (lzset_string *)malloc(sizeof(*s));

    s->len = len;
    s->data = (char *)malloc(len + 1);
    memcpy(s->data, data, len);

    s->data[len] = '\0';

    return s;
}

static void lzset_string_free(void *s) {
    free(((lzset_string *)s)->data);
    free(s);
}

static int lzset_string_compare(const void *a, const void *b) {
    const lzset_string *s1 = a, *s2 = b;

    int cmp =
        memcmp(s1->data, s2->data, s1->len <= s2->len ? s1->len : s2->len);

    return cmp ? cmp : s1->len - s2->len;
}

static int lzset_int_compare(const void *a, const void *b) {
    const int *n1 = (const int *)a;
    const int *n2 = (const int *)b;

    return (*n1 < *n2) ? -1 : (*n1 > *n2);
}

static int lzset_int_insert(lua_State *L) {
    skiplist *sl = lua_touserdata(L, 1);
    double score = luaL_checknumber(L, 2);
    int n = luaL_checkinteger(L, 3);

    int *p = malloc(sizeof(int));
    *p = n;

    skiplistInsert(sl, score, p);

    return 0;
}

static int lzset_string_insert(lua_State *L) {
    skiplist *sl = lua_touserdata(L, 1);
    double score = luaL_checknumber(L, 2);
    luaL_checktype(L, 3, LUA_TSTRING);

    size_t len;
    const char *data = lua_tolstring(L, 3, &len);
    lzset_string *s = lzset_string_create(data, len);

    skiplistInsert(sl, score, s);

    return 0;
}

static int lzset_int_delete(lua_State *L) {
    skiplist *sl = lua_touserdata(L, 1);
    double score = luaL_checknumber(L, 2);
    int n = luaL_checkinteger(L, 3);

    lua_pushboolean(L, skiplistDelete(sl, score, &n));

    return 1;
}

static int lzset_string_delete(lua_State *L) {
    skiplist *sl = lua_touserdata(L, 1);
    double score = luaL_checknumber(L, 2);
    luaL_checktype(L, 3, LUA_TSTRING);

    lzset_string s;
    s.data = (char *)lua_tolstring(L, 3, &s.len);

    lua_pushboolean(L, skiplistDelete(sl, score, &s));

    return 1;
}

static int lzset_int_update(lua_State *L) {
    skiplist *sl = lua_touserdata(L, 1);
    double curscore = luaL_checknumber(L, 2);
    int n = luaL_checkinteger(L, 3);
    double newscore = luaL_checknumber(L, 4);

    skiplistUpdateScore(sl, curscore, &n, newscore);

    return 0;
}

static int lzset_string_update(lua_State *L) {
    skiplist *sl = lua_touserdata(L, 1);
    double curscore = luaL_checknumber(L, 2);
    luaL_checktype(L, 3, LUA_TSTRING);
    double newscore = luaL_checknumber(L, 4);

    lzset_string s;
    s.data = (char *)lua_tolstring(L, 3, &s.len);

    skiplistUpdateScore(sl, curscore, &s, newscore);

    return 0;
}

static int lzset_int_at(lua_State *L) {
    skiplist *sl = lua_touserdata(L, 1);
    unsigned int rank = luaL_checkinteger(L, 2);
    skiplistNode *node = skiplistGetNodeByRank(sl, rank);

    if (node) {
        lua_pushnumber(L, node->score);
        lua_pushinteger(L, *(int *)(node->obj));
        return 2;
    }

    return 0;
}

static int lzset_string_at(lua_State *L) {
    skiplist *sl = lua_touserdata(L, 1);
    unsigned int rank = luaL_checkinteger(L, 2);
    skiplistNode *node = skiplistGetNodeByRank(sl, rank);

    if (node) {
        lzset_string *s = node->obj;
        lua_pushnumber(L, node->score);
        lua_pushlstring(L, s->data, s->len);
        return 2;
    }

    return 0;
}

static void lzset_int_delete_rank_cb(void *ctx, void *obj) {
    lua_State *L = (lua_State *)ctx;
    int *p = obj;

    lua_pushvalue(L, 4);
    lua_pushinteger(L, *p);

    lua_call(L, 1, 0);
}

static void lzset_string_delete_rank_cb(void *ctx, void *obj) {
    lua_State *L = (lua_State *)ctx;
    lzset_string *s = obj;

    lua_pushvalue(L, 4);
    lua_pushlstring(L, s->data, s->len);

    lua_call(L, 1, 0);
}

static int lzset_int_delete_range_by_rank(lua_State *L) {
    skiplist *sl = lua_touserdata(L, 1);
    unsigned int start = luaL_checkinteger(L, 2);
    unsigned int end = luaL_checkinteger(L, 3);
    luaL_checktype(L, 4, LUA_TFUNCTION);

    if (start > end) {
        unsigned int tmp = start;
        start = end;
        end = tmp;
    }

    lua_pushinteger(L, skiplistDeleteRangeByRank(sl, start, end,
                                                 lzset_int_delete_rank_cb, L));

    return 1;
}

static int lzset_string_delete_range_by_rank(lua_State *L) {
    skiplist *sl = lua_touserdata(L, 1);
    unsigned int start = luaL_checkinteger(L, 2);
    unsigned int end = luaL_checkinteger(L, 3);
    luaL_checktype(L, 4, LUA_TFUNCTION);

    if (start > end) {
        unsigned int tmp = start;
        start = end;
        end = tmp;
    }

    lua_pushinteger(L, skiplistDeleteRangeByRank(
                           sl, start, end, lzset_string_delete_rank_cb, L));

    return 1;
}

static int lzset_int_get_rank(lua_State *L) {
    skiplist *sl = lua_touserdata(L, 1);
    double score = luaL_checknumber(L, 2);
    int n = luaL_checkinteger(L, 3);

    unsigned long rank = skiplistGetRank(sl, score, &n);
    if (rank == 0) {
        return 0;
    }

    lua_pushinteger(L, rank);

    return 1;
}

static int lzset_string_get_rank(lua_State *L) {
    skiplist *sl = lua_touserdata(L, 1);
    double score = luaL_checknumber(L, 2);
    luaL_checktype(L, 3, LUA_TSTRING);

    lzset_string s;
    s.data = (char *)lua_tolstring(L, 3, &s.len);

    unsigned long rank = skiplistGetRank(sl, score, &s);
    if (rank == 0) {
        return 0;
    }

    lua_pushinteger(L, rank);

    return 1;
}

static int lzset_get_score_rank(lua_State *L) {
    skiplist *sl = lua_touserdata(L, 1);
    double score = luaL_checknumber(L, 2);
    int ex = lua_toboolean(L, 3);

    unsigned long rank = skiplistGetScoreRank(sl, score, ex);

    lua_pushinteger(L, rank);

    return 1;
}

static int lzset_int_get_range_by_rank(lua_State *L) {
    skiplist *sl = lua_touserdata(L, 1);

    unsigned long r1 = luaL_checkinteger(L, 2);
    unsigned long r2 = luaL_checkinteger(L, 3);

    int reverse, span;
    if (r1 <= r2) {
        reverse = 0;
        span = r2 - r1 + 1;
    } else {
        reverse = 1;
        span = r1 - r2 + 1;
    }

    skiplistNode *node = skiplistGetNodeByRank(sl, r1);

    lua_createtable(L, span, 0);

    int n = 0;
    while (node && n < span) {
        lua_pushinteger(L, *(int *)node->obj);
        lua_rawseti(L, -2, ++n);
        node = reverse ? node->backward : node->level[0].forward;
    }

    return 1;
}

static int lzset_string_get_range_by_rank(lua_State *L) {
    skiplist *sl = lua_touserdata(L, 1);

    unsigned long r1 = luaL_checkinteger(L, 2);
    unsigned long r2 = luaL_checkinteger(L, 3);

    int reverse, span;
    if (r1 <= r2) {
        reverse = 0;
        span = r2 - r1 + 1;
    } else {
        reverse = 1;
        span = r1 - r2 + 1;
    }

    skiplistNode *node = skiplistGetNodeByRank(sl, r1);

    lua_createtable(L, span, 0);

    int n = 0;
    lzset_string *s;
    while (node && n < span) {
        s = node->obj;
        lua_pushlstring(L, s->data, s->len);
        lua_rawseti(L, -2, ++n);
        node = reverse ? node->backward : node->level[0].forward;
    }

    return 1;
}

static int lzset_int_get_range_by_score(lua_State *L) {
    skiplist *sl = lua_touserdata(L, 1);
    double s1 = luaL_checknumber(L, 2);
    double s2 = luaL_checknumber(L, 3);

    int reverse;
    skiplistNode *node;

    if (s1 <= s2) {
        reverse = 0;
        node = skiplistFirstInRange(sl, s1, s2, 0, 0);
    } else {
        reverse = 1;
        node = skiplistLastInRange(sl, s2, s1, 0, 0);
    }

    lua_newtable(L);
    int n = 0;
    while (node) {
        if (reverse) {
            if (node->score < s2) {
                break;
            }
        } else if (node->score > s2) {
            break;
        }
        lua_pushinteger(L, *(int *)node->obj);
        lua_rawseti(L, -2, ++n);
        node = reverse ? node->backward : node->level[0].forward;
    }

    return 1;
}

static int lzset_string_get_range_by_score(lua_State *L) {
    skiplist *sl = lua_touserdata(L, 1);
    double s1 = luaL_checknumber(L, 2);
    double s2 = luaL_checknumber(L, 3);

    int reverse;
    skiplistNode *node;

    if (s1 <= s2) {
        reverse = 0;
        node = skiplistFirstInRange(sl, s1, s2, 0, 0);
    } else {
        reverse = 1;
        node = skiplistLastInRange(sl, s2, s1, 0, 0);
    }

    lua_newtable(L);
    int n = 0;
    lzset_string *s;
    while (node) {
        if (reverse) {
            if (node->score < s2) {
                break;
            }
        } else if (node->score > s2) {
            break;
        }
        s = node->obj;
        lua_pushlstring(L, s->data, s->len);
        lua_rawseti(L, -2, ++n);
        node = reverse ? node->backward : node->level[0].forward;
    }

    return 1;
}

static int lzset_int_print_node(void *ctx, int index, double score, void *obj) {
    printf("(%d, %f, %d)\n", index, score, *(int *)obj);

    return 1;
}

static int lzset_string_print_node(void *ctx, int index, double score,
                                   void *obj) {
    printf("(%d, %f, %s)\n", index, score, ((lzset_string *)obj)->data);

    return 1;
}

static int lzset_int_dump(lua_State *L) {
    skiplist *sl = lua_touserdata(L, 1);

    skiplistIterate(sl, NULL, lzset_int_print_node);

    return 0;
}

static int lzset_string_dump(lua_State *L) {
    skiplist *sl = lua_touserdata(L, 1);

    skiplistIterate(sl, NULL, lzset_string_print_node);

    return 0;
}

static int lzset_int_new(lua_State *L) {
    skiplist *sl = lua_newuserdata(L, sizeof(skiplist));

    skiplistInit(sl, lzset_int_compare, free);

    lua_pushvalue(L, lua_upvalueindex(1));
    lua_setmetatable(L, -2);

    return 1;
}

static int lzset_string_new(lua_State *L) {
    skiplist *sl = lua_newuserdata(L, sizeof(skiplist));

    skiplistInit(sl, lzset_string_compare, lzset_string_free);

    lua_pushvalue(L, lua_upvalueindex(1));
    lua_setmetatable(L, -2);

    return 1;
}

static int lzset_count(lua_State *L) {
    skiplist *sl = lua_touserdata(L, 1);
    lua_pushinteger(L, sl->length);

    return 1;
}

static int lzset_release(lua_State *L) {
    skiplist *sl = lua_touserdata(L, 1);

    skiplistFreeNodes(sl);

    return 0;
}

int luaopen_lzset_int(lua_State *L) {
    luaL_Reg libs[] = {{"insert", lzset_int_insert},
                       {"delete", lzset_int_delete},
                       {"update", lzset_int_update},
                       {"at", lzset_int_at},
                       {"count", lzset_count},
                       {"delete_range_by_rank", lzset_int_delete_range_by_rank},

                       {"get_rank", lzset_int_get_rank},
                       {"get_score_rank", lzset_get_score_rank},
                       {"get_range_by_rank", lzset_int_get_range_by_rank},
                       {"get_range_by_score", lzset_int_get_range_by_score},

                       {"dump", lzset_int_dump},
                       {NULL, NULL}};

    lua_createtable(L, 0, 3);

#if LUA_VERSION_NUM >= 502
    luaL_newlib(L, libs);
#else
    lzset_lua_newlibtable(L, libs);
    luaL_register(L, NULL, libs);
#endif

    lua_setfield(L, -2, "__index");

    lua_pushcfunction(L, lzset_release);
    lua_setfield(L, -2, "__gc");

    lua_pushcfunction(L, lzset_count);
    lua_setfield(L, -2, "__len");

    lua_pushcclosure(L, lzset_int_new, 1);

    return 1;
}

int luaopen_lzset_string(lua_State *L) {
    luaL_Reg libs[] = {
        {"insert", lzset_string_insert},
        {"delete", lzset_string_delete},
        {"update", lzset_string_update},
        {"at", lzset_string_at},
        {"count", lzset_count},
        {"delete_range_by_rank", lzset_string_delete_range_by_rank},

        {"get_rank", lzset_string_get_rank},
        {"get_score_rank", lzset_get_score_rank},
        {"get_range_by_rank", lzset_string_get_range_by_rank},
        {"get_range_by_score", lzset_string_get_range_by_score},

        {"dump", lzset_string_dump},
        {NULL, NULL}};

    lua_createtable(L, 0, 3);

#if LUA_VERSION_NUM >= 502
    luaL_newlib(L, libs);
#else
    lzset_lua_newlibtable(L, libs);
    luaL_register(L, NULL, libs);
#endif

    lua_setfield(L, -2, "__index");

    lua_pushcfunction(L, lzset_release);
    lua_setfield(L, -2, "__gc");

    lua_pushcfunction(L, lzset_count);
    lua_setfield(L, -2, "__len");

    lua_pushcclosure(L, lzset_string_new, 1);

    return 1;
}
