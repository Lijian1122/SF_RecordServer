/*****************************************************
版权所有:北京三海教育科技有限公司
作者：lijian
版本：V0.0.1
时间：2019年05月17日
功能：读取相关配置文件

v 0.0.1
******************************************************/

#ifndef CONFIGFILEREADER_H_
#define CONFIGFILEREADER_H_
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>
#include <map>
#include "ostype.h"

using namespace std;

class CConfigFileReader
{
public:
    CConfigFileReader(const char* filename);
    ~CConfigFileReader();

    char* GetConfigName(const char* name);
    int SetConfigValue(const char* name, const char*  value);
private:
    void _LoadFile(const char* filename);
    int _WriteFIle(const char*filename = NULL);
    void _ParseLine(char* line);
    char* _TrimSpace(char* name);

    bool m_load_ok;
    map<string, string> m_config_map;
    string m_config_file;
};



#endif /* CONFIGFILEREADER_H_ */
