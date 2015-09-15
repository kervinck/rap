
/*----------------------------------------------------------------------+
 |                                                                      |
 |      assemble.c                                                      |
 |                                                                      |
 +----------------------------------------------------------------------*/

#include <stdlib.h>
#include <string.h>

#include "cplus.h"

#include "assemble.h"
#include "rap.h" // for vm instruction set

/*----------------------------------------------------------------------+
 |      Definitions                                                     |
 +----------------------------------------------------------------------*/

// My own interface
enum tokenType {
        tokenOpen,
        tokenClose,
        tokenOpcode,
        tokenSpace,
        tokenColon,
        tokenInt,
        tokenHexInt,
        tokenFloat,
        tokenHexFloat,
        tokenSymbol,
        tokenGet,
        tokenSet,
        tokenEnd,
};

// Be independent of locales influencing ctype.h (but still assume ASCII-like)
#define isLower(c) ('a' <= (c) && (c) <= 'z')
#define isUpper(c) ('A' <= (c) && (c) <= 'Z')
#define isDigit(c) ('0' <= (c) && (c) <= '9')
#define isHexDigit(c) (isDigit(c) || ('a' <= (c) && (c) <= 'f'))
#define isSymbolChar(c) (isLower(c) || isUpper(c) || isDigit(c) || (c) == '_')

struct vm {
        int sp;
        int maxSp;
        intList *code;
        intList jumps;
};

/*----------------------------------------------------------------------+
 |      Data                                                            |
 +----------------------------------------------------------------------*/

static
const char * const opcodes[] = {
        "int", "t", "f", "z", "flt", "dbl",
        "move", "swap",
        "neg", "add", "sub", "mul", "div", "inc", "dec",                // divm abs max min
                                                                        // fadd fsub fmul fdiv fneg fabs fmax fmin fexp fln finv fsqt fsig
        "not", "and", "or", "xor", "shl", "shr", "rol", "ror",          // bcnt blzc btzc bext bdep

        "call", "ret",
        "if", "ifn", "ifeq", "ifne", "iflt", "ifgt", "ifle", "ifge",
        "loop", "brk", "cont",
        "getl", "setl",
        "le",
};

typedef err_t Compiler_t(struct tokenize *T, struct vm *out);

static Compiler_t compileInt, compileT, compileF, compileZ, compileFlt, compileDbl;
static Compiler_t compileMove, compileSwap;
static Compiler_t compileNeg, compileAdd, compileSub, compileMul, compileDiv, compileInc, compileDec;
static Compiler_t compileNot, compileAnd, compileOr, compileXor, compileShl, compileShr, compileRol, compileRor;
static Compiler_t compileCall, compileRet;
static Compiler_t compileIf, compileIfn, compileIfeq, compileIfne, compileIflt, compileIfgt, compileIfle, compileIfge;
static Compiler_t compileLoop, compileBrk, compileCont;
static Compiler_t compileGetl, compileSetl;
static Compiler_t compileLe;

static
Compiler_t *jumpTable[] = {
        compileInt, compileT, compileF, compileZ, compileFlt, compileDbl,
        compileMove, compileSwap,
        compileNeg, compileAdd, compileSub, compileMul, compileDiv, compileInc, compileDec,
        compileNot, compileAnd, compileOr, compileXor, compileShl, compileShr, compileRol, compileRor,
        compileCall, compileRet,
        compileIf, compileIfn, compileIfeq, compileIfne, compileIflt, compileIfgt, compileIfle, compileIfge,
        compileLoop, compileBrk, compileCont,
        compileGetl, compileSetl,
        compileLe,
};

#define next(T) do{\
        (T)->source += (T)->tokenLen;\
        (T)->tokenId = nextToken(T);\
}while(0)

#define skipSpaces(T) do{\
        while ((T)->tokenId == tokenSpace) next(T);\
}while(0)

#define skip(T, t) do{\
        if ((T)->tokenId != (t)) {\
                xRaise("Error: bad syntax ("#t" expected)");\
        }\
        next(T);\
}while(0)

#define skipv(T, t, v) do{\
        if ((T)->tokenId != (t) || (T)->tokenValue != (v)) {\
                xRaise("Error: bad syntax ("#t"="#v" expected)");\
        }\
        next(T);\
}while(0)

/*----------------------------------------------------------------------+
 |      nextToken                                                       |
 +----------------------------------------------------------------------*/

int nextToken(struct tokenize *T)
{
        int n;

        switch (*T->source) {

        case 'a': case 'b': case 'c': case 'd': case 'e':
        case 'f': case 'g': case 'h': case 'i': case 'j':
        case 'k': case 'l': case 'm': case 'n': case 'o':
        case 'p': case 'q': case 'r': case 's': case 't':
        case 'u': case 'v': case 'w': case 'x': case 'y':
        case 'z':

                for (n=1; isLower(T->source[n]); n++)
                        ;

                if (isUpper(T->source[n]) || isDigit(T->source[n]))
                        break;

                for (int i=0; i<arrayLen(opcodes); i++) {
                        const char *k = opcodes[i];
                        int j;
                        for (j=0; j<n && k[j]==T->source[j]; j++)
                                ;
                        if (j==n && k[j] == '\0') {
                                T->tokenLen = n;
                                T->tokenValue = i; // side effect
                                return tokenOpcode;
                        }
                }
                break;

        // TODO: negative values ('-')
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
                ;
                int value = T->source[0] - '0';
                for (n=1; isDigit(T->source[n]); n++) {
                        // TODO: overflow detection
                        value = (10 * value) + (T->source[n] - '0');
                }
                if (isSymbolChar(T->source[n]))
                        break;
                T->tokenLen = n;
                T->tokenValue = value;
                return tokenInt;

        case '$':
                for (n=1; isHexDigit(T->source[n]); n++)
                        ;
                if (isSymbolChar(T->source[n]))
                        break;
                if (n > 1) {
                        T->tokenLen = n;
                        return tokenHexInt;
                }
                break;

        case ':':
                T->tokenLen = 1;
                return tokenColon;

        case '(':
                T->tokenLen = 1;
                return tokenOpen;

        case ')':
                T->tokenLen = 1;
                return tokenClose;

        case '?':
                T->tokenLen = 1;
                return tokenGet;

        case '!':
                T->tokenLen = 1;
                return tokenSet;

        case '`': // TODO: alternative syntax is quote ('\'') or carrot ('^')
                for (n=1; isSymbolChar(T->source[n]); n++)
                        ;
                if (n > 1) {
                        T->tokenLen = n;
                        return tokenSymbol;
                }
                break;

        case ' ': case '\t': case '\r': case '\n':
                T->tokenLen = 1;
                return tokenSpace;

        case '\0':
                T->tokenLen = 0;
                return tokenEnd;

        default:
                break;
        }
        return -1; // error
}

/*----------------------------------------------------------------------+
 |      tokenizeStart                                                   |
 +----------------------------------------------------------------------*/

err_t tokenizeStart(struct tokenize *T)
{
        err_t err = OK;
        T->tokenId = nextToken(T);
        skipSpaces(T);
        return err;
}

/*----------------------------------------------------------------------+
 |      emit vm code                                                    |
 +----------------------------------------------------------------------*/

// TODO: move this into vm.c (because typechecking happens there)

static
err_t emitLoadint(struct vm *out, int value)
{
        err_t err = OK;
        listPush(*out->code, vmInt);
        listPush(*out->code, value);
        out->sp++;
        out->maxSp = max(out->maxSp, out->sp);
cleanup:
        return err;
}

static
err_t emitSubtractInt(struct vm *out)
{
        err_t err = OK;
        xAssert(out->sp >= 2);
        listPush(*out->code, vmSubtractInt);
        out->sp--;
cleanup:
        return err;
}

static
err_t emitMultiplyInt(struct vm *out)
{
        err_t err = OK;
        xAssert(out->sp >= 2);
        listPush(*out->code, vmMultiplyInt);
        out->sp--;
cleanup:
        return err;
}

static
err_t emitIncrementInt(struct vm *out)
{
        err_t err = OK;
        xAssert(out->sp >= 1);
        listPush(*out->code, vmIncrementInt);
cleanup:
        return err;
}

static
err_t emitLessEqualInt(struct vm *out)
{
        err_t err = OK;
        xAssert(out->sp >= 2);
        listPush(*out->code, vmLessEqualInt);
        out->sp--;
cleanup:
        return err;
}

static
err_t emitGetLocal(struct vm *out, int offset)
{
        err_t err = OK;

        xAssert(offset >= 0);
        xAssert(offset < out->sp);
        listPush(*out->code, vmGetLocal);
        listPush(*out->code, offset);
        out->sp++;
        out->maxSp = max(out->maxSp, out->sp);
cleanup:
        return err;
}

static
err_t emitSetLocal(struct vm *out, int offset)
{
        err_t err = OK;
        xAssert(offset >= 0);
        xAssert(offset < out->sp);
        listPush(*out->code, vmSetLocal);
        listPush(*out->code, offset);
cleanup:
        return err;
}

static
err_t emitSymbol(struct vm *out, const char *name, int len)
{
        err_t err = OK;

        int symbol = -1;
        if (0==strncmp(name, "printInt", len)) { // TODO: strncmp here is wrong!
                symbol = vmFunctionPrintInt;
        }
        if (0==strncmp(name, "subtractInt", len)) { // TODO: strncmp here is wrong!
                symbol = vmFunctionSubtractInt;
        }
        xAssert(symbol != -1);

        listPush(*out->code, symbol);
        out->sp++;
        out->maxSp = max(out->maxSp, out->sp);
cleanup:
        return err;
}

static
err_t emitCall(struct vm *out, int argc)
{
        err_t err = OK;
        xAssert(1 <= argc && argc <= out->sp);
        listPush(*out->code, vmCall);
        listPush(*out->code, argc);
        out->sp += argc - 1;
cleanup:
        return err;
}

static
err_t emitReturn(struct vm *out)
{
        err_t err = OK;
        listPush(*out->code, vmReturn);
cleanup:
        return err;
}

static
err_t emitDrop(struct vm *out, int n)
{
        err_t err = OK;

        listPush(*out->code, vmDrop);
        listPush(*out->code, n);
cleanup:
        return err;
}

static
err_t emitJump(struct vm *out, int pc)
{
        err_t err = OK;

        int offset = (pc - out->code->len) * sizeof(int);
        listPush(*out->code, vmJump);
        listPush(*out->code, offset);
cleanup:
        return err;
}

static
err_t emitJumpF(struct vm *out, int pc)
{
        err_t err = OK;

        int offset = (pc - out->code->len) * sizeof(int);
        listPush(*out->code, vmJumpF);
        listPush(*out->code, offset);
cleanup:
        return err;
}

static
err_t emitJumpT(struct vm *out, int pc)
{
        err_t err = OK;

        int offset = (pc - out->code->len) * sizeof(int);
        listPush(*out->code, vmJumpT);
        listPush(*out->code, offset);
cleanup:
        return err;
}

/*----------------------------------------------------------------------+
 |      compilation                                                     |
 +----------------------------------------------------------------------*/

static
err_t compileSymbol(struct tokenize *T, struct vm *out)
{
        err_t err = OK;

        err = emitSymbol(out, T->source+1, T->tokenLen-1);
        check(err);

        skip(T, tokenSymbol);
        skipSpaces(T);
cleanup:
        return err;
}

/*
 * There are several types of expressions
 *  ( op ... )          canonical notation
 *  op : args ...       alias for: (op args ...)
 *  ? ... arg           alias for: (get ...)
 *  ! ... arg           alias for: (set ...)
 */

static
err_t compileExpression(struct tokenize *T, struct vm *out)
{
        err_t err = OK;

        switch (T->tokenId) {

        case tokenOpen:
                skip(T, tokenOpen);
                skipSpaces(T);

                if (T->tokenId != tokenOpcode) {
                        xRaise("Opcode expected");
                }

                err = jumpTable[T->tokenValue](T, out);
                check(err);

                if (T->tokenId != tokenClose) {
                        xRaise("Error: too many arguments");\
                }

                skip(T, tokenClose);
                skipSpaces(T);
                break;

        case tokenOpcode:
                xRaise("Not implemented");

        case tokenSymbol:
                err = compileSymbol(T, out);
                check(err);
                break;

        default:
                xRaise("Expression expected");
        }
        skipSpaces(T);
cleanup:
        return err;
}

static err_t compileInt(struct tokenize *T, struct vm *out)
{
        err_t err = OK;

        skip(T, tokenOpcode);
        skipSpaces(T);

        skip(T, tokenInt);
        skipSpaces(T);

        err = emitLoadint(out, T->tokenValue);
        check(err);
cleanup:
        return err;
}

err_t compileLine(struct tokenize *T, intList *code)
{
        err_t err = OK;

        struct vm out = {
                .sp = 0,
                .maxSp = 0,
                .code = code,
                .jumps = emptyList,
        };

        code->len = 0;
        listPush(*code, 0); // dummy, to become local storage length

        while (T->tokenId != tokenClose && T->tokenId != tokenEnd) {
                err = compileExpression(T, &out);
                check(err);
        }

        err = emitReturn(&out);
        check(err);

        code->v[0] = out.maxSp;
cleanup:

        freeList(out.jumps);

        return err;
}

static err_t compileT(struct tokenize *T, struct vm *out) { err_t err; xRaise("Not implemented"); cleanup: return err; }
static err_t compileF(struct tokenize *T, struct vm *out) { err_t err; xRaise("Not implemented"); cleanup: return err; }
static err_t compileZ(struct tokenize *T, struct vm *out) { err_t err; xRaise("Not implemented"); cleanup: return err; }
static err_t compileFlt(struct tokenize *T, struct vm *out) { err_t err; xRaise("Not implemented"); cleanup: return err; }
static err_t compileDbl(struct tokenize *T, struct vm *out) { err_t err; xRaise("Not implemented"); cleanup: return err; }

static err_t compileMove(struct tokenize *T, struct vm *out) { err_t err; xRaise("Not implemented"); cleanup: return err; }
static err_t compileSwap(struct tokenize *T, struct vm *out) { err_t err; xRaise("Not implemented"); cleanup: return err; }

static err_t compileNeg(struct tokenize *T, struct vm *out) { err_t err; xRaise("Not implemented"); cleanup: return err; }
static err_t compileAdd(struct tokenize *T, struct vm *out) { err_t err; xRaise("Not implemented"); cleanup: return err; }

static err_t compileSub(struct tokenize *T, struct vm *out)
{
        err_t err = OK;

        skip(T, tokenOpcode);
        skipSpaces(T);
        err = compileExpression(T, out);
        check(err);
        err = compileExpression(T, out);
        check(err);
        err = emitSubtractInt(out);
        check(err);
cleanup:
        return err;
}

static err_t compileMul(struct tokenize *T, struct vm *out)
{
        err_t err = OK;

        skip(T, tokenOpcode);
        skipSpaces(T);
        err = compileExpression(T, out);
        check(err);
        err = compileExpression(T, out);
        check(err);
        err = emitMultiplyInt(out);
        check(err);
cleanup:
        return err;
}

static err_t compileDiv(struct tokenize *T, struct vm *out) { err_t err; xRaise("Not implemented"); cleanup: return err; }

static err_t compileInc(struct tokenize *T, struct vm *out)
{
        err_t err = OK;

        skip(T, tokenOpcode);
        skipSpaces(T);
        err = compileExpression(T, out);
        check(err);
        err = emitIncrementInt(out);
        check(err);
cleanup:
        return err;
}

static err_t compileDec(struct tokenize *T, struct vm *out) { err_t err; xRaise("Not implemented"); cleanup: return err; }

static err_t compileNot(struct tokenize *T, struct vm *out) { err_t err; xRaise("Not implemented"); cleanup: return err; }
static err_t compileAnd(struct tokenize *T, struct vm *out) { err_t err; xRaise("Not implemented"); cleanup: return err; }
static err_t compileOr(struct tokenize *T, struct vm *out) { err_t err; xRaise("Not implemented"); cleanup: return err; }
static err_t compileXor(struct tokenize *T, struct vm *out) { err_t err; xRaise("Not implemented"); cleanup: return err; }
static err_t compileShl(struct tokenize *T, struct vm *out) { err_t err; xRaise("Not implemented"); cleanup: return err; }
static err_t compileShr(struct tokenize *T, struct vm *out) { err_t err; xRaise("Not implemented"); cleanup: return err; }
static err_t compileRol(struct tokenize *T, struct vm *out) { err_t err; xRaise("Not implemented"); cleanup: return err; }
static err_t compileRor(struct tokenize *T, struct vm *out) { err_t err; xRaise("Not implemented"); cleanup: return err; }

static err_t compileCall(struct tokenize *T, struct vm *out)
{
        err_t err = OK;

        skip(T, tokenOpcode); // "call"
        skipSpaces(T);

        int argc = 0;
        do {
                err = compileExpression(T, out);
                check(err);
                argc++;
        } while (T->tokenId != tokenClose);

        err = emitCall(out, argc);
        check(err);
cleanup:
        return err;
}       

static err_t compileRet(struct tokenize *T, struct vm *out) { err_t err; xRaise("Not implemented"); cleanup: return err; }

static err_t compileIf(struct tokenize *T, struct vm *out) { err_t err; xRaise("Not implemented"); cleanup: return err; }

static err_t compileIfn(struct tokenize *T, struct vm *out)
{
        err_t err = OK;

        skip(T, tokenOpcode); // "ifn"
        skipSpaces(T);

        err = compileExpression(T, out); // condition
        check(err);

        int jumpPc = out->code->len;
        err = emitJumpT(out, jumpPc); // dummy operand
        check(err);

        int n = 0;
        while (T->tokenId != tokenClose) {
                err = compileExpression(T, out); // condition
                check(err);
                n++;
        }

        err = emitDrop(out, n);
        check(err);

        xAssert(out->code->v[jumpPc+0] == vmJumpT);
        xAssert(out->code->v[jumpPc+1] == 0);

        out->code->v[jumpPc+1] = (out->code->len - jumpPc) * sizeof(int);
cleanup:
        return err;
}

static err_t compileIfeq(struct tokenize *T, struct vm *out) { err_t err; xRaise("Not implemented"); cleanup: return err; }
static err_t compileIfne(struct tokenize *T, struct vm *out) { err_t err; xRaise("Not implemented"); cleanup: return err; }
static err_t compileIflt(struct tokenize *T, struct vm *out) { err_t err; xRaise("Not implemented"); cleanup: return err; }
static err_t compileIfgt(struct tokenize *T, struct vm *out) { err_t err; xRaise("Not implemented"); cleanup: return err; }
static err_t compileIfle(struct tokenize *T, struct vm *out) { err_t err; xRaise("Not implemented"); cleanup: return err; }
static err_t compileIfge(struct tokenize *T, struct vm *out) { err_t err; xRaise("Not implemented"); cleanup: return err; }

static err_t compileLoop(struct tokenize *T, struct vm *out)
{
        err_t err = OK;

        int oldJumpsLen = out->jumps.len;

        skip(T, tokenOpcode); // "loop"
        skipSpaces(T);

        int startLoop = out->code->len;

        // Loop body
        int n = 0;
        do {
                err = compileExpression(T, out);
                check(err);
                n++;
        } while (T->tokenId != tokenClose);

        // Jump back
        emitDrop(out, n);
        emitJump(out, startLoop);

        out->sp -= n;

        // Fill in the operands of vmJump instructions that break the loop
        int endLoop = out->code->len;
        while (oldJumpsLen < out->jumps.len) {
                int pc = listPop(out->jumps);
                xAssert(out->code->v[pc+0] == vmJump);
                xAssert(out->code->v[pc+1] == 0);
                out->code->v[pc+1] = (endLoop - pc) * sizeof(int);
        }

cleanup:
        out->jumps.len = oldJumpsLen;

        return err;
}

static err_t compileBrk(struct tokenize *T, struct vm *out)
{
        err_t err = OK;

        skip(T, tokenOpcode); // "brk"
        skipSpaces(T);

        // TODO: drop?!

        // Remember this location so we can fill in the operand later when we know it
        listPush(out->jumps, out->code->len);
        emitJump(out, out->code->len); // Operand is just a dummy for now
cleanup:
        return err;
}

static err_t compileCont(struct tokenize *T, struct vm *out) { err_t err; xRaise("Not implemented"); cleanup: return err; }

static err_t compileGetl(struct tokenize *T, struct vm *out)
{
        err_t err = OK;

        skip(T, tokenOpcode); // "getl"
        skipSpaces(T);

        skip(T, tokenInt);
        skipSpaces(T);

        err = emitGetLocal(out, T->tokenValue);
        check(err);
cleanup:
        return err;
}

static err_t compileSetl(struct tokenize *T, struct vm *out)
{
        err_t err = OK;

        skip(T, tokenOpcode); // "setl"
        skipSpaces(T);

        skip(T, tokenInt);
        skipSpaces(T);

        int offset = T->tokenValue;

        err = compileExpression(T, out);
        check(err);

        err = emitSetLocal(out, offset);
        check(err);
cleanup:
        return err;
}

static err_t compileLe(struct tokenize *T, struct vm *out)
{
        err_t err = OK;

        skip(T, tokenOpcode);
        skipSpaces(T);
        err = compileExpression(T, out);
        check(err);
        err = compileExpression(T, out);
        check(err);
        err = emitLessEqualInt(out);
        check(err);
cleanup:
        return err;
}

/*----------------------------------------------------------------------+
 |                                                                      |
 +----------------------------------------------------------------------*/

