#include "lang.h"

#include "stdio.h"
#include "malloc.h"
#include "inttypes.h"

char *read_code(const char *filename)
{
    FILE *f = fopen(filename, "r");
    if (f == NULL)
    {
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    int64_t file_size = ftell(f);
    rewind(f);
    
    char *buf = malloc(file_size + 1);
    int64_t res = fread(buf, 1, file_size, f);
    buf[res] = 0;
    
    fclose(f);

    return buf;
}

int main(int argc, char **argv)
{
    if (argc == 1)
    {
        printf("need input file\n");
        return 1;
    }
    char *input_file = argv[1];

    char *code = read_code(input_file);
    
    if (code == NULL)
    {
        printf("Error: can't read file\n");
    }

    struct program *program = program_create_from_code(argv[1], code);
}
