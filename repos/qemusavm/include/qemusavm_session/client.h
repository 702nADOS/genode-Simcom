#ifndef _INCLUDE__QEMUSAVM_SESSION_H__CLIENT_H_
#define _INCLUDE__QEMUASAVM_SESSION_H__CLIENT_H_

#include <qemusavm_session/qemusavm_session.h>
#include <base/rpc_client.h>
#include <base/printf.h>

namespace QemusaVm {

	struct Session_client : Genode::Rpc_client<Session>
	{
		Session_client(Genode::Capability<Session> cap)
		: Genode::Rpc_client<Session>(cap) { }

		SensorDataOut get_sensor_data()
		{
			return call<Rpc_get_sensor_data>();
		}

		void set_command_data(SensorDataIn commandData)
		{
			call<Rpc_set_command_data>(commandData);
		}
	};
}

#endif
