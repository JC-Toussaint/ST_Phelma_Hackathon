/**
 * app.h - La table qui écoute : point d'entrée de l'application
 *
 * Appelées depuis MEMS/App/app_mems.c (zones USER CODE, conservées par CubeMX) à la place de
 * l'exemple DataLogTerminal de ST : app_init() une fois, app_loop() en boucle.
 */
#ifndef APP_H
#define APP_H

void app_init(void);
void app_loop(void);

#endif /* APP_H */
