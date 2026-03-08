#ifndef __UNICODE_COLLATION_PROTOCOL_H__
#define __UNICODE_COLLATION_PROTOCOL_H__

#include <uefi/types.h>

struct EfiUnicodeCollationProtocol {
  int64_t (*stri_coll)(EfiUnicodeCollationProtocol *self,
                       char16_t *s1, char16_t *s2);

  bool (*metai_match)(EfiUnicodeCollationProtocol *self,
                         char16_t *string, char16_t *pattern);

  void (*str_lwr)(EfiUnicodeCollationProtocol *self,
                  char16_t *string);

  void (*str_upr)(EfiUnicodeCollationProtocol *self,
                  char16_t *string);

  void (*fat_to_str)(EfiUnicodeCollationProtocol *self,
                     uint64_t fat_size, char *fat, char16_t *string);

  void (*str_to_fat)(EfiUnicodeCollationProtocol *self,
                     char16_t *string, uint64_t fat_size, char *fat);
  
  char *supported_languages;
};

extern const EfiUnicodeCollationProtocol unicodeCollationProtocol;

#endif  //__UNICODE_COLLATION_PROTOCOL_H__