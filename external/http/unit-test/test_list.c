// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "../include/list.h"

#include <assert.h>

static void count_entry(listEntry_t *entry, void *data)
{
    (void)entry;
    ++*(int *)data;
}

int main(void)
{
    list_t *list = list_createList();
    int visited = 0;

    assert(list != NULL);
    list_iterateThroughListBackward(list, count_entry, &visited);
    assert(visited == 0);

    list_freeList(list);
    return 0;
}
