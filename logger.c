#include "lang.h"

#include "stdio.h"
#include "ctype.h"
#include "stdlib.h"
#include "string.h"
#include "inttypes.h"


static char *str_from_code(struct program *program, int64_t begin, int64_t end)
{
    if (end > program->source_code_len)
    {
        end = program->source_code_len;
    }
    if (begin >= end)
    {
        char *empty = malloc(1);
        empty[0] = '\0';
        return empty;
    }
    char *res = malloc(end - begin + 1);
    memcpy(res, program->source_code + begin, end - begin);
    res[end - begin] = 0;
    return res;
}

static void position_to_line_col(struct program *program, int64_t pos, int64_t *line, int64_t *col)
{
    int64_t l = 0, r = program->code_lines, m = 0;
    while (r - l > 1)
    {
        m = (l + r) / 2;
        if (program->line_to_position[m] <= pos)
        { l = m; }
        else
        { r = m; }
    }
    *line = l + 1;
    *col = pos - program->line_to_position[l];
}


struct log_item *program_log(struct program *program, enum log_source_type source, enum log_level level, char *message, struct code_span code_span, struct log_item *associated_item)
{

    if (program->log.items_len >= program->log.items_alloc)
    {
        program->log.items_alloc = 2 * program->log.items_alloc + !program->log.items_alloc;
        void *new_ptr = realloc(program->log.items, sizeof(*program->log.items) * program->log.items_alloc);
        if (new_ptr == NULL)
        {
            fprintf(stderr, "Error: No memory for PARSING.\n");
            exit(1);
        }

        program->log.items = new_ptr;
    }

    program->log.items[program->log.items_len++] = (struct log_item){
        .source = source,
        .level = level,
        .message = message,
        .code_span = code_span,
        .associated_item = associated_item,
    };

    switch (source)
    {
        case LOG_PARSER:
            printf("PARSER::");
        case LOG_WORKFLOW:
            printf("WORKFLOW::");
    }
    switch (level)
    {
        case LOG_INFO: printf("INFO");
        case LOG_NOTE: printf("NOTE");
        case LOG_WARNING: printf("WARNING");
        case LOG_ERROR: printf("ERROR");
    }
    int64_t line, col;
    position_to_line_col(program, code_span.begin, &line, &col);
    printf(":%s:%lld:%lld %s\n", program->filename, line, col, message);

    char *s = str_from_code(program, code_span.begin, code_span.end);
    printf("[at <%s>]\n", s);
    free(s);

    return &program->log.items[program->log.items_len - 1];
}
