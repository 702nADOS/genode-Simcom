#ifndef _INCLUDE__SIMCOM_SESSION__CONNECTION_H_
#define _INCLUDE__SIMCOM_SESSION__CONNECTION_H_

#include <simcom_session/client.h>
#include <base/connection.h>

namespace SimCom {

	struct Connection : Genode::Connection<Session>, Session_client
	{
		Connection()
		:
			/* create session */
			Genode::Connection<SimCom::Session>(session("foo, ram_quota=4K")),

			/* initialize RPC interface */
			Session_client(cap()) { }
	};
}

#endif /* _INCLUDE__SIMCOM_SESSION__CONNECTION_H_ */
