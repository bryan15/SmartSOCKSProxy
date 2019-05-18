// Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#ifndef STRING2_H
#define STRING2_H

int string_starts_with(char *str, char *start);
int string_ends_with(char *str, char *end);
int string_contains(char *str, char *contained);
int string_count_char_instances(char *str, char delimiter);
int string_is_a_number(char *str);

#endif // STRING2_H

