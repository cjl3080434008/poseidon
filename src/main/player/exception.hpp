#ifndef POSEIDON_PLAYER_EXCEPTION_HPP_
#define POSEIDON_PLAYER_EXCEPTION_HPP_

#include "../exception.hpp"
#include "status.hpp"

namespace Poseidon {

class PlayerProtocolException : public ProtocolException {
public:
	PlayerProtocolException(const char *file, std::size_t line,
		PlayerStatus status, std::string reason);
	~PlayerProtocolException() NOEXCEPT;

public:
	PlayerStatus status() const NOEXCEPT {
		return static_cast<PlayerStatus>(ProtocolException::code());
	}
};

}

#endif
