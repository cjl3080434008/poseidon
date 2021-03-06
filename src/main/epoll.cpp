// 这个文件是 Poseidon 服务器应用程序框架的一部分。
// Copyleft 2014 - 2015, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "epoll.hpp"
#include "tcp_session_base.hpp"
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include "multi_index_map.hpp"
#include "exception.hpp"
#include "log.hpp"
#include "utilities.hpp"

namespace Poseidon {

namespace {
	enum {
		MAX_PUMP_COUNT = 256
	};

	struct SessionMapElement {
		int fd;
		// 时间戳，零表示无数据可读/写。
		unsigned long long lastRead;
		unsigned long long lastWritten;

		boost::shared_ptr<TcpSessionBase> session;

		SessionMapElement(boost::shared_ptr<TcpSessionBase> session_,
			unsigned long long lastRead_, unsigned long long lastWritten_)
			: fd(session_->getFd()), lastRead(lastRead_), lastWritten(lastWritten_)
			, session(STD_MOVE(session_))
		{
		}
	};

	MULTI_INDEX_MAP(SessionMap, SessionMapElement,
		UNIQUE_MEMBER_INDEX(fd)
		MULTI_MEMBER_INDEX(lastRead)
		MULTI_MEMBER_INDEX(lastWritten)
	)

	enum {
		IDX_FD,
		IDX_READ,
		IDX_WRITE,
	};
}

struct Epoll::SessionMapImpl
	: public SessionMap
{
};

Epoll::Epoll(){
	if(!m_epoll.reset(::epoll_create(4096))){
		DEBUG_THROW(SystemError);
	}
	m_sessions.reset(new SessionMapImpl);
}
Epoll::~Epoll(){
	for(AUTO(it, m_sessions->begin()); it != m_sessions->end(); ++it){
		it->session->setEpoll(NULLPTR);
	}
}

void Epoll::notifyWriteable(TcpSessionBase *session){
	const AUTO(now, getFastMonoClock());
	const boost::mutex::scoped_lock lock(m_mutex);

	const AUTO(it, m_sessions->find<IDX_FD>(session->getFd()));
	if(it == m_sessions->end<IDX_FD>()){
		LOG_POSEIDON_WARNING("Session is not in epoll?");
		return;
	}
	m_sessions->setKey<IDX_FD, IDX_WRITE>(it, now);
}

void Epoll::addSession(const boost::shared_ptr<TcpSessionBase> &session){
	const boost::mutex::scoped_lock lock(m_mutex);
	const AUTO(result, m_sessions->insert(SessionMapElement(session, 0, 0)));
	if(!result.second){
		LOG_POSEIDON_WARNING("Session is already in epoll.");
		return;
	}
	::epoll_event event;
	event.events = static_cast< ::uint32_t>(EPOLLIN | EPOLLOUT | EPOLLET);
#ifndef NDEBUG
	std::memset(&event.data, 0xCC, sizeof(event.data)); // valgrind 误报。
#endif
	event.data.fd = session->getFd();
	if(::epoll_ctl(m_epoll.get(), EPOLL_CTL_ADD, session->getFd(), &event) != 0){
		const int errCode = errno;
		m_sessions->erase(result.first);
		DEBUG_THROW(SystemError, errCode);
	}
	session->setEpoll(this);
}
void Epoll::removeSession(const boost::shared_ptr<TcpSessionBase> &session){
	const boost::mutex::scoped_lock lock(m_mutex);
	const AUTO(it, m_sessions->find<IDX_FD>(session->getFd()));
	if(it == m_sessions->end<IDX_FD>()){
		LOG_POSEIDON_WARNING("Session is not in epoll.");
		return;
	}
	session->setEpoll(NULLPTR);
	if(::epoll_ctl(m_epoll.get(), EPOLL_CTL_DEL, session->getFd(), NULLPTR) != 0){
		const int errCode = errno;
		LOG_POSEIDON_WARNING("Error deleting from epoll: errno = ", errCode);
	}
	m_sessions->erase<IDX_FD>(it);
}
void Epoll::snapshot(std::vector<boost::shared_ptr<TcpSessionBase> > &sessions) const {
	const boost::mutex::scoped_lock lock(m_mutex);
	sessions.reserve(m_sessions->size());
	for(AUTO(it, m_sessions->begin()); it != m_sessions->end(); ++it){
		sessions.push_back(it->session);
	}
}
void Epoll::clear(){
	const boost::mutex::scoped_lock lock(m_mutex);
	AUTO(it, m_sessions->begin());
	while(it != m_sessions->end()){
		::epoll_ctl(m_epoll.get(), EPOLL_CTL_DEL, it->session->getFd(), NULLPTR);
		it->session->setEpoll(NULLPTR);
		it = m_sessions->erase(it);
	}
}

std::size_t Epoll::wait(unsigned timeout){
	::epoll_event events[MAX_PUMP_COUNT];
	const int count = ::epoll_wait(m_epoll.get(), events, (int)COUNT_OF(events), (int)timeout);
	if(count < 0){
		const int errCode = errno;
		LOG_POSEIDON_ERROR("::epoll_wait() failed: errno = ", errCode);
		return 0;
	}

	const AUTO(now, getFastMonoClock());
	for(unsigned i = 0; i < (unsigned)count; ++i){
		const ::epoll_event &event = events[i];

		boost::shared_ptr<TcpSessionBase> session;
		SessionMap::delegated_container::nth_index<IDX_FD>::type::iterator it;
		{
			const boost::mutex::scoped_lock lock(m_mutex);
			it = m_sessions->find<IDX_FD>(event.data.fd);
			if(it == m_sessions->end<IDX_FD>()){
				LOG_POSEIDON_WARNING("Session is not in epoll?");
				continue;
			}
			session = it->session;
		}

		if(event.events & EPOLLHUP){
			LOG_POSEIDON_INFO("Socket hung up, remote is ", session->getRemoteInfo());
			session->onClose();
			removeSession(session);
			continue;
		}
		if(event.events & EPOLLERR){
			int errCode;
			::socklen_t errLen = sizeof(errCode);
			if(::getsockopt(session->getFd(), SOL_SOCKET, SO_ERROR, &errCode, &errLen) != 0){
				errCode = errno;
			}
			const AUTO(desc, getErrorDesc(errCode));
			LOG_POSEIDON_WARNING("Socket error: ", desc);
			session->onClose();
			removeSession(session);
			continue;
		}

		{
			boost::mutex::scoped_lock lock(m_mutex, boost::defer_lock);
			if(event.events & EPOLLIN){
//				if(!lock.owns_lock()){
					lock.lock();
//				}
				m_sessions->setKey<IDX_FD, IDX_READ>(it, now);
			}
			if(event.events & EPOLLOUT){
				if(!lock.owns_lock()){
					lock.lock();
				}
				m_sessions->setKey<IDX_FD, IDX_WRITE>(it, now);
			}
		}
	}
	return (unsigned)count;
}
std::size_t Epoll::pumpReadable(){
	// 有序的关系型容器在插入元素时迭代器不失效。这一点非常重要。
	SessionMap::delegated_container::nth_index<IDX_READ>::type::iterator iterators[MAX_PUMP_COUNT];
	std::size_t count = 0;
	{
		const boost::mutex::scoped_lock lock(m_mutex);
		for(AUTO(it, m_sessions->upperBound<IDX_READ>(0)); it != m_sessions->end<IDX_READ>(); ++it){
			iterators[count] = it;
			if(++count >= MAX_PUMP_COUNT){
				break;
			}
		}
	}
	for(std::size_t i = 0; i < count; ++i){
		const AUTO_REF(it, iterators[i]);
		const AUTO_REF(session, it->session);

		try {
			unsigned char temp[1024];
			long bytesRead = session->syncReadAndProcess(temp, sizeof(temp));
			if(bytesRead < 0){
				if(errno == EINTR){
					continue;
				}
				if(errno == EAGAIN){
					const boost::mutex::scoped_lock lock(m_mutex);
					m_sessions->setKey<IDX_READ, IDX_READ>(it, 0);
					continue;
				}
				DEBUG_THROW(SystemError);
			} else if(bytesRead == 0){
				LOG_POSEIDON_INFO("Connection closed: remote = ", session->getRemoteInfo());
				session->send(StreamBuffer(), true);
				continue;
			}
		} catch(std::exception &e){
			LOG_POSEIDON_ERROR("std::exception thrown while dispatching data: what = ", e.what());
			session->send(StreamBuffer(), true);
		} catch(...){
			LOG_POSEIDON_ERROR("Unknown exception thrown while dispatching data.");
			session->send(StreamBuffer(), true);
		}
	}
	return count;
}
std::size_t Epoll::pumpWriteable(){
	// 有序的关系型容器在插入元素时迭代器不失效。这一点非常重要。
	SessionMap::delegated_container::nth_index<IDX_WRITE>::type::iterator iterators[MAX_PUMP_COUNT];
	std::size_t count = 0;
	{
		const boost::mutex::scoped_lock lock(m_mutex);
		for(AUTO(it, m_sessions->upperBound<IDX_WRITE>(0)); it != m_sessions->end<IDX_WRITE>(); ++it){
			iterators[count] = it;
			if(++count >= MAX_PUMP_COUNT){
				break;
			}
		}
	}
	for(std::size_t i = 0; i < count; ++i){
		const AUTO_REF(it, iterators[i]);
		const AUTO_REF(session, it->session);

		try {
			unsigned char temp[1024];
			long bytesWritten;
			bool shutdown;
			{
				boost::mutex::scoped_lock sessionLock;
				bytesWritten = session->syncWrite(sessionLock, temp, sizeof(temp));
				shutdown = session->hasBeenShutdown();
				if(bytesWritten == 0){
					if(!shutdown){
						const boost::mutex::scoped_lock lock(m_mutex);
						m_sessions->setKey<IDX_WRITE, IDX_WRITE>(it, 0);
					}
				}
			}
			if(bytesWritten < 0){
				if(errno == EINTR){
					continue;
				}
				if(errno == EAGAIN){
					const boost::mutex::scoped_lock lock(m_mutex);
					m_sessions->setKey<IDX_WRITE, IDX_WRITE>(it, 0);
					continue;
				}
				DEBUG_THROW(SystemError);
			} else if(bytesWritten == 0){
				if(shutdown){
					session->forceShutdown();
				}
				continue;
			}
		} catch(std::exception &e){
			LOG_POSEIDON_ERROR("std::exception thrown while writing socket: what = ", e.what());
			session->forceShutdown();
		} catch(...){
			LOG_POSEIDON_ERROR("Unknown exception thrown while writing socket.");
			session->forceShutdown();
		}
	}
	return count;
}

}
