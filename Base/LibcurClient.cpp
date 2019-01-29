#include "LibcurClient.h"

LibcurClient::LibcurClient()
{
	
	m_lfFlag = Lf_Get;
	
	//首先全局初始化CURL
	curl_global_init(CURL_GLOBAL_ALL); 
	
	//初始化CURL句柄
	m_curl = curl_easy_init(); 
	
	if(NULL == m_curl)
	{
		printf("(申请内存失败!\n");
		curl_easy_cleanup(m_curl);
	}
	m_getWritedata = "";
}

//get请求方法
int LibcurClient::HttpGetData(const char *url, int timeout , int connect_timeout)
{
	m_getWritedata = "";

	m_lfFlag = Lf_Get;

	// 设置目标URL
	CURLcode res = curl_easy_setopt(m_curl, CURLOPT_URL, url);

	// 设置接收到HTTP服务器的数据时调用的回调函数
	res = curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, writeCallbackData);
	// 设置自定义参数(回调函数的第四个参数)

	res = curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, this); 

	//支持重定向
	res = curl_easy_setopt(m_curl, CURLOPT_FOLLOWLOCATION, 1);

	//设置数据返回超时时间为30s
	res = curl_easy_setopt(m_curl, CURLOPT_TIMEOUT, timeout);

	//设置连接超时时间为10s
	res = curl_easy_setopt(m_curl, CURLOPT_CONNECTTIMEOUT, connect_timeout);

	// 执行一次URL请求
    res = curl_easy_perform(m_curl);

	return res;
}

//post请求方法
int LibcurClient::HttpPostData(const char *url, std::string strLiveID , std::string strLiveData , int timeout, int connect_timeout)
{  

    CURLcode res;

    m_getWritedata = "";
   

    m_lfFlag = Lf_Post;

    struct curl_httppost *formpost = NULL;
    struct curl_httppost *lastptr = NULL;
   
    //curl_formadd(&formpost, &lastptr, CURLFORM_PTRNAME, "reqformat", CURLFORM_PTRCONTENTS, "plain", CURLFORM_END);

    //CURLFORMcode mres = 
    curl_formadd(&formpost, &lastptr, CURLFORM_COPYNAME, "ID", CURLFORM_COPYCONTENTS, strLiveID.c_str(), CURLFORM_END);
    curl_formadd(&formpost, &lastptr, CURLFORM_COPYNAME, "DATA", CURLFORM_COPYCONTENTS, strLiveData.c_str(), CURLFORM_END);

    res = curl_easy_setopt(m_curl, CURLOPT_URL, url);
    res =curl_easy_setopt(m_curl, CURLOPT_HTTPPOST, formpost);

    res =curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, writeCallbackData);
    res = curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, this);
    res = curl_easy_perform(m_curl);

    return res;
}

//结果回调
size_t  LibcurClient::writeCallbackData(void *buffer, size_t size, size_t nmemb, void *stream)
{
	if(stream) 
	{
            LibcurClient* pThis = (LibcurClient*)stream;
	    pThis->WriteCallback_fun(buffer, size, nmemb);
	}
	return size * nmemb;
}

size_t  LibcurClient::WriteCallback_fun(void *buffer, size_t size, size_t nmemb)
{        
    switch (m_lfFlag)
    {
      case Lf_Get:
      {                        
	     WriteCallbackData(buffer, size, nmemb);
		 break;
      }    
      case Lf_Post:
      {      
         WriteCallbackData(buffer, size, nmemb);
		 break;
      } 
      default:
       break;
    }
    return size * nmemb;	
}

size_t LibcurClient::WriteCallbackData(void *buffer, size_t size, size_t nmemb)
{
    char* pData = (char*)buffer;

	m_getWritedata.append(pData, size * nmemb);
	
	return size * nmemb;
}

//返回数据对外接口
std::string LibcurClient::GetResdata()
{
   return m_getWritedata;
}

char *LibcurClient::UrlEncode(std::string &m_str)
{
	return curl_easy_escape(m_curl ,m_str.c_str() ,static_cast<int>(m_str.size()));
}

LibcurClient::~LibcurClient()
{
	if(m_curl)
	{
		curl_easy_cleanup(m_curl);
	}
	curl_global_cleanup();
}
