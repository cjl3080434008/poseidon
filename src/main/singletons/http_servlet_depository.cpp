// 这个文件是 Poseidon 服务器应用程序框架的一部分。
// Copyleft 2014 - 2015, LH_Mouse. All wrongs reserved.

#include "../precompiled.hpp"
#include "http_servlet_depository.hpp"
#include <string>
#include <map>
#include <boost/ref.hpp>
#include <boost/thread/shared_mutex.hpp>
#include "main_config.hpp"
#include "../log.hpp"
#include "../exception.hpp"

namespace Poseidon {

struct HttpServlet : NONCOPYABLE {
	const SharedNts uri;
	const boost::shared_ptr<const HttpServletCallback> callback;

	HttpServlet(SharedNts uri_, boost::shared_ptr<const HttpServletCallback> callback_)
		: uri(STD_MOVE(uri_)), callback(STD_MOVE(callback_))
	{
		LOG_POSEIDON_INFO("Created HTTP servlet for URI ", uri);
	}
	~HttpServlet(){
		LOG_POSEIDON_INFO("Destroyed HTTP servlet for URI ", uri);
	}
};

namespace {
	std::size_t g_maxRequestLength = 16 * 0x400;
	unsigned long long g_keepAliveTimeout = 5000;

	typedef std::map<std::size_t,
		std::map<SharedNts, boost::weak_ptr<HttpServlet> >
		> ServletMap;

	boost::shared_mutex g_mutex;
	ServletMap g_servlets;

	bool getExactServlet(boost::shared_ptr<const HttpServletCallback> &ret,
		std::size_t category, const char *uri, std::size_t uriLen)
	{
		const boost::shared_lock<boost::shared_mutex> slock(g_mutex);

		const AUTO(it, g_servlets.find(category));
		if(it == g_servlets.end()){
			LOG_POSEIDON_DEBUG("No servlet in category ", category);
			return false;
		}

		AUTO_REF(servletsInCategory, it->second);
		const AUTO(sit, servletsInCategory.lower_bound(SharedNts::observe(uri)));
		if(sit == servletsInCategory.end()){
			LOG_POSEIDON_DEBUG("No more handlers: ", uri);
			return false;
		}
		const std::size_t len = std::strlen(sit->first.get());
		if((len < uriLen) || (std::memcmp(sit->first.get(), uri, uriLen) != 0)){
			LOG_POSEIDON_DEBUG("No more handlers: ", uri);
			return false;
		}
		if(len == uriLen){
			const AUTO(servlet, sit->second.lock());
			if(servlet){
				LOG_POSEIDON_DEBUG("Found handler: ", sit->first);
				ret = servlet->callback;
			}
		}
		return true;
	}
}

void HttpServletDepository::start(){
	LOG_POSEIDON_INFO("Starting HTTP servlet depository...");

	AUTO_REF(conf, MainConfig::getConfigFile());

	conf.get(g_maxRequestLength, "http_max_request_length");
	LOG_POSEIDON_DEBUG("Max request length = ", g_maxRequestLength);

	conf.get(g_keepAliveTimeout, "http_keep_alive_timeout");
	LOG_POSEIDON_DEBUG("Keep-Alive timeout = ", g_keepAliveTimeout);
}
void HttpServletDepository::stop(){
	LOG_POSEIDON_INFO("Unloading all HTTP servlets...");

	ServletMap servlets;
	{
		const boost::unique_lock<boost::shared_mutex> ulock(g_mutex);
		servlets.swap(g_servlets);
	}
}

std::size_t HttpServletDepository::getMaxRequestLength(){
	return g_maxRequestLength;
}
unsigned long long HttpServletDepository::getKeepAliveTimeout(){
	return g_keepAliveTimeout;
}

boost::shared_ptr<HttpServlet> HttpServletDepository::registerServlet(
	std::size_t category, SharedNts uri, HttpServletCallback callback)
{
	AUTO(sharedCallback, boost::make_shared<HttpServletCallback>());
	sharedCallback->swap(callback);
	AUTO(servlet, boost::make_shared<HttpServlet>(uri, sharedCallback));
	{
		const boost::unique_lock<boost::shared_mutex> ulock(g_mutex);
		AUTO_REF(old, g_servlets[category][uri]);
		if(!old.expired()){
			LOG_POSEIDON_ERROR("Duplicate servlet for URI ", uri, " in category ", category);
			DEBUG_THROW(Exception, SharedNts::observe("Duplicate HTTP servlet"));
		}
		old = servlet;
	}
	LOG_POSEIDON_DEBUG("Created servlet for for URI ", uri, " in category ", category);
	return servlet;
}

boost::shared_ptr<const HttpServletCallback> HttpServletDepository::getServlet(
	std::size_t category, const char *uri)
{
	if(!uri){
		LOG_POSEIDON_ERROR("uri is null.");
		DEBUG_THROW(Exception, SharedNts::observe("uri is null"));
	}
	if(uri[0] != '/'){
		LOG_POSEIDON_ERROR("URI must begin with a slash: ", uri);
		DEBUG_THROW(Exception, SharedNts::observe("Bad URI format"));
	}

	boost::shared_ptr<const HttpServletCallback> ret;
	const std::size_t uriLen = std::strlen(uri);
	getExactServlet(ret, category, uri, uriLen);
	if(!ret && (uriLen > 1)){
		LOG_POSEIDON_DEBUG("Searching for fallback handlers for URI ", uri);

		std::string fallback;
		fallback.reserve(uriLen + 1);
		fallback.push_back('/');
		std::size_t slash = 0;
		for(;;){
			LOG_POSEIDON_DEBUG("Trying fallback URI handler ", fallback);

			boost::shared_ptr<const HttpServletCallback> test;
			if(!getExactServlet(test, category, fallback.c_str(), fallback.size())){
				break;
			}
			if(test){
				LOG_POSEIDON_DEBUG("Fallback handler matches: ", fallback);
				ret.swap(test);
			}

			if(slash >= uriLen - 1){
				break;
			}
			const char *const nextSlash = std::strchr(uri + slash + 1, '/');
			if(nextSlash){
				fallback.append(uri + slash + 1, nextSlash);
			} else {
				fallback.append(uri + slash + 1);
			}
			fallback.push_back('/');
			slash = static_cast<std::size_t>(nextSlash - uri);
		}
	}
	return ret;
}

}
