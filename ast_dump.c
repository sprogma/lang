#include "lang.h"

#include "stdio.h"
#include "ctype.h"
#include "stdlib.h"
#include "string.h"
#include "inttypes.h"

static void print_pipeline(FILE *stream, int64_t indent, struct pipeline_definition *pipeline)
{
    char sindent[128];
    memset(sindent, ' ', sizeof(sindent));
    for (int i = 0; i < 128; i += 2)
    {
        sindent[i] = '|';
    }
    sindent[indent] = 0;
    
    fprintf(stream, "%sPipeline:\n", sindent);
    
    fprintf(stream, "%s| Arguments: %lld\n", sindent, pipeline->args_len);
    for (int64_t i = 0; i < pipeline->args_len; ++i)
    {
        if (pipeline->args[i].type == ARGUMENT_NAME)
        {
            fprintf(stream, "%s| | Argument %lld : NAME : %s\n", sindent, i, pipeline->args[i].name);
        }
        else
        {
            fprintf(stream, "%s| | Argument %lld : PIPE\n", sindent, i);
            print_pipeline(stream, indent + 6, pipeline->args[i].pipeline);
        }
    }
    
    fprintf(stream, "%s| Workers: %lld\n", sindent, pipeline->workers_len);
    for (int64_t i = 0; i < pipeline->workers_len; ++i)
    {
        fprintf(stream, "%s| | Worker %lld : %s\n", sindent, i, pipeline->workers[i].name);
        fprintf(stream, "%s| | | Substitutions : %lld\n", sindent, pipeline->workers[i].subs_len);
        for (int64_t j = 0; j < pipeline->workers[i].subs_len; ++j)
        {
            if (pipeline->workers[i].subs[j].type == SUBSTITUTION_SYMBOL)
            {
                fprintf(stream, "%s| | | | Substitution %lld : SYMBOL : %s -> %s\n", sindent, j, pipeline->workers[i].subs[j].name, pipeline->workers[i].subs[j].symbol);
            }
            else
            {
                fprintf(stream, "%s| | | | Substitution %lld : PIPELINE : %s ->\n", sindent, j, pipeline->workers[i].subs[j].name);
                print_pipeline(stream, indent + 10, pipeline->workers[i].subs[j].pipeline);
            }
        }
    }
    
    fprintf(stream, "%s| Outputs: %lld\n", sindent, pipeline->outputs_len);
    for (int64_t i = 0; i < pipeline->outputs_len; ++i)
    {
        fprintf(stream, "%s| | Output %lld : %s\n", sindent, i, pipeline->outputs[i].name);
    }
}

static void print_def(FILE *stream, struct definition *definition)
{
    fprintf(stream, "Definition %s\n", definition->name);
    fprintf(stream, "Free vars: %lld\n", definition->free_vars_len);
    for (int64_t i = 0; i < definition->free_vars_len; ++i)
    {
        if (i != 0)
        {
            fprintf(stream, ", ");
        }
        fprintf(stream, "%s", definition->free_vars[i]);
    }
    fprintf(stream, "\n");
    fprintf(stream, "Piped vars: %lld\n", definition->pipeline_vars_len);
    for (int64_t i = 0; i < definition->pipeline_vars_len; ++i)
    {
        if (i != 0)
        {
            fprintf(stream, ", ");
        }
        fprintf(stream, "%s", definition->pipeline_vars[i]);
    }
    fprintf(stream, "\n");
    fprintf(stream, "Pipelines: %lld\n\n", definition->pipelines_len);
    for (int64_t i = 0; i < definition->pipelines_len; ++i)
    {
        print_pipeline(stream, 0, &definition->pipelines[i]);
    }
}

void program_ast_dump(FILE *stream, struct program *program)
{
    /* print all file */
    fprintf(stream, "Program from file %s of %lld lines of code %lld characters total\n", program->filename, program->code_lines, program->source_code_len);
    for (int64_t i = 0; i < program->definitions_len; ++i)
    {
        struct definition *definition = program->definitions[i];
        fprintf(stream, "-------------------- Definition %lld\n", i);
        print_def(stream, definition);
    }
}
