/**
 * \file
 */

#ifndef __SEAWOLF_VAR_INCLUDE_H
#define __SEAWOLF_VAR_INCLUDE_H

void Var_init(void);
float Var_get(char* name);
void Var_setAutoNotify(bool autonotify);
void Var_set(char* name, float value);
void Var_close(void);

int Var_subscribe(char* name);
int Var_bind(char* name, float* store_to);
void Var_unsubscribe(char* name);
void Var_unbind(char* name);
bool Var_stale(char* name);
bool Var_poked(char* name);
void Var_touch(char* name);
void Var_sync(void);
void Var_inputMessage(Comm_Message* message);

#endif // #ifndef __SEAWOLF_VAR_INCLUDE_H
