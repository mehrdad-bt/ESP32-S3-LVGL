#pragma once
#ifdef __cplusplus
extern "C" {
#endif

void update_page_enter(void);    // request a rescan (called from action)
void update_page_up(void);
void update_page_down(void);
void update_page_select(void);
void update_page_process(void);  // call from tasks_run()

#ifdef __cplusplus
}
#endif