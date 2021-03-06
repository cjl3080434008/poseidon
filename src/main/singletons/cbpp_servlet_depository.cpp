// 这个文件是 Poseidon 服务器应用程序框架的一部分。
// Copyleft 2014 - 2015, LH_Mouse. All wrongs reserved.

#include "../precompiled.hpp"
#include "cbpp_servlet_depository.hpp"
#include <map>
#include <boost/ref.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <boost/thread/locks.hpp>
#include "main_config.hpp"
#include "../log.hpp"
#include "../exception.hpp"

namespace Poseidon {

struct CbppServlet : NONCOPYABLE {
	const boost::uint16_t protocolId;
	const boost::shared_ptr<const CbppServletCallback> callback;

	CbppServlet(boost::uint16_t protocolId_, boost::shared_ptr<const CbppServletCallback> callback_)
		: protocolId(protocolId_), callback(STD_MOVE(callback_))
	{
	}
};

namespace {
	std::size_t g_maxRequestLength			= 16 * 0x400;
	unsigned long long g_keepAliveTimeout	= 30000;

	typedef std::map<std::size_t,
		std::map<boost::uint16_t, boost::weak_ptr<const CbppServlet> >
		> ServletMap;

	boost::shared_mutex g_mutex;
	ServletMap g_servlets;
}

void CbppServletDepository::start(){
	LOG_POSEIDON_INFO("Starting CBPP servlet depository...");

	AUTO_REF(conf, MainConfig::getConfigFile());

	conf.get(g_maxRequestLength, "cbpp_max_request_length");
	LOG_POSEIDON_DEBUG("Max request length = ", g_maxRequestLength);

	conf.get(g_keepAliveTimeout, "cbpp_keep_alive_timeout");
	LOG_POSEIDON_DEBUG("Keep alive timeout = ", g_keepAliveTimeout);
}
void CbppServletDepository::stop(){
	LOG_POSEIDON_INFO("Unloading all CBPP servlets...");

	ServletMap servlets;
	{
		const boost::unique_lock<boost::shared_mutex> ulock(g_mutex);
		servlets.swap(servlets);
	}
}

std::size_t CbppServletDepository::getMaxRequestLength(){
	return g_maxRequestLength;
}
unsigned long long CbppServletDepository::getKeepAliveTimeout(){
	return g_keepAliveTimeout;
}

boost::shared_ptr<CbppServlet> CbppServletDepository::registerServlet(
	std::size_t category, boost::uint16_t protocolId, CbppServletCallback callback)
{
	AUTO(sharedCallback, boost::make_shared<CbppServletCallback>());
	sharedCallback->swap(callback);
	AUTO(servlet, boost::make_shared<CbppServlet>(protocolId, sharedCallback));
	{
		const boost::unique_lock<boost::shared_mutex> ulock(g_mutex);
		AUTO_REF(old, g_servlets[category][protocolId]);
		if(!old.expired()){
			LOG_POSEIDON_ERROR("Duplicate servlet for id ", protocolId, " in category ", category);
			DEBUG_THROW(Exception, SharedNts::observe("Duplicate CBPP protocol servlet"));
		}
		old = servlet;
	}
	LOG_POSEIDON_DEBUG("Created servlet for protocol ", protocolId, " in category ", category);
	return servlet;
}

boost::shared_ptr<const CbppServletCallback> CbppServletDepository::getServlet(
	std::size_t category, boost::uint16_t protocolId)
{
    const boost::shared_lock<boost::shared_mutex> slock(g_mutex);
    const AUTO(it, g_servlets.find(category));
    if(it == g_servlets.end()){
        LOG_POSEIDON_DEBUG("No servlet in category ", category);
        return VAL_INIT;
    }
    const AUTO(it2, it->second.find(protocolId));
    if(it2 == it->second.end()){
    	LOG_POSEIDON_DEBUG("No servlet for protocol ", protocolId, " in category ", category);
    	return VAL_INIT;
    }
    const AUTO(servlet, it2->second.lock());
    if(!servlet){
    	LOG_POSEIDON_DEBUG("Expired servlet for protocol ", protocolId, " in category ", category);
    	return VAL_INIT;
    }
    return servlet->callback;
}

}
