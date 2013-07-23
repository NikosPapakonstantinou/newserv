#include "hmac.h"
#include <string.h>
#include <ctype.h>

void hmacsha256_init(hmacsha256 *c, unsigned char *key, int keylen) {
  unsigned char realkey[64], outerkey[64], innerkey[64];
  SHA256_CTX keyc;
  int i;

  memset(realkey, 0, sizeof(realkey));
  if(keylen > 64) {
    SHA256_Init(&keyc);
    SHA256_Update(&keyc, key, keylen);
    SHA256_Final(realkey, &keyc);
    keylen = 32;
  } else {
    memcpy(realkey, key, keylen);
  }

  /* abusing the cache here, if we do sha256 in between that'll erase it */
  for(i=0;i<64;i++) {
    int r = realkey[i];
    innerkey[i] = r ^ 0x36;
    outerkey[i] = r ^ 0x5c;
  }

  SHA256_Init(&c->outer);
  SHA256_Init(&c->inner);
  SHA256_Update(&c->outer, outerkey, 64);
  SHA256_Update(&c->inner, innerkey, 64);
}

void hmacsha256_update(hmacsha256 *c, unsigned char *message, int messagelen) {
  SHA256_Update(&c->inner, message, messagelen);
}

void hmacsha256_final(hmacsha256 *c, unsigned char *digest) {
  SHA256_Final(digest, &c->inner);
  SHA256_Update(&c->outer, digest, 32);
  SHA256_Final(digest, &c->outer);
}

void hmacsha1_init(hmacsha1 *c, unsigned char *key, int keylen) {
  unsigned char realkey[64], outerkey[64], innerkey[64];
  SHA1_CTX keyc;
  int i;

  memset(realkey, 0, sizeof(realkey));
  if(keylen > 64) {
    SHA1Init(&keyc);
    SHA1Update(&keyc, key, keylen);
    SHA1Final(realkey, &keyc);
    keylen = 20;
  } else {
    memcpy(realkey, key, keylen);
  }

  /* abusing the cache here, if we do sha1 in between that'll erase it */
  for(i=0;i<64;i++) {
    int r = realkey[i];
    innerkey[i] = r ^ 0x36;
    outerkey[i] = r ^ 0x5c;
  }

  SHA1Init(&c->outer);
  SHA1Init(&c->inner);
  SHA1Update(&c->outer, outerkey, 64);
  SHA1Update(&c->inner, innerkey, 64);
}

void hmacsha1_update(hmacsha1 *c, unsigned char *message, int messagelen) {
  SHA1Update(&c->inner, message, messagelen);
}

void hmacsha1_final(hmacsha1 *c, unsigned char *digest) {
  SHA1Final(digest, &c->inner);
  SHA1Update(&c->outer, digest, 20);
  SHA1Final(digest, &c->outer);
}

void hmacmd5_init(hmacmd5 *c, unsigned char *key, int keylen) {
  unsigned char realkey[64], outerkey[64], innerkey[64];
  MD5Context keyc;
  int i;

  memset(realkey, 0, sizeof(realkey));
  if(keylen > 64) {
    MD5Init(&keyc);
    MD5Update(&keyc, key, keylen);
    MD5Final(realkey, &keyc);
    keylen = 16;
  } else {
    memcpy(realkey, key, keylen);
  }

  /* abusing the cache here, if we do sha1 in between that'll erase it */
  for(i=0;i<64;i++) {
    int r = realkey[i];
    innerkey[i] = r ^ 0x36;
    outerkey[i] = r ^ 0x5c;
  }

  MD5Init(&c->outer);
  MD5Init(&c->inner);
  MD5Update(&c->outer, outerkey, 64);
  MD5Update(&c->inner, innerkey, 64);
}

void hmacmd5_update(hmacmd5 *c, unsigned char *message, int messagelen) {
  MD5Update(&c->inner, message, messagelen);
}

void hmacmd5_final(hmacmd5 *c, unsigned char *digest) {
  MD5Final(digest, &c->inner);
  MD5Update(&c->outer, digest, 16);
  MD5Final(digest, &c->outer);
}

static const char *hexdigits = "0123456789abcdef";

char *hmac_printhex(unsigned char *data, char *out, size_t len) {
  size_t i;
  unsigned char *o = (unsigned char *)out;

  for(i=0;i<len;i++) {
    *o++ = hexdigits[(*data & 0xf0) >> 4];
    *o++ = hexdigits[*data & 0x0f];
    data++;
  }

  *o = '\0';
  return out;
}

int hmac_strcmp(const char *a, const char *b) {
  int result = 1;

  if(!a || !b)
    return 1;

  if(strlen(a) != strlen(b))
    return 1;

  while(*a)
    result&=(tolower(*a++) == tolower(*b++));

  return !result;
}

