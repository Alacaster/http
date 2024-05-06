#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <shlwapi.h>

int main() {
    char field[] = "connec";
    int fieldlen = strlen(field);
    FILE * file = fopen("output.txt", "r");
    fseek(file, 0, SEEK_END);
    int buflen = ftell(file);
    const char * returnstr;
    char filestr[80000];
    char * str = filestr;
    fseek(file, 0, SEEK_SET);
    fread(str, 1, buflen, file);
    char * strend = str + buflen;
    printf("%.*s", buflen, filestr);
    returnstr = StrStrIA(str, field);
    str = returnstr + strlen(field);
    /*
    {
        char * tempstrend = strend - (fieldlen+1);
        while(str < tempstrend){
            int a;
            putchar('\n');
            for(a = 0; a < fieldlen; a++){
                if(tolower(str[a]) != field[a]) {
                    printf(" %c != %c",str[a], field[a]);
                    str+= a > 0 ? a : 1;
                    goto continuewhile;
                }
                printf("%c",str[a]);
            }
            returnstr = str;
            str += a;
            break;
            continuewhile:
        }
        if(str == tempstrend){
            printf("\nNot Found");
            return 0;
        }
    }*/
    printf("\n\nFound %s\n\n%.40s\n\n%.40s", field, (returnstr - 10 < filestr) ? filestr : returnstr - 10, str);
}