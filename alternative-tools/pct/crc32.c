
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**************************************************************************/
/* HASH                                                                   */
/**************************************************************************/

struct CRC32
{
    size_t len;
    uint32_t h;
    union
    {
        uint8_t buf[8];
        uint64_t word;
    };
    uint8_t i;
};

static void crc32_update_byte(uint8_t b, struct CRC32 *ctx)
{
    ctx->len++;
    ctx->buf[ctx->i++] = b;
    if (ctx->i >= sizeof(ctx->buf))
    {
        ctx->h = __builtin_ia32_crc32di(ctx->h, ctx->word);
        ctx->i = 0;
    }
}

static void crc32_update(const uint8_t *buf, size_t size, struct CRC32 *ctx)
{
    for (size_t i = 0; i < size; i++)
        crc32_update_byte(buf[i], ctx);
}

static uint32_t crc32_finalize(struct CRC32 *ctx)
{
    crc32_update((const uint8_t *)&ctx->len, sizeof(ctx->len), ctx);
    for (size_t i = 0; ctx->i != 0; i++)
        crc32_update_byte((uint8_t)i, ctx);
    return ctx->h;
}

int main(int argc, char **argv)
{
    if (argc != 1 && argc != 2)
    {
        fprintf(stderr, "usage: %s [filename]\n", argv[0]);
        return 1;
    }

    if (argc == 2)
    {
        const char *filename = argv[1];
        FILE *stream = freopen(filename, "r", stdin);
        if (stream == NULL)
        {
            fprintf(stderr, "error: failed to open \"%s\": %s\n",
                filename, strerror(errno));
            return 1;
        }
    }

    struct CRC32 ctx = {0};
    while (true)
    {
        char c = getc(stdin);
        if (c == EOF && (feof(stdin) || ferror(stdin)))
            break;
        uint8_t b = (uint8_t)c;
        crc32_update(&b, 1, &ctx);
    }

    uint32_t h = crc32_finalize(&ctx);
    printf("0x%.8X\n", h);
    return 0;
}

