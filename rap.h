
/*----------------------------------------------------------------------+
 |                                                                      |
 |      rap.h                                                           |
 |                                                                      |
 +----------------------------------------------------------------------*/

/*----------------------------------------------------------------------+
 |      Generic value type                                              |
 +----------------------------------------------------------------------*/

typedef unsigned xTypeId_t;

typedef void(xVoidFunction_t)(void);

struct xValue {
        xTypeId_t               typeId;
        union {
                int             Int;
                void            (*VoidFunction)(void);
        } u;
};

// Avoid need for C11 compiler
#define Int          u.Int
#define VoidFunction u.VoidFunction

typedef struct xValue xValue_t;

/*----------------------------------------------------------------------+
 |      Basic C types in Rap                                            |
 +----------------------------------------------------------------------*/

enum {
        xNoneId, // Do we need a None?
        xFalseId,
        xTrueId,
        xIntId,
        xFunctionId // err_t (*fn)(*data, argc, argv[])
};

/*----------------------------------------------------------------------+
 |      Macros to construct basic values from C                         |
 +----------------------------------------------------------------------*/

#define xNone\
        ((xValue_t) { .typeId = xNoneId })

#define xTrue\
        ((xValue_t) { .typeId = xTrueId })

#define xInt(i)\
        ((xValue_t) {.typeId = xIntId, .Int = (i) })

#define xFunction(fn)\
        ((xValue_t) {\
                .typeId = xFunctionId,\
                .VoidFunction = (xVoidFunction_t*)(fn) })

/*----------------------------------------------------------------------+
 |      Macros to test for basic C types                                |
 +----------------------------------------------------------------------*/

#define xIsNone(v)\
        ((v).typeId == xNoneId)

#define xIsTrue(v)\
        ((v).typeId == xTrueId)

#define xIsInt(v)\
        ((v).typeId == xIntId)

#define xIsFunction(v)\
        ((v).typeId == xFunctionId)

/*----------------------------------------------------------------------+
 |      Generic function type                                           |
 +----------------------------------------------------------------------*/

/*
 *  Generic function call interface for Rap functions
 *
 *  Rules are:
 *   1. Checking argv[0] is the responsibility of the caller
 *   2. Checking argv[1..argc-1] is the responsibility of the function
 *   3. A value must be returned in argv[0], unless there is an exception
 *   4. 'data' is to parameterize/scope the function. For builtin functions,
 *       data is not unused (TODO: that sounds wrong)
 */
typedef err_t(xFunction_t)(void *data, int argc, xValue_t argv[]);

/*----------------------------------------------------------------------+
 |      Global interpreter data                                         |
 +----------------------------------------------------------------------*/

struct xRap {
        int dummy;
};

/*
 *  xInit may not give variable size exceptions
 */
err_t xInit(struct xRap *rap);

/*----------------------------------------------------------------------+
 |      The virtual machine                                             |
 +----------------------------------------------------------------------*/

enum {
        vmInt,
        vmSubtractInt,
        vmMultiplyInt,
        vmIncrementInt,
        vmLessEqualInt,
        vmFunctionSubtractInt,
        vmFunctionPrintInt,
        vmCall,
        vmReturn,
        vmDrop,
        vmJump,
        vmJumpF,
        vmJumpT,
        vmGetLocal,
        vmSetLocal,
};

/*
 *  Builtin function to jump to assembled code
 */
xFunction_t xExecute;

/*----------------------------------------------------------------------+
 |                                                                      |
 +----------------------------------------------------------------------*/

