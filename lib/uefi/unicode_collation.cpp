#include "unicode_collation.h"

#include <stdio.h>
#include <uefi/protocols/unicode_collation_protocol.h>
#include <uefi/types.h>

#include "charset.h"

static int64_t stri_coll(EfiUnicodeCollationProtocol *self,
                  char16_t *s1, char16_t *s2) {
    return utf16_strcmp(s1, s2);
}

static bool metai_match(EfiUnicodeCollationProtocol *self,
                 char16_t *string, char16_t *pattern) {

    printf("%s(EFI_UNICODE_COLLATION_PROTOCOL), Unsupported function", __FUNCTION__);

    return false;
}

static void str_upr(EfiUnicodeCollationProtocol *self,
             char16_t *string) {
    
    for (; *string != '\0'; string++) {
        *string = utf_to_upper(*string);
    }
}

static void str_lwr(EfiUnicodeCollationProtocol *self,
             char16_t *string) {
    
    for (; *string != '\0'; string++) {
        *string = utf_to_lower(*string);
    }
}

static void fat_to_str(EfiUnicodeCollationProtocol *self,
                uint64_t fat_size, char *fat, char16_t *string) {
    printf("%s(EFI_UNICODE_COLLATION_PROTOCOL), Unsupported function", __FUNCTION__);
}

static void str_to_fat(EfiUnicodeCollationProtocol *self,
                char16_t *string, uint64_t fat_size, char *fat) {
    printf("%s(EFI_UNICODE_COLLATION_PROTOCOL), Unsupported function", __FUNCTION__);
}

const EfiUnicodeCollationProtocol unicodeCollationProtocol = {
    .stri_coll = stri_coll,
    .metai_match = metai_match,
    .str_lwr = str_lwr,
    .str_upr = str_upr,
    .fat_to_str = fat_to_str,
    .str_to_fat = str_to_fat,
    .supported_languages = "en",
};
