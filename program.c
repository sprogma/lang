#include "lang.h"

#include "stdio.h"
#include "ctype.h"
#include "stdlib.h"
#include "string.h"
#include "inttypes.h"

static int64_t iskey(int chr);
static int64_t skip_spaces(struct program *program, int64_t position);
static int64_t skip_until(struct program *program, int64_t position, int symbol);
static char *str_from_code(struct program *program, int64_t begin, int64_t end);
static struct log_item *program_log(struct program *program, enum log_source_type source, enum log_level level, char *message, struct code_span code_span, struct log_item *associated_item);
static int64_t parse_pipeline_argument(struct program *program, int64_t position, struct definition *definition, struct pipeline_definition *pipeline, struct pipeline_argument_definition *arg);
static int64_t parse_pipeline_worker(struct program *program, int64_t position, struct definition *definition, struct pipeline_definition *pipeline, struct pipeline_worker_definition *worker);
static int64_t parse_pipeline_output(struct program *program, int64_t position, struct definition *definition, struct pipeline_definition *pipeline, struct pipeline_output_definition *output);
static int64_t parse_pipeline(struct program *program, int64_t position, struct definition *definition, struct pipeline_definition *pipeline);
static int64_t parse_pipeline_many(struct program *program, int64_t position, struct definition *definition);
static int64_t parse_definition(struct program *program, int64_t position);
static void program_parse(struct program *program);

static int64_t iskey(int chr)
{
    return isalpha(chr) || isdigit(chr) || chr == '-' || chr == '_' || chr == '?' || chr == '!' || chr == '[' || chr == ']' || chr == '.';
}


static int64_t skip_spaces(struct program *program, int64_t position)
{
    while (position < program->source_code_len && isspace(program->source_code[position])) { position++; }
    return position;
}


static int64_t skip_until(struct program *program, int64_t position, int symbol)
{
    while (position < program->source_code_len && program->source_code[position] != symbol) { position++; }
    return position;
}


static void register_definition(struct program *program, struct definition *definition)
{
    if (program->definitions_len >= program->definitions_alloc)
    {
        program->definitions_alloc = 2 * program->definitions_alloc + !program->definitions_alloc;
        void *new_ptr = realloc(program->definitions, sizeof(*program->definitions) * program->definitions_alloc);
        if (new_ptr == NULL)
        {
            fprintf(stderr, "Error: No memory for PARSING.\n");
            exit(1);
        }

        program->definitions = new_ptr;
    }

    program->definitions[program->definitions_len++] = definition;
}


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


static int64_t parse_pipeline_argument(struct program *program, int64_t position, struct definition *definition, struct pipeline_definition *pipeline, struct pipeline_argument_definition *arg)
{
    (void)definition;
    (void)pipeline;

    position = skip_spaces(program, position);

    int64_t arg_begin = position;

    /* find next ',' or '>' */
    int64_t cnt = 0;
    while (position < program->source_code_len && (
               (
                program->source_code[position] != ',' &&
                program->source_code[position] != '>'
               ) || cnt != 0))
    {
        cnt += (program->source_code[position] == '(') - (program->source_code[position] == ')');
        if (cnt < 0)
        {
            break;
        }
        position++;
    }

    if (position == program->source_code_len)
    {
        if (cnt != 0)
        {
            program_log(program, LOG_PARSER, LOG_ERROR, "Wrong definition syntax. Excepted closing ')' in pipeline argument definition", SPAN(arg_begin, position), NULL);
        }
        else
        {
            program_log(program, LOG_PARSER, LOG_ERROR, "Wrong definition syntax. Excepted ',' or '>' after pipeline argument definition", SPAN(arg_begin, position), NULL);
        }
        return position;
    }

    int64_t arg_end = position;

    while (arg_begin < arg_end && isspace(program->source_code[arg_begin])) { arg_begin++; }
    while (arg_end > arg_begin && isspace(program->source_code[arg_end - 1])) { arg_end--; }


    arg->code_position = SPAN(arg_begin, arg_end);

    if (program->source_code[arg_begin] == '(' && program->source_code[arg_end - 1] == ')')
    {
        arg->type = ARGUMENT_PIPELINE;
        arg->pipeline = malloc(sizeof(*arg->pipeline));
        parse_pipeline(program, arg_begin + 1, NULL, arg->pipeline);
    }
    else
    {
        /* check that it is all from keys */
        for (int i = arg_begin; i < arg_end; ++i)
        {
            if (!iskey(program->source_code[i]))
            {
                program_log(program, LOG_PARSER, LOG_ERROR, "Wrong definition syntax. Pipeline argument isn't pipeline, but contains invalid characters", SPAN(i, i + 1), NULL);
                return position;
            }
        }

        arg->type = ARGUMENT_NAME;
        arg->name = str_from_code(program, arg_begin, arg_end);
    }

    return position;
}


static int64_t parse_pipeline_worker(struct program *program, int64_t position, struct definition *definition, struct pipeline_definition *pipeline, struct pipeline_worker_definition *worker)
{
    (void)definition;
    (void)pipeline;

    position = skip_spaces(program, position);

    int64_t worker_begin = position;

    /* find next ',' or '>' */
    int64_t cnt = 0;
    while (position < program->source_code_len && (
               (
                program->source_code[position] != '>' &&
                (program->source_code[position] != '|' || program->source_code[position + 1] != ':') &&
                program->source_code[position] != ';' &&
                program->source_code[position] != '}'
               ) || cnt != 0))
    {
        if (program->source_code[position] == '|' && cnt == 0)
        {
            program_log(program, LOG_PARSER, LOG_ERROR, "Wrong definition syntax. Symbol '|' inside worker definition. Probably forgot to end previous definition", SPAN(position, position + 1), NULL);
            break;
        }
        cnt += (program->source_code[position] == '(') - (program->source_code[position] == ')');
        if (cnt < 0)
        {
            break;
        }
        position++;
    }

    if (position == program->source_code_len)
    {
        if (cnt != 0)
        {
            program_log(program, LOG_PARSER, LOG_ERROR, "Wrong definition syntax. Excepted closing ')' in pipeline worker definition", SPAN(worker_begin, position), NULL);
        }
        else
        {
            program_log(program, LOG_PARSER, LOG_ERROR, "Wrong definition syntax. Excepted '>' or '|:' or ';' or '}' after pipeline worker definition", SPAN(worker_begin, position), NULL);
        }
        return position;
    }

    int64_t worker_end = position;

    while (worker_begin < worker_end && isspace(program->source_code[worker_begin])) { worker_begin++; }
    while (worker_end > worker_begin && isspace(program->source_code[worker_end - 1])) { worker_end--; }
    
    worker->code_position = SPAN(worker_begin, worker_end);

    /* 1. parse name */
    int64_t name_end = worker_begin;
    {
        while (name_end < worker_end && iskey(program->source_code[name_end])) { name_end++; }

        worker->name = str_from_code(program, worker_begin, name_end);
    }
    
    /* 2. parse replacement table */
    {
        worker->subs_len = 0;
        
        int64_t i = name_end + 1;
        while (1)
        {
            /* extract group */
            i = skip_spaces(program, i);
            if (i >= worker_end)
            {
                break;
            }
            int64_t begin = i, cnt = 0;
            while (i < worker_end && (!isspace(program->source_code[i]) || cnt != 0)) 
            { 
                cnt += (program->source_code[i] == '(') - (program->source_code[i] == ')');
                i++; 
            }
            int64_t end = i;

            /* parse group */
            
            int64_t delim = begin;
            while (delim < end && program->source_code[delim] != '=') { delim++; }

            /* check that it is all from keys */
            for (int pos = begin; pos < delim; ++pos)
            {
                if (!iskey(program->source_code[pos]))
                {
                    program_log(program, LOG_PARSER, LOG_ERROR, "Wrong definition syntax. Substitution contains invalid characters", SPAN(pos, pos + 1), NULL);
                    return position;
                }
            }

            if (program->source_code[delim + 1] == '(' && program->source_code[end - 1] == ')')
            {
                worker->subs[worker->subs_len].code_position = SPAN(begin, end);
                worker->subs[worker->subs_len].type = SUBSTITUTION_PIPELINE;
                worker->subs[worker->subs_len].name = str_from_code(program, begin, delim);
                worker->subs[worker->subs_len].pipeline = malloc(sizeof(*worker->subs[worker->subs_len].pipeline));
                parse_pipeline(program, delim + 2, NULL, worker->subs[worker->subs_len].pipeline);
                worker->subs_len++;
            }
            else
            {
                for (int pos = delim + 1; pos < end; ++pos)
                {
                    if (!iskey(program->source_code[pos]))
                    {
                        program_log(program, LOG_PARSER, LOG_ERROR, "Wrong definition syntax. Substitution is not to pipe, and contains invalid characters", SPAN(pos, pos + 1), NULL);
                        return position;
                    }
                }

                worker->subs[worker->subs_len].code_position = SPAN(begin, end);
                worker->subs[worker->subs_len].type = SUBSTITUTION_SYMBOL;
                worker->subs[worker->subs_len].name = str_from_code(program, begin, delim);
                worker->subs[worker->subs_len].symbol = str_from_code(program, delim + 1, end);
                worker->subs_len++;
            }
        }
    }
    

    return position;
}


static int64_t parse_pipeline_output(struct program *program, int64_t position, struct definition *definition, struct pipeline_definition *pipeline, struct pipeline_output_definition *output)
{
    (void)definition;
    (void)pipeline;

    position = skip_spaces(program, position);

    int64_t output_begin = position;

    /* find next ',' or '>' */
    int64_t cnt = 0;
    while (position < program->source_code_len && (
               (
                program->source_code[position] != ',' &&
                (program->source_code[position] != '|' || program->source_code[position + 1] != ':') &&
                program->source_code[position] != '}' &&
                program->source_code[position] != ';'
               ) || cnt != 0))
    {
        if (program->source_code[position] == '>' && cnt == 0)
        {
            program_log(program, LOG_PARSER, LOG_ERROR, "Wrong definition syntax. Symbol '>' inside output definition. Probably forgot comma or semicolon", SPAN(position, position + 1), NULL);
            break;
        }
        if (program->source_code[position] == '|' && cnt == 0)
        {
            program_log(program, LOG_PARSER, LOG_ERROR, "Wrong definition syntax. Symbol '|' inside output definition. Probably forgot to end previous definition", SPAN(position, position + 1), NULL);
            break;
        }
        cnt += (program->source_code[position] == '(') - (program->source_code[position] == ')');
        if (cnt < 0)
        {
            break;
        }
        position++;
    }

    if (position == program->source_code_len)
    {
        if (cnt != 0)
        {
            program_log(program, LOG_PARSER, LOG_ERROR, "Wrong definition syntax. Excepted closing ')' in pipeline output definition", SPAN(output_begin, position), NULL);
        }
        else
        {
            program_log(program, LOG_PARSER, LOG_ERROR, "Wrong definition syntax. Excepted ',' or ';' or '|:' or '}' after pipeline output definition", SPAN(output_begin, position), NULL);
        }
        return position;
    }

    /* check all characters */
    for (int pos = output_begin; pos < position; ++pos)
    {
        if (!iskey(program->source_code[pos]))
        {
            program_log(program, LOG_PARSER, LOG_ERROR, "Wrong definition syntax. Output name contains invalid characters", SPAN(pos, pos + 1), NULL);
            return position;
        }
    }

    output->code_position = SPAN(output_begin, position);
    output->name = str_from_code(program, output_begin, position);

    return position;
}


static int64_t parse_pipeline(struct program *program, int64_t position, struct definition *definition, struct pipeline_definition *pipeline)
{
    (void)definition;

    position = skip_spaces(program, position);

    int64_t pipeline_begin = position;

    /* read all arguments */
    pipeline->args_len = 0;
    while (1)
    {
        position = skip_spaces(program, position);

        if (program->source_code[position] == '>')
        {
            break;
        }

        position = parse_pipeline_argument(program, position, definition, pipeline, &pipeline->args[pipeline->args_len++]);

        position = skip_spaces(program, position);

        if (program->source_code[position] == '>')
        {
            break;
        }
        else if (program->source_code[position] == ',')
        {
            position++;
        }
        else
        {
            program_log(program, LOG_PARSER, LOG_ERROR, "Wrong definition syntax. Excepted ',' or '>' after pipeline argument definition", SPAN(position, position + 1), NULL);
            return position;
        }
    }

    /* read all pipeline */
    int64_t parsing_outputs = 0;
    pipeline->workers_len = 0;
    while (1)
    {
        position = skip_spaces(program, position);

        if (program->source_code[position] == '>' &&
            program->source_code[position + 1] == '>')
        {
            position += 2;
            parsing_outputs = 1;
            break;
        }
        if (program->source_code[position] == '|' &&
            program->source_code[position + 1] == ':')
        {
            break;
        }
        if (program->source_code[position] == ';')
        {
            break;
        }
        if (program->source_code[position] == ')')
        {
            break;
        }
        if (program->source_code[position] == '}')
        {
            break;
        }

        /* skip leading > symbol */
        if (program->source_code[position] == '>')
        {
            position++;
        }
        else
        {
            program_log(program, LOG_PARSER, LOG_ERROR, "Wrong definition syntax. Excepted '>' or '>>' or '|:' or ';' or '}' after pipeline worker definition", SPAN(position, position + 1), NULL);
            return position;
        }

        position = parse_pipeline_worker(program, position, definition, pipeline, &pipeline->workers[pipeline->workers_len++]);
    }


    /* read all pipeline outputs */
    pipeline->outputs_len = 0;
    if (parsing_outputs)
    {
        while (1)
        {
            position = skip_spaces(program, position);

            if (program->source_code[position] == '|' &&
                program->source_code[position + 1] == ':')
            {
                break;
            }
            if (program->source_code[position] == ';')
            {
                break;
            }
            if (program->source_code[position] == '}')
            {
                break;
            }

            position = parse_pipeline_output(program, position, definition, pipeline, &pipeline->outputs[pipeline->outputs_len++]);

            position = skip_spaces(program, position);

            if (program->source_code[position] == '|' &&
                program->source_code[position + 1] == ':')
            {
                break;
            }
            if (program->source_code[position] == ';')
            {
                break;
            }
            if (program->source_code[position] == '}')
            {
                break;
            }
            if (program->source_code[position] == ')')
            {
                break;
            }
            /* skip trailing , symbol */
            if (program->source_code[position] == ',')
            {
                position++;
            }
            else
            {
                program_log(program, LOG_PARSER, LOG_ERROR, "Wrong definition syntax. Excepted ',' or '|:' or ';' or '}' after pipeline output definition", SPAN(position, position + 1), NULL);
                return position;
            }
        }
    }

    pipeline->code_position = SPAN(pipeline_begin, position);

    return position;
}


static int64_t parse_pipeline_many(struct program *program, int64_t position, struct definition *definition)
{
    (void)definition;

    position = skip_spaces(program, position);


    definition->pipelines_len = 0;
    if (program->source_code[position] == '{')
    {
        position++;
        /* this is pipelines gathering */
        while (1)
        {
            position = parse_pipeline(program, position, definition, &definition->pipelines[definition->pipelines_len++]);

            position = skip_spaces(program, position);

            if (program->source_code[position] == '}')
            {
                position++;
                break;
            }
            if (program->source_code[position] == ';')
            {
                position++;
            }
            else
            {
                program_log(program, LOG_PARSER, LOG_ERROR, "Wrong definition syntax. Excepted ';' or '}' after pipeline in pipeline group", SPAN(position, position + 1), NULL);
                return position;
            }
        }

    }
    else
    {
        position = parse_pipeline(program, position, definition, &definition->pipelines[definition->pipelines_len++]);
    }

    return position;
}


static int64_t parse_definition(struct program *program, int64_t position)
{
    /* skip all spaces */
    position = skip_spaces(program, position);

    if (position >= program->source_code_len)
    {
        return position;
    }

    /* if this is comment skip it */
    if (program->source_code[position] == '#')
    {
        while (program->source_code[position] != '\n' &&
               position < program->source_code_len)
       {
            position++;
       }
       return position;
    }


    struct definition *definition = malloc(sizeof(*definition));
    definition->name = NULL;
    definition->free_vars_len = 0;
    definition->pipeline_vars_len = 0;


    definition->code_position.begin = position;

    int64_t new_position = parse_pipeline_many(program, position, definition);
    if (new_position == position)
    {
        program_log(program, LOG_PARSER, LOG_ERROR, "Wrong definition syntax. parsed empty pipeline [there was error at parsing]", SPAN(position, position + 1), NULL);
        position++;
    }
    else
    {
        position = new_position;
    }

    position = skip_spaces(program, position);

    if (program->source_code[position] != '|' ||
        program->source_code[position + 1] != ':')
    {
        program_log(program, LOG_PARSER, LOG_ERROR, "Wrong definition syntax. Expected '|:' after pipeline", SPAN(position, position + 2), NULL);
        return position;
    }
    else
    {
        /* skip this |: sign */
        position += 2;
    }

    position = skip_spaces(program, position);

    /* read definition name */
    {
        int64_t start = position;
        while (iskey(program->source_code[position])) { position++; }

        definition->name = str_from_code(program, start, position);
    }

    position = skip_spaces(program, position);


    /* read pipeline names */
    if (program->source_code[position] == '(')
    {
        int64_t start = skip_spaces(program, position + 1);

        position = skip_until(program, position, ')');
        if (program->source_code[position] != ')')
        {
            program_log(program, LOG_PARSER, LOG_ERROR, "Wrong definition syntax. Expected closing ')' to match this one", SPAN(start - 1, start), NULL);
        }
        else
        {
            /* parse all data */
            int64_t i = start;
            while (i < position)
            {
                i = skip_spaces(program, i);

                int64_t name_start = i;
                while (iskey(program->source_code[i])) { i++; }
                int64_t name_end = i;

                if (name_start != name_end)
                {
                    definition->pipeline_vars[definition->pipeline_vars_len++] = str_from_code(program, name_start, name_end);
                }
                else
                {
                    if (name_start != start)
                    {
                        program_log(program, LOG_PARSER, LOG_ERROR, "Wrong definition pipeline variables syntax. Expected not empty name", SPAN(name_start, name_end + 1), NULL);
                        break;
                    }
                }

                i = skip_spaces(program, i);

                if (i >= position)
                {
                    break;
                }

                if (program->source_code[i] != ',')
                {
                    program_log(program, LOG_PARSER, LOG_ERROR, "Wrong definition pipeline variables syntax. Expected ',' before next name", SPAN(i, i + 1), NULL);
                    break;
                }
                else
                {
                    i++;
                }
            }
            position++;
        }
    }

    /* read free variables */
    if (program->source_code[position] == '{')
    {
        int64_t start = skip_spaces(program, position + 1);

        position = skip_until(program, position, '}');
        if (program->source_code[position] != '}')
        {
            program_log(program, LOG_PARSER, LOG_ERROR, "Wrong definition syntax. Expected closing ')' to match this one", SPAN(start - 1, start), NULL);
        }
        else
        {
            /* parse all data */
            int64_t i = start;
            while (i < position)
            {
                i = skip_spaces(program, i);

                int64_t name_start = i;
                while (iskey(program->source_code[i])) { i++; }
                int64_t name_end = i;

                if (name_start != name_end)
                {
                    definition->free_vars[definition->free_vars_len++] = str_from_code(program, name_start, name_end);
                }
                else
                {
                    if (name_start != start)
                    {
                        program_log(program, LOG_PARSER, LOG_ERROR, "Wrong definition free variables syntax. Expected not empty name", SPAN(name_start, name_end + 1), NULL);
                        break;
                    }
                }

                i = skip_spaces(program, i);

                if (i >= position)
                {
                    break;
                }

                if (program->source_code[i] != ',')
                {
                    program_log(program, LOG_PARSER, LOG_ERROR, "Wrong definition free variables syntax. Expected ',' before next name", SPAN(i, i + 1), NULL);
                    break;
                }
                else
                {
                    i++;
                }
            }
            position++;
        }
    }

    definition->code_position.end = position;
    
    register_definition(program, definition);

    return position;
}


static void program_parse(struct program *program)
{
    /* parse all top-level definitions */
    int64_t position = 0;
    while (position < program->source_code_len)
    {
        /* parse definition */
        position = parse_definition(program, position);
    }
}


struct program *program_create_from_code(char *filename, char *code)
{
    struct program *program = malloc(sizeof(*program));

    program->log.items = NULL;
    program->log.items_len = 0;
    program->log.items_alloc = 0;

    program->definitions = NULL;
    program->definitions_len = 0;
    program->definitions_alloc = 0;

    program->filename = filename;
    program->source_code_len = strlen(code);
    program->source_code = code;

    int64_t code_lines_alloc = 1;
    for (int64_t i = 0; i < program->source_code_len; ++i)
    {
        code_lines_alloc += (code[i] == '\n');
    }
    program->line_to_position = malloc(sizeof(*program->line_to_position) * code_lines_alloc);
    program->code_lines = 1;
    program->line_to_position[0] = 0;
    for (int64_t i = 0; i < program->source_code_len; ++i)
    {
        if (code[i] == '\n')
        {
            program->line_to_position[program->code_lines++] = i;
        }
    }

    /* parse file content */
    program_parse(program);

    /* print program */
    program_ast_dump(stdout, program);

    /* get full workflow */
    program_get_workflow(program);

    return program;
}
