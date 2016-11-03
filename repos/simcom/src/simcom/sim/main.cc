#include <algorithm>

#include <base/printf.h>
#include <base/thread.h>
#include <util/string.h>
#include <timer_session/connection.h>
#include <simcom_session/connection.h>
#include <terminal_session/connection.h>

#include <nic/packet_allocator.h>

#include <lwip/lwipopts.h>
#include <lwip/sockets.h>
#include <lwip/api.h>
#include <lwip/genode.h>

#include <simcom/types.h>

float g_distThreshold = 50.0; //50m
float g_followDist = 10.0; // 10m
float g_maxAccel = 1.5;
float g_maxBrake = 1.0;

int getSpeedDepGear(float speed, int currentGear);
CommandDataOut followDriving(SensorDataIn sd);


int getSpeedDepGear(float speed, int currentGear)
{
					 // 0   60  100 150 200 250 km/h
	float gearUP[6] = {-1, 17, 27, 41, 55, 70}; //Game uses values in m/s: xyz m/s = (3.6 * xyz) km/h
	float gearDN[6] = {0,  0,  15, 23, 35, 48};

	int gear = currentGear;

	if (speed > gearUP[gear])
	{
		gear = std::min(5, currentGear + 1);
	}
	if (speed < gearDN[gear])
	{
		gear = std::max(1, currentGear - 1);
	}
	return gear;
}

CommandDataOut followDriving(SensorDataIn sd)
{
   // Get position of nearest opponent in front
	// via: - Sensor data
	// If distance below certain threshold
	// Drive in that direction (set angle)
	// If position in previous frame is known:
	//	 Calculate speed from old and new world positon
	//	 Try to adjust accel and brake to match speed of opponent
	//	 (Try to shift gear accordingly)
	// Save new world position in old position
	CommandDataOut cd = {0};

	if(!sd.isPositionTracked)
	{
		return cd;
	}
	vec2 curLeadPos = sd.leadPos;
	vec2 ownPos = sd.ownPos;

	// Get point of view axis of car in world coordinates
	// by substracting the positon of front corners and position of rear corners
	vec2 axis = (sd.cornerFrontRight - sd.cornerRearRight) + (sd.cornerFrontLeft - sd.cornerRearLeft);
	axis.normalize();

	//Get angle beween view axis and curleadPos to adjust steer
	vec2 leadVec = curLeadPos - ownPos;
	float dist = leadVec.len(); // absolute distance between cars

	// printf("DISTANCE: %f\n", leadVec.len());
	leadVec.normalize();


	// printf("CROSS: %f\n", axis.fakeCrossProduct(&leadVec));
	// printf("ANGLE: %f\n", RAD2DEG(asin(axis.fakeCrossProduct(&leadVec))));

	const float cross = axis.fakeCrossProduct(&leadVec);
	const float dot = axis * leadVec;
	const float angle = std::atan2(cross, dot) / sd.steerLock / 2.0;

	cd.steer = angle; // Set steering angle

	// Only possible to calculate accel and brake if speed of leading car known
	if(!sd.isSpeedTracked) // If position of leading car known in last frame
	{
		return cd;
	}

	float fspeed = sd.ownSpeed; // speed of following car

	float lspeed = sd.leadSpeed; // speed of leading car

	// adjusted distance to account for different speed, but keep it positive so brake command will not be issued if leading speed is too high
	float adist = std::max<float>(0.1, g_followDist + (fspeed - lspeed));

	// Accel gets bigger if we are further away from the leading car
	// Accel goes to zero if we are at the target distance from the leading car
	// Target distance is adjusted, dependent on the the speed difference of both cars
	// Accel = maxAccel if dist = threshold
	// Accel = 0 if dist = adist (adjusted target dist)
	//cd.accel = std::max<float>(0, std::min<float>(g_maxAccel, std::sqrt(g_maxAccel * (dist - adist) / (g_distThreshold - adist))));


	// Äquivalent to accel but the other way round
	//float b = std::max<float>(0, std::min<float>(g_maxBrake, std::sqrt(g_maxBrake * (adist - dist) / adist)));
	float dv = (lspeed - fspeed);
	if (dv > 0.0 && dist > 5.0)
	{
		cd.accel = 0.5 * dv;
	}
	else if (dist < 30.0)
	{
		cd.brakeFL = cd.brakeFR = cd.brakeRL = cd.brakeRR = -0.5 * dv;
	}
	//char format[256];
	//sprintf(format, "Speeds: %4.4f %4.4f %4.4f\n", lspeed, fspeed, dist);
	//PDBG("%s", format);

	// Individual brake commands for each wheel

	cd.gear = getSpeedDepGear(sd.ownSpeed, sd.curGear);

	return cd;
}

int main(int argc, char* argv[])
{
	PDBG("Hello, SimCom-Sim!\n");
	// Mostly copied from libports/src/test/lwip/http_clnt

	enum { BUF_SIZE = Nic::Packet_allocator::DEFAULT_PACKET_SIZE * 128 };

	SimCom::Connection ecu;
	//Terminal::Connection terminal;
	PDBG("ECU Server connection established.\n");
	static Timer::Connection timer;
	lwip_tcpip_init();

	char serv_addr[] = "10.0.2.5";

	if(lwip_nic_init(0, 0, 0, BUF_SIZE, BUF_SIZE))
	{
		PERR("We got no IP address!");
		return 0;
	}

	timer.msleep(2000);
	for(int j = 0; ; ++j) {
		timer.msleep(2000);
		PDBG("Connection attempt %d\n", j + 1);

		PDBG("Create new socket ...");
		int s = lwip_socket(AF_INET, SOCK_STREAM, 0 );
		if (s < 0) {
			PERR("No socket available!");
			continue;
		}

		PDBG("Connect to server ...");
		struct sockaddr_in addr;
		addr.sin_port = htons(66666);
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = inet_addr(serv_addr);

		if((lwip_connect(s, (struct sockaddr *)&addr, sizeof(addr))) < 0) {
			PERR("Could not connect!");
			lwip_close(s);
			continue;
		}

		PDBG("Connected successfully. Starting communication loop.\n");
		char buf[1024];
		CommandDataOut commandData = {0};
		SensorDataIn* pSensorDataIn = nullptr;
		SensorDataOut sensorDataOut = {0};
		const size_t outSize = 2 + sizeof(CommandDataOut) + sizeof(SensorDataOut) + 1;
		const size_t inSize = 2 + sizeof(SensorDataIn) + 1;
		char checksum = 0;
		char format[512];
		bool alive = true;
		//ecu.set_command_data(commandData);
		while (alive)
		{
			sensorDataOut = ecu.get_sensor_data();
			// Send Data.
			buf[0] = 0xaa;
			buf[1] = 0xcc;
			memcpy(&buf[2], &commandData, sizeof(CommandDataOut));
			memcpy(&buf[2]+sizeof(CommandDataOut), &sensorDataOut, sizeof(sensorDataOut));
			checksum = 0xaa ^ 0xcc;
			for (size_t i = 2; i < outSize - 1; ++i)
			{
				checksum ^= buf[i];
			}
			buf[outSize - 1] = checksum;

			//PDBG("Sending sensor and command data...\n");
			unsigned long bytes = lwip_send(s, buf, outSize, 0);
			if (bytes < outSize) {
				PERR("Couldn't send package...");
				lwip_close(s);
				alive = false;
				continue;
			}

			// Receive Sensor Data.
			bytes = lwip_recv(s, buf, inSize, 0);
			if (bytes < inSize) {
				PERR("Couldn't receive package...");
				lwip_close(s);
				alive = false;
				continue;
			}

			// Sanity checks
			if (buf[0] != 0xaa || buf[1] != 0xcc)
			{
				PDBG("Error: Incorrect package header. Received 0x%x%x Expected \"0xaacc\".", buf[0], buf[1]);
				alive = false;
				continue;
			}
			checksum = 0xaa ^ 0xcc;
			for (size_t i = 2; i < inSize - 1; ++i)
			{
				checksum ^= buf[i];
			}
			if (buf[inSize - 1] != checksum)
			{
				PDBG("Error: Invalid checksum %d, expected %d", checksum, buf[inSize - 1]);
				alive = false;
				continue;
			}

			pSensorDataIn = (SensorDataIn*)&buf[2];
			//sprintf(format, "Sensor Data received. Position: %f %f\n", pSensorDataIn->ownPos.x, pSensorDataIn->ownPos.y);
			//PDBG("%s", format);

			// Calculate following.
			commandData = followDriving(*pSensorDataIn);
			ecu.set_command_data(*pSensorDataIn);
		}

		/* Close socket */
		lwip_close(s);
	}


	return 0;
}
