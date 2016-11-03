#ifndef _INCLUDE__SIMCOM_SESSION_H__CLIENT_H_
#define _INCLUDE__SIMCOM_SESSION_H__CLIENT_H_

#include <simcom_session/simcom_session.h>
#include <base/rpc_client.h>
#include <base/printf.h>

namespace SimCom {

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
