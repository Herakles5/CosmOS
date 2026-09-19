#ifndef AION_APP_H
#define AION_APP_H

void InitAionApp();
void UpdateAionApp(int cx, int cy, int cw, int ch, bool is_active);
void UpdateAionBackground();
void AionHandleInput(const char* text);
void AionProactiveSpeak(const char* message);

extern char aion_system_context[256];
extern volatile int aion_requested_app;

#ifdef __cplusplus
extern "C" {
#endif
    extern bool aion_wants_foreground;
    extern bool aion_wants_background;
#ifdef __cplusplus
}
#endif

#endif
