#ifndef BNRPersistence_BNRSalt_h
#define BNRPersistence_BNRSalt_h

#define BNR_SALT_WORD_COUNT (2)

typedef struct BNRSalt {
    UInt32 word[BNR_SALT_WORD_COUNT];
} BNRSalt;

/*! Copies words into |outSalt|.
 *  Returns |outSalt| by reference. */
void GetBNRSaltWithWords(UInt32 word0, UInt32 word1, BNRSalt *outSalt);

/*! XORs word0 and word1 with |salt|.
 *  This changes the values of |salt|. */
void StirBNRSaltWithWords(BNRSalt *salt, UInt32 word0, UInt32 word1);

/*! Swaps each word to little endian. */
void BNRSaltToLittle(BNRSalt *salt);

/*! Swaps each word from little endian to host endian. */
void BNRSaltToHost(BNRSalt *salt);
#endif
