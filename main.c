
/*----------------------------------------------------------------------+
 |                                                                      |
 |      (rap-demo)                                                      |
 |                                                                      |
 +----------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>

#include "cplus.h"

#include "rap.h"

#include "assemble.h"
#include "library.h"

/*----------------------------------------------------------------------+
 |      main                                                            |
 +----------------------------------------------------------------------*/

int main(void)
{
        err_t err = OK;

        struct xRap rap;
        err = xInit(&rap);
        check(err);

        for(;;) {
                size_t len;
                char *line = fgetln(stdin, &len);

                if (line == NULL) break;
                xAssert(len > 0);

                if (line[len-1] != '\n') {
                        xRaise("Incomplete input line");
                }
                line[len-1] = '\0';

                printf("Source: %s\n", line);

                struct tokenize tokenize = {
                        .source = line,
                };

                err = tokenizeStart(&tokenize);
                check(err);

                intList code = emptyList;

                err = compileLine(&tokenize, &code);
                check(err);

                printf("Object:");
                for (int i=0; i<code.len; i++) {
                        printf(" %d", code.v[i]);
                }
                printf(" (length: %d)\n", code.len);

                xValue_t locals[2];

                err = xExecute(code.v, arrayLen(locals) - 1, locals + 1);
                check(err);

                freeList(code);

                err = xPrintInt(NULL, 2, locals);
                check(err);

                putchar('\n');
        }

cleanup:
        return xExitMain(err);
}

/*----------------------------------------------------------------------+
 |                                                                      |
 +----------------------------------------------------------------------*/

