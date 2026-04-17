#ifndef CERBERUS_CRUMB_H
#define CERBERUS_CRUMB_H

void crumb_init(void);
void crumb_enter(const char *test_name);
void crumb_exit(void);
void crumb_check_previous(void);
void crumb_skiplist_add(const char *name);
int  crumb_skiplist_has(const char *name);

#endif
