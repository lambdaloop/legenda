#include <stdio.h>
#include <string.h>
#include <stdlib.h>

typedef unsigned short USHORT;

//each function returns its contribution to the checksum
void write_header1(FILE *fout); //header1 doesn't count towards checksum
USHORT write_header2(USHORT data_length, char *name, FILE *fout);

USHORT write_little_endian(USHORT data, FILE *fout);

//write_data now adds the length of the string written into the length reference
USHORT write_data(char *data, int *length, FILE *fout);
USHORT write_data_n(char *data, int n, FILE *fout);

/*
//like write_data, expect writes from a file
//converts characters to calculator characters!!
USHORT write_file(FILE *fin, int *length, FILE *fout);

USHORT write_text(char *text, int *length, FILE *fout);
*/

int add_screen_endings(char *text, char* out);

int create_appvar(char *text, int text_length, char *filename, char* appvar_name);

char calc_char(char asciiChar);
int char_col(char c);


void printUsage(char *progName)
{
    printf("%s input_file output_file appvar_name\n", progName);
    printf("\nConverts input_file (text file) into AppVar for TI-83+ (and on).\n");
    printf("Result is stored in file output_file, with name appvar_name\n(as it will appear on calculator).\n");
}

#define MAX_CHUNK_LEN 64000
int main(int argc, char *argv[])
{
    if(argc != 4)
    {
        printUsage(argv[0]);
        exit(0);
    }

    FILE *fin;
    fin  = fopen(argv[1], "r");

    char *filename = argv[2];

    char *last_dot;
    last_dot = strrchr(filename, '.');
    if(last_dot)
        *last_dot = 0;

    char appvar_name[8];
    int i;
    for(i=0; i<8; i++)
        appvar_name[i] = 0;

    strcpy(appvar_name, argv[3]);

    if(appvar_name[7] != 0)
    {
        printf("Invalid AppVar name. Name must be 7 chars or less.");
        exit(1);
    }

    fseek(fin, 0L, SEEK_END);
    long file_len = ftell(fin);     //get file len
    rewind(fin);               // back to start of file


    char *cFile;
    cFile = calloc(file_len + 1, sizeof(char));

    if(cFile==NULL )
    {
        printf("\nInsufficient memory to read file.\n");
        return 1;
    }

    fread(cFile, file_len, 1, fin);

    char *text;
    text = calloc(file_len + file_len/100 + 10, sizeof(char));
    text[0] = 0xFF; //start
    int text_len = add_screen_endings(cFile, text+1);
    text_len++;
    text[text_len] = 0xFF; //end
    text_len++;

    free(cFile);

    char new_filename[10+strlen(filename)];


    if(text_len < MAX_CHUNK_LEN)
    {
        sprintf(new_filename, "%s.8xv", filename);
        create_appvar(text, text_len, new_filename, appvar_name);
    }
    else
    {
        char c;
        int start = 0;
        int end;
        int var_num = 0;

        int name_len = strlen(appvar_name);


        while(start < text_len)
        {
            end = start+MAX_CHUNK_LEN-2;
            if(end > text_len)
                end = text_len-2;
            else
            {
                while(text[end] != 0) end--; //find last zero that we can use
            }
            end++;
            c = text[end];
            text[end] = 0xFE;
            if(start > 0)
                text[start] = 0xFE;

            appvar_name[name_len] = var_num+48;
            sprintf(new_filename, "%s_%d.8xv", filename, var_num);

            create_appvar(text+start, end-start+1, new_filename, appvar_name);

            text[end] = c;
            start = end+1;
            var_num++;
        }
    }

    free(text);

}

//write appvar text (with length text_len) to file with name filename.
//name of appvar itself (as seen on calc) is appvar_name (must be 8 bytes)
int create_appvar(char *text, int text_length, char *filename, char* appvar_name)
{


    FILE *fout;

    fout = fopen(filename, "wb");

    USHORT checksum = 0;

    write_header1(fout);

    long header_pos = ftell(fout);


    //make space for length + header + prog length (2 bytes + 17 bytes + 2 bytes)
    int i;
    for(i=0; i<21; i++)
        putc(0, fout);

    int length = text_length;
    //random data stuff

    //checksum += write_data("data data data", &length, fout); //MAIN DATA!!
    checksum += write_data_n(text, text_length, fout);

    //goto header spot
    fseek(fout, header_pos, SEEK_SET);

    printf("%i\n", length);

    checksum += write_header2(length, appvar_name, fout);

    //goto end
    fseek(fout, 0, SEEK_END);

    write_little_endian(checksum, fout); //checksum

    fclose(fout);

    return 0;

}

void write_header1(FILE *fout)
{
    int i;

    //1st magic word
    fputs("**TI83F*", fout);
    //2nd magic word
    putc(0x1A, fout);
    putc(0x0A, fout);
    putc(0x00, fout);

    //file comment (need exactly 42 chars)
    char comment[42];
    for(i=0; i<42; i++)
        comment[i] = 0; //fill with nulls

    strcpy(comment, "LEGENDA text file.");

    for(i=0; i<42; i++)
        putc(comment[i], fout);

}

//name is a string of length 8
USHORT write_header2(USHORT data_length, char *name, FILE *fout)
{
    USHORT checksum = 0;

    //length of this header and data (not in checksum)
    write_little_endian(data_length+17, fout);

    checksum += write_little_endian(0x0D, fout); //magic word

    checksum += write_little_endian(data_length+2, fout);  //length of data
    checksum += putc(0x15, fout); //type. 0x15 = Appvar

    //name
    checksum += write_data_n(name, 8, fout);

    checksum += putc(0, fout); //version
    checksum += putc(0x80, fout); //flag. (0=RAM, 0x80 = Archived)

    checksum += write_little_endian(data_length+2, fout); //length of data...again

    checksum += write_little_endian(data_length, fout); //length of program....

    return checksum;
}


USHORT write_little_endian(USHORT data, FILE *fout)
{
    USHORT checksum = data & 255;
    putc(data & 255, fout);
    data = data >> 8;
    checksum += putc(data & 255, fout);
    return checksum;
}

USHORT write_data(char *data, int *length, FILE *fout)
{
    //*length = 0;
    USHORT checksum = 0;
    char *ptr;
    for(ptr = data; *ptr; ptr++)
    {
        checksum += putc(*ptr, fout);
        *length += 1;
    }
    return checksum;
}

USHORT write_data_n(char *data, int n, FILE *fout)
{
    USHORT checksum=0;
    int i;
    for(i=0; i<n; i++)
    {
        checksum += putc(data[i], fout);
    }
    return checksum;
}

int add_screen_endings(char *text, char* out)
{
    /*
    int length = 0;

    char *currChar;
    for(currChar = text; *currChar; currChar++)
        length++; //find length of text
    */


    char soFar[1000];
    char *currChar, c, *last_ptr, prevc;
    int col, row, last_space, index, last_col;
    int out_index, i;

    row = col = 1;
    last_space = last_col = 0;
    index = 0;
    prevc = 0;
    out_index = 0;

    for(currChar = text; *currChar; currChar++)
    {
        c = calc_char(*currChar);
        if(c == '\r')
            continue;
        if(c == '\n' && prevc != '\n')
        {
//            c = ' ';
            prevc = '\n';
        }
        else
            prevc = c;

        if(c == ' ')
        {
            //checking for last space to break page at best part
            last_space = index;
            last_ptr = currChar;
            last_col = col;
        }
        soFar[index] = c;
        index++;
        col += char_col(c);

        if(col > 96 || c == '\n')
        {
            row += 6;
            col = 1 + (last_space && c != '\n' ? col-last_col+1 : 0);
            if(row > 59)
            {
                if(last_space)
                {
                    index = last_space;
                    currChar = last_ptr;
                    printf("%c%c%c\n", text[index-3], text[index-2], text[index-1]);
                }

                for(i=0; i<index; i++)
                    out[out_index+i] = soFar[i];
                out_index += index;
                out[out_index] = 0;
                out_index++;
                /*
                checksum += write_data_n(soFar, index, fout);
                putc(0, fout); //end screen
                *length += index + 1;
                */
                index = 0;
                row = 1;
            }
            last_space = 0;
            last_col = 0;
        }

    }
    if(row != 1 && col != 1)
    {
        for(i=0; i<index; i++)
            out[out_index+i] = soFar[i];
        out_index += index;
        out[out_index] = 0;
        out_index++;
/*
        checksum += write_data_n(soFar, index, fout); //index already increased
        putc(0, fout); //end screen
        *length += index+1;
*/
    }

/*
    char *newOut;

    newOut = calloc(out_index, sizeof(char));
    for(i=0; i<out_index; i++)
        newOut[i] = out[i];

    *out_length = out_index;
*/
    return out_index;
}
/*
USHORT write_text(char *text, int *length, FILE *fout)
{
    USHORT checksum = 0;

    char soFar[1000];
    char *currChar, c, *last_ptr, prevc;
    int col, row, last_space, index, last_col;
    row = col = 1;
    last_space = last_col = 0;
    index = 0;
    prevc = 0;

    checksum += putc(0xFF, fout); //start of data

    for(currChar = text; *currChar; currChar++)
    {
        c = calc_char(*currChar);
        if(c == '\r')
            continue;
        if(c == '\n' && prevc != '\n')
        {
            prevc = '\n';
            //c = ' ';
        }
        else
            prevc = c;

        if(c == ' ')
        {
            //checking for last space to break page at best part
            last_space = index;
            last_ptr = currChar;
            last_col = col;
        }
        soFar[index] = c;
        index++;
        col += char_col(c);

        if(col > 96 || c == '\n')
        {
            row += 6;
            col = 1 + (last_space ? col-last_col : 0);
            if(row >= 59)
            {
                if(last_space)
                {
                    index = last_space;
                    currChar = last_ptr;
                }

                checksum += write_data_n(soFar, index, fout);
                putc(0, fout); //end screen
                *length += index + 1;

                index = 0;
                row = 1;
            }
            last_space = 0;
        }

    }
    if(row != 1 && col != 1)
    {
        checksum += write_data_n(soFar, index, fout); //index already increased
        putc(0, fout); //end screen
        *length += index+1;
    }

    checksum += putc(0xFF, fout); //end of data

    *length += 2; //beggining + end;

    return checksum;
}
USHORT write_file(FILE *fin, int *length, FILE *fout)
{
    #define MAX_FILE_LEN 32000

    fseek(fin, 0L, SEEK_END);
    long file_len = ftell(fin);     //get file len
    rewind(fin);               // back to start of file



    char *cFile;
    cFile = calloc(MAX_FILE_LEN + 1, sizeof(char));

    if(cFile==NULL )
    {
        printf("\nInsufficient memory to read file.\n");
        return 0;
    }

    fread(cFile, MAX_FILE_LEN, 1, fin);

    USHORT checksum = write_text(cFile, length, fout);

    free(cFile);

    return checksum;
}
*/

//simple converter from ascii to calculator
char calc_char(char ascii_char)
{
    if(ascii_char == '[')
        return 0xC1;
    else
        return ascii_char;

}

//given a calc character, returns number of columns it takes up
int char_col(char c)
{

   static int ref[] =
   {
//      0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F
        0, 6, 4, 4, 4, 4, 4, 5, 4, 4, 0, 4, 4, 4, 4, 5, // 0
        4, 5, 4, 4, 5, 5, 4, 5, 6, 5, 4, 4, 5, 6, 4, 4, // 1
        4, 2, 4, 6, 6, 4, 5, 2, 3, 3, 6, 4, 3, 4, 2, 4, // 2 //count space as 4
        4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 2, 3, 4, 4, 4, 4, // 3
        6, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, // 4
        4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 3, 4, 4, // 5
        3, 4, 4, 4, 4, 4, 3, 4, 4, 2, 4, 4, 3, 6, 4, 4, // 6
        4, 4, 4, 3, 3, 4, 4, 6, 4, 4, 5, 4, 2, 4, 5, 4, // 7
        4, 3, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, // 8
        5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, // 9
        4, 4, 6, 6, 6, 6, 6, 6, 6, 6, 4, 4, 4, 4, 5, 5, // A
        5, 5, 4, 4, 5, 5, 3, 3, 4, 4, 2, 5, 4, 5, 6, 4, // B
        4, 3, 4, 5, 6, 5, 5, 5, 5, 6, 6, 4, 4, 4, 3, 4, // C
        4, 4, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5, 4, // D
        3, 6, 5, 6, 5, 6, 5, 6, 5, 6, 5, 6, 5, 0, 0, 0, // E
        0, 4, 6, 6, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0  // F
   };

   return ref[c];
}
