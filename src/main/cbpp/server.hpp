// 这个文件是 Poseidon 服务器应用程序框架的一部分。
// Copyleft 2014 - 2015, LH_Mouse. All wrongs reserved.

#ifndef POSEIDON_CBPP_SERVER_HPP_
#define POSEIDON_CBPP_SERVER_HPP_

#include "../tcp_server_base.hpp"

namespace Poseidon {

class CbppServer : public TcpServerBase {
private:
	const std::size_t m_category;

public:
	CbppServer(std::size_t category, const IpPort &bindAddr,
		const char *cert, const char *privateKey);

protected:
	boost::shared_ptr<class TcpSessionBase> onClientConnect(UniqueFile client) const;
};

}

#endif
