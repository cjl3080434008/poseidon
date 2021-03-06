// 这个文件是 Poseidon 服务器应用程序框架的一部分。
// Copyleft 2014 - 2015, LH_Mouse. All wrongs reserved.

#ifndef POSEIDON_SINGLETONS_WEBSOCKET_SERVLET_DEPOSITORY_HPP_
#define POSEIDON_SINGLETONS_WEBSOCKET_SERVLET_DEPOSITORY_HPP_

#include "../cxx_ver.hpp"
#include <string>
#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>
#include <boost/function.hpp>
#include "../shared_nts.hpp"
#include "../stream_buffer.hpp"
#include "../http/websocket/callbacks.hpp"

namespace Poseidon {

class WebSocketServlet;
class WebSocketSession;

struct WebSocketServletDepository {
	static void start();
	static void stop();

	static std::size_t getMaxRequestLength();
	static unsigned long long getKeepAliveTimeout();

	// 返回的 shared_ptr 是该响应器的唯一持有者。
	static boost::shared_ptr<WebSocketServlet> registerServlet(
		std::size_t category, SharedNts uri, WebSocketServletCallback callback);

	static boost::shared_ptr<const WebSocketServletCallback> getServlet(
		std::size_t category, const char *uri);

private:
	WebSocketServletDepository();
};

}

#endif
