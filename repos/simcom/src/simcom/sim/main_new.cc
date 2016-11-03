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
#include <simcom/GenericPacket.h>
#include <simcom/packettypes.h>

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



	GenericPacket psend(4096);
	GenericPacket precv(4096);

	enum { BUF_SIZE = Nic::Packet_allocator::DEFAULT_PACKET_SIZE * 128 };

        //	SimCom::Connection ecu;
	//Terminal::Connection terminal;
	PDBG("ECU Server connection established.\n");
	static Timer::Connection timer;
	lwip_tcpip_init();

	if(lwip_nic_init(0, 0, 0, BUF_SIZE, BUF_SIZE))
	//if(
		//lwip_nic_init(inet_addr("10.0.3.5"), // ip
		//lwip_nic_init(inet_addr("10.0.2.5"), // ip
		//inet_addr("255.255.255.0"), // subnet mask
		//0, // gw
		//BUF_SIZE, BUF_SIZE))
	{
		PERR("We got no IP address!");
		return 0;
	}

	timer.msleep(2000);
	for(int j = 0; ; ++j) {
		timer.msleep(2000);
		PDBG("Connection attempt %d\n", j + 1);

		PDBG("Create new socket ...");
		int sockfd = lwip_socket(AF_INET, SOCK_STREAM, 0 );
		if (sockfd < 0) {
			PERR("No socket available!");
			continue;
		}

		PDBG("Create new socket to Beaglebone...");
		int sock2fd = lwip_socket(AF_INET, SOCK_STREAM, 0);
		if (sock2fd < 0) {
			PERR("No socket available!");
			continue;
		}

		PDBG("Waiting for clients  ...");
		struct sockaddr_in qemu_addr;
		qemu_addr.sin_port = htons(8000);
		qemu_addr.sin_family = AF_INET;
		qemu_addr.sin_addr.s_addr = INADDR_ANY;


		if ((lwip_bind(sockfd, (struct sockaddr *)&qemu_addr, sizeof(qemu_addr))) < 0) {
			PERR("Could not connect!");
		 	lwip_close(sockfd);
		 	continue;
		}
		
		
		lwip_listen(sockfd,5);
		socklen_t addr_len = sizeof(qemu_addr);
		int newsockfd = lwip_accept(sockfd, (struct sockaddr *) &qemu_addr, &addr_len);


		struct sockaddr_in beagle_addr;
		beagle_addr.sin_port = htons(5000);
		beagle_addr.sin_family = AF_INET;
		//beagle_addr.sin_addr.s_addr = inet_addr("10.0.1.209");
		//beagle_addr.sin_addr.s_addr = inet_addr("10.0.2.209");
		beagle_addr.sin_addr.s_addr = INADDR_ANY;

		PDBG("Connect to Beaglebone...");
		if ((lwip_bind(sock2fd, (struct sockaddr *)&beagle_addr, sizeof(beagle_addr))) < 0) {
			PERR("Could not connect!");
			lwip_close(sock2fd);
			continue;
		}


		lwip_listen(sock2fd,5);
		addr_len = sizeof(beagle_addr);
		int newsock2fd = lwip_accept(sock2fd, (struct sockaddr *) &beagle_addr, &addr_len);


		PDBG("Connected! GoGo");
		//Please comment out if you are using debug messages for debugging
		PDBG("Only shows error notes because of performance issues!");

		bool alive = true;
		

		while (alive)
		{
			long bytes;
			//Data Communication to QEMUSAVM
			
			precv.clear();
			bytes = lwip_recv(newsockfd,precv.getPacketPtr(), 4096, 0);
			if (bytes<precv.getPacketLength()){
				PDBG("Error: Packet receive from qemu not complete!");
				continue;
			}

			switch (precv.getPacketType()){
				case packettype_data_to_simcom:
				{
					
				
					float brake = 42;
				 	precv.get(&brake, sizeof(float));
				    float brakeFL = 42;
					precv.get(&brakeFL, sizeof(float));
					float brakeFR = 42;
					precv.get(&brakeFR, sizeof(float));
					float brakeRL = 42;
					precv.get(&brakeRL, sizeof(float));
					float brakeRR = 42;
					precv.get(&brakeRR, sizeof(float));
					

					float speed = 0.0;
					precv.get(&speed, sizeof(float));
					//PDBG("Speed = %d", (int)speed);

					int gear = 0;
					precv.get(&gear, sizeof(int));

					//PDBG("Gear = %d", gear);

					gear = getSpeedDepGear(speed, gear);
		//			PDBG("Gear after function call = %d", gear);

					//Sending gear to pululu
					psend.clear();
					psend.setPacketType(packettype_send_gear);
					
					PDBG("%d", sizeof(float));
				    psend.add(&brake, sizeof(float));
				    psend.add(&brakeFL, sizeof(float));
				    psend.add(&brakeFR, sizeof(float));
				    psend.add(&brakeRL, sizeof(float));
				    psend.add(&brakeRR, sizeof(float));
				    psend.add(&gear, sizeof(int));
					
					/*
					 PDBG("Brake is %d", (int) (brake * 2000));
					 PDBG("BrakeFL is %d", (int) (brakeFL * 2000));
					 PDBG("BrakeFR is %d", (int) (brakeFR * 2000));
					 PDBG("BrakeRL is %d", (int) (brakeRL * 2000));
					 PDBG("BrakeRR is %d", (int) (brakeRR * 2000));
					*/
				    
					//PDBG("Sending gear to Pululu....");
					bytes = lwip_send(newsock2fd, psend.getPacketPtr(), psend.getPacketLength(), 0);
					if (bytes<psend.getPacketLength()){
						PERR("Packet not send correctly");
						continue;
					}
					//PDBG("Send Done!");

					//receiving answer from polulu
					precv.clear();
					bytes = lwip_recv(newsock2fd,precv.getPacketPtr(), 4096, 0);
					if (bytes<precv.getPacketLength()){
						PDBG("Error: Packet receive from beagle  not complete!");
						continue;
					}

					//Sending answer to Qemusavm
					psend.clear();
					psend.setPacketType(packettype_send_gear);
					psend.add(&gear, sizeof(gear));

					//PDBG("Sending gear to QEMUSAVM....");
					bytes = lwip_send(newsockfd, psend.getPacketPtr(), psend.getPacketLength(), 0);
					if (bytes<psend.getPacketLength()){
						PERR("Packet not send correctly");
						continue;
					}
					//PDBG("Send Done!");

					break;
				}

				default:
				{
					PDBG("Unbekannter Pakettyp: %i", precv.getPacketType());
					psend.setPacketType(packettype_undefined);
				}

			}

		}


		/* Close socket */
		lwip_close(sock2fd);
		lwip_close(sockfd);
	}


	return 0;
}
