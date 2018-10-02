/*****************************************************************************
 * lfind.c : implement lfind
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

void *lfind(const void *key, const void *base, size_t *nmemb,
            size_t size, int(*cmp)(const void *, const void *))
{
    for (int i = 0; i < *nmemb; ++i)
    {
        const void *elem = base + i * size;
        if (!cmp(key, elem))
            return elem;
    }
    return NULL;
}
