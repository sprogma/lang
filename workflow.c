#include "lang.h"

#include "stdio.h"
#include "ctype.h"
#include "stdlib.h"
#include "string.h"
#include "inttypes.h"

#define MAX_FUNCTION_PIPES 4096
#define MAX_FUNCTION_WORKERS 1024
struct name_table
{
    struct pipe *pipes[MAX_FUNCTION_PIPES];
    int64_t pipes_len;

    struct worker *workers[MAX_FUNCTION_WORKERS];
    int64_t workers_len;
};


static struct pipe *add_pipe(struct program *program, char *name, struct code_span code_position)
{
    struct workflow *workflow = &program->workflow;

    if (workflow->pipes_len >= workflow->pipes_alloc)
    {
        workflow->pipes_alloc = 2 * workflow->pipes_alloc + !workflow->pipes_alloc;
        void *new_ptr = realloc(workflow->pipes, sizeof(*workflow->pipes) * workflow->pipes_alloc);
        if (new_ptr == NULL)
        {
            fprintf(stderr, "Error: No memory for WORKFLOW.\n");
            exit(1);
        }
        workflow->pipes = new_ptr;
    }

    workflow->pipes[workflow->pipes_len] = malloc(sizeof(*workflow->pipes[workflow->pipes_len]));
    if (workflow->pipes[workflow->pipes_len] == NULL)
    {
        fprintf(stderr, "Error: No memory for WORKFLOW.\n");
    }
        
    workflow->pipes[workflow->pipes_len]->name = name;
    workflow->pipes[workflow->pipes_len]->code_position = code_position;
    
    return workflow->pipes[workflow->pipes_len++];
}


static struct pipe *get_pipe(struct program *program, struct name_table *name_table, char *name, struct code_span span)
{    
    int i = 0;
    for (; name[i] != '\0'; ++i)
    {
        if (!isdigit(name[i]))
        {
            break;
        }
    }
    if (name[i] == '\0')
    {
        /* this is number */
        struct pipe *pipe = add_pipe(program, "numeric pipeline", span);
        name_table->pipes[name_table->pipes_len++] = pipe;
        return pipe;
    }

    int a = 0;
    for (; a < name_table->pipes_len; ++a)
    {
        if (strcmp(name_table->pipes[a]->name, name) == 0)
        {
            return name_table->pipes[a];
        }
    }

    if (a == name_table->pipes_len)
    {
        program_log(program, LOG_WORKFLOW, LOG_ERROR, "Wrong name of pipe: this pipeline name doesn't exists", span, NULL);
    }
    
    return NULL;
}


static struct worker *add_worker(struct program *program, struct pipeline_worker_definition *worker)
{
    struct workflow *workflow = &program->workflow;

    if (workflow->workers_len >= workflow->workers_alloc)
    {
        workflow->workers_alloc = 2 * workflow->workers_alloc + !workflow->workers_alloc;
        void *new_ptr = realloc(workflow->workers, sizeof(*workflow->workers) * workflow->workers_alloc);
        if (new_ptr == NULL)
        {
            fprintf(stderr, "Error: No memory for WORKFLOW.\n");
            exit(1);
        }
        workflow->workers = new_ptr;
    }

    workflow->workers[workflow->workers_len] = malloc(sizeof(*workflow->workers[workflow->workers_len]));
    if (workflow->workers[workflow->workers_len] == NULL)
    {
        fprintf(stderr, "Error: No memory for WORKFLOW.\n");
    }
        
    workflow->workers[workflow->workers_len]->name = worker->name;
    workflow->workers[workflow->workers_len]->code_position = worker->code_position;
    workflow->workers[workflow->workers_len]->worker_definition = worker;
    workflow->workers[workflow->workers_len]->inputs_len = 0;
    workflow->workers[workflow->workers_len]->outputs_len = 0;
    
    return workflow->workers[workflow->workers_len++];
}

static void build_pipeline(struct program *program, struct name_table *name_table, struct definition *definition, struct pipeline_definition *pipeline)
{
    struct workflow *workflow = &program->workflow;
    (void)workflow;
    
    /* add pipe's name */
    struct worker *worker, *prev_worker;
    for (int64_t j = 0; j < pipeline->workers_len; ++j)
    {
        name_table->workers[name_table->workers_len++] = worker = add_worker(program, &pipeline->workers[j]);
        /* add connection */
        if (j == 0)
        {
            for (int k = 0; k < pipeline->args_len; ++k)
            {
                if (pipeline->args[k].type == ARGUMENT_NAME)
                {
                    /* find pipeline by name */
                    struct pipe *pipe = get_pipe(program, name_table, pipeline->args[k].name, pipeline->args[k].code_position);
                    if (pipe != NULL)
                    {
                        worker->inputs[worker->inputs_len++] = pipe;
                    }
                }
                else
                {
                    /* build this pipeline? */
                    if (pipeline->args[k].pipeline->outputs_len != 0)
                    {
                        program_log(program, LOG_WORKFLOW, LOG_ERROR, "Unsopported for now: inline pipelines, with output pipes", pipeline->args[k].pipeline->code_position, NULL);
                    }
                    build_pipeline(program, name_table, definition, pipeline->args[k].pipeline);
                }
            }
        }
        else
        {
            worker->inputs[worker->inputs_len++] = prev_worker->outputs[prev_worker->outputs_len++] = add_pipe(program, "implict pipe", SPAN(prev_worker->code_position.end, worker->code_position.begin));
        }
        prev_worker = worker;
    }
    /* add pipes to all outputs */
    for (int k = 0; k < pipeline->outputs_len; ++k)
    {
        /* find pipeline by name */
        struct pipe *pipe = get_pipe(program, name_table, pipeline->outputs[k].name, pipeline->outputs[k].code_position);
        if (pipe != NULL)
        {
            worker->outputs[worker->outputs_len++] = pipe;
        }
    }
}

static void update_using_pure_definition(struct program *program, struct definition *definition)
{

    struct workflow *workflow = &program->workflow;
    struct name_table name_table;
    (void)workflow;

    name_table.pipes_len = 0;
    name_table.workers_len = 0;

    /* 1. create all pipelines output pipes */
    for (int64_t i = 0; i < definition->pipelines_len; ++i)
    {
        /* add pipe's name */
        for (int64_t j = 0; j < definition->pipelines[i].outputs_len; ++j)
        {
            name_table.pipes[name_table.pipes_len++] = add_pipe(program, 
                                                                definition->pipelines[i].outputs[j].name, 
                                                                definition->pipelines[i].outputs[j].code_position);
        }
    }

    /* connect all workers using pipes */
    for (int64_t i = 0; i < definition->pipelines_len; ++i)
    {
        build_pipeline(program, &name_table, definition, &definition->pipelines[i]);
    }
    printf("\n\nadd %lld pipes\n", name_table.pipes_len);
    for (int i = 0; i < name_table.pipes_len; ++i)
    {
        printf("pipe %d: %s\n", i, name_table.pipes[i]->name);
    }
    printf("\n\nadd %lld workers\n", name_table.workers_len);
    for (int i = 0; i < name_table.workers_len; ++i)
    {
        printf("worker %d: %s\n", i, name_table.workers[i]->name);
        printf("inputs: ");
        for (int a = 0; a < name_table.workers[i]->inputs_len; ++a)
        {
            printf("%s ", name_table.workers[i]->inputs[a]->name);
        }
        printf("\n");
        printf("outputs: ");
        for (int a = 0; a < name_table.workers[i]->outputs_len; ++a)
        {
            printf("%s ", name_table.workers[i]->outputs[a]->name);
        }
        printf("\n");
    }
}

void program_get_workflow(struct program *program)
{
    printf("get workflow...\n");
    
    struct workflow *workflow = &program->workflow;
    workflow->pipes = NULL;
    workflow->pipes_len = 0;
    workflow->pipes_alloc = 0;
    workflow->workers = NULL;
    workflow->workers_len = 0;
    workflow->workers_alloc = 0;

    int64_t empty = 1;
    for (int64_t i = 0; i < program->definitions_len; ++i)
    {
        if (program->definitions[i]->free_vars_len == 0 && 
            program->definitions[i]->pipeline_vars_len == 0)
        {        
            empty = 0;
            update_using_pure_definition(program, program->definitions[i]);
        }
    }
    if (empty)
    {
        program_log(program, LOG_WORKFLOW, LOG_ERROR, "Wrong function: no pure functions to build found", SPAN(0, 0), NULL);
    }
}
