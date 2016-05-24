#ifndef SMS_H
#define SMS_H

#include "application.h"

typedef struct { char* buf; char* num; } CMGRparam;
typedef struct { int* ix; int num; } CMGLparam;

int _cbCMGL(int type, const char* buf, int len, CMGLparam* param);
int _cbCMGR(int type, const char* buf, int len, CMGRparam* param);
int smsList(const char* stat /*= "ALL"*/, int* ix /*=NULL*/, int num /*= 0*/);
bool smsDelete(int ix);
enum SmsDeleteFlag {DELETE_READ = 1, DELETE_READ_SENT = 2, DELETE_READ_SENT_UNSENT = 3, DELETE_ALL = 4};
bool smsDeleteAll(SmsDeleteFlag flag);
bool smsRead(int ix, char* num, char* buf, int len);
String checkUnreadSMS();
void checkReadSMS();

#endif