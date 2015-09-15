
/*----------------------------------------------------------------------+
 |                                                                      |
 |      library.c -- functions accessible from Rap                      |
 |                                                                      |
 +----------------------------------------------------------------------*/

#include <stdio.h>

#include "cplus.h"
#include "rap.h"

#include "library.h"

/*----------------------------------------------------------------------+
 |      Functions                                                       |
 +----------------------------------------------------------------------*/

/*----------------------------------------------------------------------+
 |      xSubtractInt                                                    |
 +----------------------------------------------------------------------*/

err_t xSubtractInt(void *data, int argc, xValue_t argv[])
{
        err_t err = OK;

        xAssert(argc == 3);
        xAssert(xIsInt(argv[1]));
        xAssert(xIsInt(argv[2]));

        argv[0] = xInt(argv[1].Int - argv[2].Int);
cleanup:
        return err;
}

/*----------------------------------------------------------------------+
 |      xPrintInt                                                       |
 +----------------------------------------------------------------------*/

err_t xPrintInt(void *data, int argc, xValue_t argv[])
{
        err_t err = OK;

        xAssert(argc == 2);
        xAssert(xIsInt(argv[1]));

        int n = printf("%d\n", argv[1].Int);
        if (n < 0) {
                xRaise("printf failed"); // printf doesn't use errno
        }

        argv[0] = xInt(n);

cleanup:
        return err;
}

/*----------------------------------------------------------------------+
 |                                                                      |
 +----------------------------------------------------------------------*/

