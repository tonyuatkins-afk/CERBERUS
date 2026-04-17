#include <stdio.h>
#include <string.h>
#include "crumb.h"

void crumb_init(void)         { /* Task 0.5 — path probe with %TEMP% fallback */ }
void crumb_enter(const char *n){ (void)n; }
void crumb_exit(void)         { }
void crumb_check_previous(void){ }
void crumb_skiplist_add(const char *n){ (void)n; }
int  crumb_skiplist_has(const char *n){ (void)n; return 0; }
