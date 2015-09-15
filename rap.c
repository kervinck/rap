
/*----------------------------------------------------------------------+
 |                                                                      |
 |      rap-demo.c                                                      |
 |                                                                      |
 +----------------------------------------------------------------------*/

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "cplus.h"
#include "rap.h"

#include "library.h"

/*----------------------------------------------------------------------+
 |      Data                                                            |
 +----------------------------------------------------------------------*/

static const xTypeId_t boolTypeIds[] = { xFalseId, xTrueId };

/*----------------------------------------------------------------------+
 |      Global interpreter data                                         |
 +----------------------------------------------------------------------*/

/*
 *  xInit may not give variable size exceptions
 */

err_t xInit(struct xRap *rap)
{
        err_t err = OK;

        xAssert(rap != NULL);

        // TODO: Initialize typeId generator

        // TODO: Initialize basic method tables

        // TODO: Initialize assembler jump tables

        rap->dummy = 1;

cleanup:
        return err;
}

/*----------------------------------------------------------------------+
 |      The virtual machine                                             |
 +----------------------------------------------------------------------*/

/*
 *  Builtin function to jump to assembled code
 */
err_t xExecute(void *data, int argc, xValue_t argv[])
{
        err_t err = OK;

        char *pc = data;

        int nrLocals = *(int *)pc;
        pc += sizeof(int);

        xValue_t locals[nrLocals];
        xAssert(nrLocals > 0);

        xValue_t *sp = &locals[0];

        for (;;) {
                switch (*(int *)pc) {
                case vmInt:
                        pc += sizeof(int);
                        *sp++ = xInt(*(int *)pc);
                        pc += sizeof(int);
                        continue;

                case vmSubtractInt:
                        pc += sizeof(int);
                        sp--;
                        sp[-1].Int -= sp[0].Int;
                        continue;

                case vmMultiplyInt:
                        pc += sizeof(int);
                        sp--;
                        sp[-1].Int *= sp[0].Int;
                        continue;

                case vmIncrementInt:
                        pc += sizeof(int);
                        sp[-1].Int++;
                        continue;

                case vmLessEqualInt:
                        pc += sizeof(int);
                        sp--;
                        sp[-1].typeId = boolTypeIds[ (sp[-1].Int <= sp[0].Int) ];
                        continue;

                case vmFunctionSubtractInt:
                        pc += sizeof(int);
                        *sp++ = xFunction(xSubtractInt);
                        continue;

                case vmFunctionPrintInt:
                        pc += sizeof(int);
                        *sp++ = xFunction(xPrintInt);
                        continue;

                case vmCall:
                        pc += sizeof(int);
                        int argc2 = *(int *)pc;
                        sp -= argc2;
                        pc += sizeof(int);
                        xAssert(sp->typeId == xFunctionId);
                        xFunction_t *fn = (xFunction_t *) sp->VoidFunction;
                        err = fn(NULL, argc2, sp);
                        check(err);
                        sp++;
                        continue;

                case vmReturn:
                        argv[0] = locals[0];
                        goto cleanup;

                case vmDrop:
                        sp -= ((int *)pc)[1];
                        pc += 2 * sizeof(int);
                        continue;

                case vmJump:
                        pc += ((int *)pc)[1];
                        continue;

                case vmJumpF:
                        if (sp[-1].typeId == xFalseId) {
                                pc += ((int *)pc)[1];
                        } else {
                                pc += 2 * sizeof(int);
                        }
                        continue;

                case vmJumpT:
                        if (sp[-1].typeId == xTrueId) {
                                pc += ((int *)pc)[1];
                        } else {
                                pc += 2 * sizeof(int);
                        }
                        continue;

                case vmGetLocal:
                        ;
                        int offset = ((int *)pc)[1];
                        xAssert(offset >= 0);
                        xAssert(offset < sp - &locals[0]);
                        pc += 2 * sizeof(int);
                        *sp++ = locals[offset];
                        continue;

                case vmSetLocal:
                        offset = ((int *)pc)[1];
                        xAssert(offset >= 0);
                        xAssert(offset < sp - &locals[0]);
                        pc += 2 * sizeof(int);
                        locals[offset] = sp[-1];;
                        continue;

                default:
                        xAssert(false);
                }
        }

cleanup:
        return err;
}

/*----------------------------------------------------------------------+
 |                                                                      |
 +----------------------------------------------------------------------*/

