#include <base/printf.h>
#include <terminal_session/connection.h>
#include <qemusavm/types.h>
#include <ecu_api/CoreAPI.h>
#include <ecu_api/Defines.h>
#include <ecu_api/ErrorCodes.h>
#include <cap_session/connection.h>
#include <root/component.h>
#include <qemusavm_session/qemusavm_session.h>
#include <base/rpc_server.h>
#include <base/sleep.h>

namespace QemusaVm {

	struct Session_component : Genode::Rpc_object<Session>
	{
		protected:
			CoreAPI _core;

		public:
			Session_component() : _core(CONNECTION_COM) { PDBG("Creating ECU Connection.\n"); }

			SensorDataOut get_sensor_data()
			{
				int status = RQ_SUCCESS, var = 0;
				SensorDataOut sensorData = {0};
				if ((status = _core.GetMCUTemperature(var)) != RQ_SUCCESS)
				{
					PDBG("Error getting MCU temperature failed: %d\n", status);
				}
				sensorData.engineTemp = var;
				if ((status = _core.GetMotorRPM(var)) != RQ_SUCCESS)
				{
					PDBG("Error getting engine RPM failed: %d\n", status);
				}
				sensorData.engineRPM = var;
				return sensorData;
			}
			
			void set_command_data(SensorDataIn commandData)
			{
				int status = RQ_SUCCESS;
				int ispeed = std::max(0, 5 * std::min(static_cast<int>(commandData.ownSpeed), 200));
				if ((status = _core.SetMotorSpeedAbs(ispeed)) != RQ_SUCCESS)
				{
					PDBG("Error setting motor speed failed: %d\n", status);
				}
			}
	};

	class Root_component : public Genode::Root_component<Session_component>
	{
		protected:

			QemusaVm::Session_component *_create_session(const char *args)
			{
				PDBG("creating ecu session.");
				return new (md_alloc()) Session_component();
			}

		public:

			Root_component(Genode::Rpc_entrypoint *ep,
			               Genode::Allocator *allocator)
			: Genode::Root_component<Session_component>(ep, allocator)
			{
				PDBG("Creating root component.");
			}
	};
}

using namespace Genode;

int main(int argc, char* argv[])
{
	PDBG("Hello, QemusaVm-ECU!\n");
	/*
	 * Get a session for the parent's capability service, so that we
	 * are able to create capabilities.
	 */
	Cap_connection cap;

	/*
	 * A sliced heap is used for allocating session objects - thereby we
	 * can release objects separately.
	 */
	static Sliced_heap sliced_heap(env()->ram_session(),
	                               env()->rm_session());

	/*
	 * Create objects for use by the framework.
	 *
	 * An 'Rpc_entrypoint' is created to announce our service's root
	 * capability to our parent, manage incoming session creation
	 * requests, and dispatch the session interface. The incoming RPC
	 * requests are dispatched via a dedicated thread. The 'STACK_SIZE'
	 * argument defines the size of the thread's stack. The additional
	 * string argument is the name of the entry point, used for
	 * debugging purposes only.
	 */
	enum { STACK_SIZE = 4096 };
	static Rpc_entrypoint ep(&cap, STACK_SIZE, "ecu_ep");

	static QemusaVm::Root_component ecu_root(&ep, &sliced_heap);
	//Illegal Announcement, just commented out for our purposes
	//	env()->parent()->announce(ep.manage(&ecu_root));

	/* We are done with this and only act upon client requests now. */
	sleep_forever();

	return 0;
}
