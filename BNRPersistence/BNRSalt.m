#import <CoreFoundation/CFByteOrder.h>
#import "BNRSalt.h"


typedef UInt32 (*BNRSaltApplier)(UInt32 word);
static void BNRSaltApply(BNRSalt *salt, BNRSaltApplier fun);

void
GetBNRSaltWithWords(UInt32 word0, UInt32 word1, BNRSalt *outSalt)
{
    if (!outSalt) return;

    outSalt->word[0] = word0;
    outSalt->word[0] = word1;
}

void
StirBNRSaltWithWords(BNRSalt *salt, UInt32 word0, UInt32 word1)
{
    if (!salt) return;

    salt->word[0] ^= word0;
    salt->word[1] ^= word1;
}

void
BNRSaltToLittle(BNRSalt *salt)
{
    BNRSaltApply(salt, CFSwapInt32HostToLittle);
}

/*! Swaps each word from little endian to host endian. */
void
BNRSaltToHost(BNRSalt *salt)
{
    BNRSaltApply(salt, CFSwapInt32LittleToHost);
}

static void
BNRSaltApply(BNRSalt *salt, BNRSaltApplier fun)
{
    if (!salt) return;

    for (size_t i = 0; i < BNR_SALT_WORD_COUNT; ++i) {
        salt->word[i] = fun(salt->word[i]);
    }
}
