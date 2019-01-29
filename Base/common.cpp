#include "common.h"
#include <string>

using namespace std;

const char *s_http_port="8081";

string FILEFOLDER="./recordFile/";
string IpPort = "http://";

string record_serverId; 

//Http API方法名
string ServerCreate;
string ServerDelete;
string ServerSelect;
string ServerUpdate;

string liveUpdate;
string liveSelect;
string liveUpload;

