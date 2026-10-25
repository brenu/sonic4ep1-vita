#ifndef SONIC_DIALOG_POLICY_H
#define SONIC_DIALOG_POLICY_H
static inline int sonic_dialog_button_count(int second, int third) {
    return second ? (third ? 3 : 2) : 1;
}
static inline int sonic_dialog_button_index(int count, int vita_id) {
    return count >= 1 && count <= 3 && vita_id >= 1 && vita_id <= count ? vita_id - 1 : -1;
}
#endif
