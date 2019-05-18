#include<string.h>
#include<ctype.h>

int string_starts_with(char *str, char *start) {
  size_t len_str   = strlen(str);
  size_t len_start = strlen(start);
  if ( len_str >= len_start) {
    if (strncmp(str,start,len_start)==0) {
      return 1;
    }
  } 
  return 0;
}

int string_ends_with(char *str, char *end) {
  int idx;
  char lower_str[1000];
  
  for (idx=0;str[idx]!=0;idx++) {
    lower_str[idx]=tolower(str[idx]);
  } 
  lower_str[idx]=0;

  int len_str=strlen(lower_str);
  int len_end=strlen(end);

  if (len_str < len_end) return 0;
  if (!strcmp(&lower_str[len_str-len_end],end)) return 1;
  return 0;
}

int string_contains(char *str, char *contained) {
  if (strcasestr(str,contained)==NULL) return 0;
  return 1;
}

// count the number of times 'delimiter' appears 
int string_count_char_instances(char *str, char delimiter) {
  char *ptr = str;
  int count=0;
  while(*ptr) {
    if (*ptr == delimiter) {
      count++;
    }
    ptr++;
  }
  return count;
} 

int string_is_a_number(char *str) {
  char *ptr = str;
  if (*ptr == 0) {
    return 0;
  }
  while (*ptr) {
    if (!isdigit(*ptr)) {
      return 0;
    }
    ptr++;
  } 
  return 1;
}

