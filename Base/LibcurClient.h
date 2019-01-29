
#ifndef LIBCURCLIENT_H
#define LIBCURCLIENT_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string>
#include <curl/curl.h> 
#include <curl/easy.h>
#include <iostream>

//请求类型
enum LibcurlFlag
{
	Lf_None = 0,
	Lf_Get,
	Lf_Post
};

using namespace std;

class LibcurClient
{
public:

	LibcurClient();

	~LibcurClient();

	//get请求方法
	int HttpGetData(const char *url, int timeout = 10, int connect_timeout = 10);

	//post请求方法
    int HttpPostData(const char *url, std::string strLiveID , std::string strLiveData , int timeout = 10, int connect_timeout = 10);
	
	//写结果静态方法
	static size_t writeCallbackData(void *buffer, size_t size, size_t nmemb, void *stream);
	
	//写回调函数
	size_t WriteCallback_fun(void *buffer, size_t size, size_t nmemb);

	size_t WriteCallbackData(void *buffer, size_t size, size_t nmemb);

	//获取返回值
	std::string GetResdata();

	char *UrlEncode(std::string &m_str);
	
public:

	LibcurlFlag  m_lfFlag;
	
private:

    CURL* m_curl;
  
	std::string m_getWritedata;
};

#endif
