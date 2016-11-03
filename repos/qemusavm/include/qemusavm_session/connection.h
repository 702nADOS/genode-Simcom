#ifndef _INCLUDE__QEMUSAVM_SESSION__CONNECTION_H_
#define _INCLUDE__QEMUASVM_SESSION__CONNECTION_H_

#include <qemusavm_session/client.h>
#include <base/connection.h>

namespace QemusaVm {

	struct Connection : Genode::Connection<Session>, Session_client
	{
		Connection()
		:
			/* create session */
			Genode::Connection<QemusaVm::Session>(session("foo, ram_quota=4K")),

			/* initialize RPC interface */
			Session_client(cap()) { }
	};
}

#endif /* _INCLUDE__SIMCOM_SESSION__CONNECTION_H_ */
