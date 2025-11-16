#ifndef LANG_H
#define LANG_H


#include "inttypes.h"

enum log_source_type
{
    LOG_PARSER,
};

enum log_level
{
    LOG_INFO,
    LOG_NOTE,
    LOG_WARNING,
    LOG_ERROR,
};

/* 0 0 means empty code span. */
/* span is half-interval */
struct code_span
{
    int64_t begin;
    int64_t end;
};

#define SPAN(begin, end) ((struct code_span){(begin), (end)})

struct log_item
{
    enum log_source_type source;
    enum log_level level;
    char *message;
    struct code_span code_span;
    struct log_item *associated_item;
};

struct compilation_log
{
    struct log_item *items;
    int64_t items_len;
    int64_t items_alloc;
};


#define MAX_PIPELINE_INPUT 16
#define MAX_PIPELINE_OUTPUT 16


struct worker
{
    struct pipe *inputs[MAX_PIPELINE_INPUT];
    int64_t inputs_len;
    struct pipe *outputs[MAX_PIPELINE_OUTPUT];
    int64_t outputs_len;
    char *name;
    struct code_span code_position;
};


struct pipe
{
    char *name;
    struct code_span code_position;
};


#define MAX_FREE_VARS 16
#define MAX_PIPELINE_VARS 16
#define MAX_PIPELINES 16
#define MAX_PIPELINE_ARGUMENTS 32
#define MAX_PIPELINE_WORKERS 64
#define MAX_PIPELINE_WORKER_SUBS 16
#define MAX_PIPELINE_OUTPUTS 8

enum pipeline_argument_type
{
    ARGUMENT_NAME,
    ARGUMENT_PIPELINE,
};

struct pipeline_argument_definition
{
    struct code_span code_position;

    enum pipeline_argument_type type;

    union {
        char *name;
        struct pipeline_definition *pipeline;
    };
};


struct pipeline_worker_substitution
{
    char *name;
    char *symbol;
    struct code_span code_position;
};


struct pipeline_worker_definition
{
    char *name;
    struct code_span code_position;

    struct pipeline_worker_substitution subs[MAX_PIPELINE_WORKER_SUBS];
    int64_t subs_len;
};


struct pipeline_output_definition
{
    char *name;
    struct code_span code_position;
};


struct pipeline_definition
{
    struct code_span code_position;

    struct pipeline_argument_definition args[MAX_PIPELINE_ARGUMENTS];
    int64_t args_len;
    struct pipeline_worker_definition workers[MAX_PIPELINE_WORKERS];
    int64_t workers_len;
    struct pipeline_output_definition outputs[MAX_PIPELINE_OUTPUTS];
    int64_t outputs_len;
};


struct definition
{
    char *name;
    struct code_span position;
    
    char *free_vars[MAX_FREE_VARS];
    int64_t free_vars_len;
    char *pipeline_vars[MAX_PIPELINE_VARS];
    int64_t pipeline_vars_len;

    struct pipeline_definition pipelines[MAX_PIPELINES];
    int64_t pipelines_len;
};


struct program
{
    char *filename;
    char *source_code;
    int64_t source_code_len;
    int64_t *line_to_position;
    int64_t code_lines;
    
    struct compilation_log log;
};


struct program *program_create_from_code(char *filename, char *code);


#endif
