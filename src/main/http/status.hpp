// 这个文件是 Poseidon 服务器应用程序框架的一部分。
// Copyleft 2014 - 2015, LH_Mouse. All wrongs reserved.

#ifndef POSEIDON_HTTP_STATUS_HPP_
#define POSEIDON_HTTP_STATUS_HPP_

namespace Poseidon {

namespace HttpStatusCodes {
	typedef unsigned HttpStatus;

	enum {
		// http://www.w3.org/Protocols/rfc2616/rfc2616-sec10.html
		HTTP_CONTINUE					= 100,
		HTTP_SWITCHING_PROTOCOLS		= 101,
		HTTP_OK 						= 200,
		HTTP_CREATED 					= 201,
		HTTP_ACCEPTED 					= 202,
		HTTP_NON_AUTHORITATIVE			= 203,
		HTTP_NO_CONTENT 				= 204,
		HTTP_RESET_CONTENT 				= 205,
		HTTP_PARTIAL_CONTENT 			= 206,
		HTTP_MULTIPLE_CHOICES			= 300,
		HTTP_MOVED_PERMANENTLY			= 301,
		HTTP_FOUND						= 302,
		HTTP_SEE_OTHER		 			= 303,
		HTTP_NOT_MODIFIED 				= 304,
		HTTP_USE_PROXY 					= 305,
		HTTP_TEMPORARY_REDIRECT 		= 307,
		HTTP_BAD_REQUEST 				= 400,
		HTTP_UNAUTHORIZED 				= 401,
		HTTP_FORBIDDEN 					= 403,
		HTTP_NOT_FOUND 					= 404,
		HTTP_METHOD_NOT_ALLOWED			= 405,
		HTTP_NOT_ACCEPTABLE 			= 406,
		HTTP_PROXY_AUTH_REQUIRED 		= 407,
		HTTP_REQUEST_TIMEOUT 			= 408,
		HTTP_CONFLICT 					= 409,
		HTTP_GONE 						= 410,
		HTTP_LENGTH_REQUIRED 			= 411,
		HTTP_PRECONDITION_FAILED 		= 412,
		HTTP_REQUEST_ENTITY_TOO_LARGE 	= 413,
		HTTP_REQUEST_URI_TOO_LONG 		= 414,
		HTTP_UNSUPPORTED_MEDIA_TYPE		= 415,
		HTTP_RANGE_NOT_SATISFIABLE		= 416,
		HTTP_EXPECTATION_FAILED			= 417,
		HTTP_INTERNAL_SERVER_ERROR 		= 500,
		HTTP_NOT_IMPLEMENTED			= 501,
		HTTP_BAD_GATEWAY 				= 502,
		HTTP_SERVICE_UNAVAILABLE		= 503,
		HTTP_GATEWAY_TIMEOUT 			= 504,
		HTTP_VERSION_NOT_SUPPORTED		= 505,
	};
}

using namespace HttpStatusCodes;

struct HttpStatusDesc {
	const char *descShort;
	const char *descLong;
};

HttpStatusDesc getHttpStatusDesc(HttpStatus status);

}

#endif
