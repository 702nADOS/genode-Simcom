#include <algorithm>

#include <base/printf.h>
#include <base/thread.h>
#include <util/string.h>
#include <timer_session/connection.h>
#include <qemusavm_session/connection.h>
#include <terminal_session/connection.h>

#include <qemusavm/GenericPacket.h>
#include <qemusavm/packettypes.h>

#include <nic/packet_allocator.h>

#include <lwip/lwipopts.h>
#include <lwip/sockets.h>
#include <lwip/api.h>
#include <lwip/genode.h>
#include <qemusavm/types.h>
#include <base/thread.h>

#include <base/sleep.h>



void process_sd_data(int* sockfd, CommandDataOut *pCommandData, SensorDataIn* pSensorDataIn,  SensorDataOut *pSensorDataOut){

	//Size of the incoming and outgoing packet
	const size_t outSize = 2 + sizeof(CommandDataOut) + sizeof(SensorDataOut) + 1;
	const size_t inSize = 2 + sizeof(SensorDataIn) + 1;

	//Timer initialization to sleep if necessary
    Timer::Connection timer;
	//Buffer to temporarily store data from socket in it
	char buf[512];
	//checksum for sanity check of packets
	char checksum = 0;

	//Fill SensorDataOut with DummyData
	pSensorDataOut->engineTemp = /*(float) ( (int) */(pSensorDataOut->engineTemp+1)/* %  100)*/;

	pSensorDataOut->engineRPM = 0;
//    PDBG("CommandDataGear = %i", pCommandData->gear);

	// Send Data.
	buf[0] = 0xaa;
	buf[1] = 0xcc;
	memcpy(&buf[2], pCommandData, sizeof(CommandDataOut));
	memcpy(&buf[2]+sizeof(CommandDataOut), pSensorDataOut, sizeof(SensorDataOut));
	checksum = 0xaa ^ 0xcc;
	for (size_t i = 2; i < outSize - 1; ++i)
	{
		checksum ^= buf[i];
	}
	buf[outSize - 1] = checksum;
	
//	PDBG("Sending Packet to SpeedDreams");
	//PDBG("Sending sensor and command data...\n");
	unsigned long bytes = lwip_send(*sockfd, buf, outSize, 0);
	if (bytes < outSize) {
		PERR("Couldn't send package...");
		lwip_close(*sockfd);
		//continue;
		return;
	}

//	PDBG("Receiving Packet from SpeedDreams");
	// Receive Sensor Data.
	bytes = lwip_recv(*sockfd, buf, inSize, 0);
	if (bytes < inSize) {
		PERR("Couldn't receive package...");
		lwip_close(*sockfd);

		//continue;
		return;
		}

	// Sanity checks
	 if (buf[0] != 0xaa || buf[1] != 0xcc)
	 {
		PDBG("Error: Incorrect package header. Received 0x%x%x Expected \"0xaa 0x-cc\".", buf[0], buf[1]);

		//continue;
		return;
	 }
	 checksum = 0xaa ^ 0xcc;
	 for (size_t i = 2; i < inSize - 1; ++i)
	 {
		checksum ^= buf[i];
	 }
	 if (buf[inSize - 1] != checksum)
	 {
		PDBG("Error: Invalid checksum %d, expected %d", checksum, buf[inSize - 1]);

		//continue;
		return;
	 }

    memcpy(pSensorDataIn,&buf[2], sizeof(SensorDataIn));
	//pSensorDataIn = (SensorDataIn *) &buf[2];
//	sprintf(format, "Sensor Data received. Position: %f %f\n", pSensorDataIn->ownPos.x, pSensorDataIn->ownPos.y);

//	PDBG("Current Speed = %d:", (int) pSensorDataIn->ownSpeed);
//	PDBG("Current Gear(SensorDataIn) = %i", pSensorDataIn->curGear);
	//printf("Current Speed = %f:", pSensorDataIn->ownSpeed);

	/*
	pCommandData->gear = getSpeedDepGear(pSensorDataIn->ownSpeed, pSensorDataIn->curGear);
	PDBG("Current Gear(commandData) = %i", pCommandData->gear);
	*/

	//}
}

void process_simcom_data(int* sockfd, CommandDataOut *pCommandData, SensorDataIn *pSensorDataIn,
	  SensorDataOut *psensorDataOut, GenericPacket *psend, GenericPacket *precv)
{
	int bytes = 0;
    static Timer::Connection timer;
//	PDBG("Process SimcomData");

	psend->clear();
//	PDBG("sendpacket cleared");

	psend->setPacketType(packettype_data_to_simcom);

//	PDBG("Packettype is set");
	float speed = pSensorDataIn->ownSpeed;
	float brake = pSensorDataIn->brake;
	float brakeFL = pSensorDataIn->brakeFL;
	float brakeFR = pSensorDataIn->brakeFR;
	float brakeRL = pSensorDataIn->brakeRL;
	float brakeRR = pSensorDataIn->brakeRR;

	PDBG(" int size %d float size %d",sizeof(int), sizeof(float)  );
	PDBG("Gear %d and brake %d", pSensorDataIn->curGear, (int) ( pSensorDataIn->brake * 2000));
	PDBG("brakeFL = %d", (int) (brakeFL * 2000));
	PDBG("brakeFR = %d", (int) (brakeFR * 2000));
	PDBG("brakeRL = %d", (int) (brakeRL * 2000));
	PDBG("brakeRR = %d", (int) (brakeRR * 2000));


	psend->add(&brake, sizeof(float));
	psend->add(&brakeFL,sizeof(float));
	psend->add(&brakeFR,sizeof(float));
	psend->add(&brakeRL,sizeof(float));
	psend->add(&brakeRR,sizeof(float));


//	PDBG("speed = %d", (int)speed);
//	PDBG("speed (sensordata) = %d", (int) pSensorDataIn->ownSpeed);

	psend->add(&speed, sizeof(float));

//	PDBG("Process gear SimcomData");
	int gear = pSensorDataIn->curGear;
	psend->add(&gear, sizeof(int));


//	PDBG("Sending Packet to SimCom");
	bytes = lwip_send(*sockfd, psend->getPacketPtr(), psend->getPacketLength(), 0);
	if (bytes < psend->getPacketLength()){
		PERR("Packet to Simcom not send correctly!");
	}

	precv->clear();
//	PDBG("Receiving Packet from SimCom");
	bytes = lwip_recv(*sockfd, precv->getPacketPtr(), 4096, 0);
	if (bytes < 1){
//	PDBG("Error: Package not received correctly!");
		 return;
	}

	if (precv->getPacketType() == packettype_send_gear){
		precv->get(&gear, sizeof(int));
		pCommandData->gear = gear;
	}
	else{
		PDBG("Wrong Packettype!!!");
	}


//	PDBG("Received Gear is %i", gear);

 }

void con2service(char* servicename , char* ip_addr, int port , int* _sockfd){

   static Timer::Connection timer;
   bool connected = false;
   int count = 0;
   struct sockaddr_in sock_addr;
   sock_addr.sin_family = AF_INET;
   sock_addr.sin_port = htons(port);
   sock_addr.sin_addr.s_addr = inet_addr(ip_addr);

   while(!connected){
   
		if(count < 3){
		   PDBG("Create new socket for %s ...", servicename);
		   count++;
		}
		*_sockfd = lwip_socket(AF_INET, SOCK_STREAM, 0 );
		if (*_sockfd < 0) {
		    if(count < 3){
			PERR("No sim socket available!");
			PERR("This Error is only displayed once! But functions will continue!\n");
			count++;
			}
			timer.msleep(2000);
			continue;
		}
		
	  if (lwip_connect(*_sockfd, (struct sockaddr *)&sock_addr, sizeof(sock_addr)) < 0)
		//if (lwip_bind(s, addr.sin_addr.s_addr, sizeof(addr)) < 0)
		{
		    if(count < 3){
			PERR("ERROR on binding socket to %s \n", servicename);
			PERR("This Error is only displayed once! But functions will continue!\n");
			count++;
			}
			timer.msleep(2000);
			lwip_close(*_sockfd);
			continue;
		}
   PDBG("Connected to %s!\n", servicename);
   connected = true;
   }
	PDBG("Starting communication loop with %s .\n", servicename);

   return;
}




int main(int argc, char* argv[])
{
	PDBG("Hello, QemusaVm-Sim!\n");
	// Mostly copied from libports/src/test/lwip/http_clnt

	//Structs to allow speedDreams qemusavm connection
	CommandDataOut commandData = {0};
	SensorDataIn sensorDataIn = {0};
	SensorDataOut sensorDataOut = {0};

	//Generic Packages for the simcom qemusavm connection
	//one for sending one for receiving
	GenericPacket psend(1024);
	GenericPacket precv(1024);

	//initialize speeddreams and simcom socket descriptor
	//for later use in upstart "con2service" and working "proccess_*_data" functions
	int speeddreamssockfd = 0;
	int simcomsockfd = 0;
	
	
	enum { BUF_SIZE = Nic::Packet_allocator::DEFAULT_PACKET_SIZE * 128 };

	static Timer::Connection timer;
   //Initializing NIC Interface
	lwip_tcpip_init();

		 //Getting IP Address
	  //if you want to use static ip addresses use code below instead of the line above
	  //we assume that this is normally not necessary and you should always use a dhcp server
	  //with static leases to achieve static ip adresses for your component!
		   
			 /*   if(lwip_nic_init(inet_addr("10.0.3.53"), // ip
					 		 inet_addr("255.255.255.0"), // subnet mask
					 	 0, // gw
					 		 BUF_SIZE, BUF_SIZE)) */

    if(lwip_nic_init(0, 0, 0, BUF_SIZE, BUF_SIZE))
    {
		PERR("We got no IP address!");
		return 0;
	}

    //Sleep to wait for IP assignement

	
   //Starts a socket for the Service with the parameters servicename (only for debugging messages), ip address to connect to, port to connect to
   //and prior created socket as pointer.
	PDBG("SpeedDreams connection routine fired ...");
   con2service((char*)"SpeedDreams", (char*)"10.0.3.55" , 9000, &speeddreamssockfd);
   

   //Starts a socket for the Service with the parameters servicename (only for debugging messages), ip address to connect to, port to connect to
   //and prior created socket as pointer.
    PDBG("Simcom connection routine fired ...");
	con2service((char*)"Simcom" , (char*)"10.0.2.5" , 8000, &simcomsockfd );

   timer.msleep(10000);
   //never stops data exchange, should be later changed to gracefully shut down the simulation
   //consider to also change it in speeddreams, because a connection close is also not handle there.
	while (1) {

	  		//processes data from and to speeddreams gets the socket and 3 needed Data pointers for dataexchange with speeddreams
			//also include a method to give date from speeddreams to simcom process function, see function code
			process_sd_data(&speeddreamssockfd, &commandData, &sensorDataIn, &sensorDataOut);
			
			//processes data from and to simcom  which will be used in the speeddreams process function to controll speeddreams or update it
			/*
			 *
			 * If you want to connect a fourth party please duplicate and modify this function as well as their connection con2service(simcom)  for the fourth party side
			 * and add data you want to process from and to speeddreams to the speeddreams process function above
			 *
			 */
			process_simcom_data(&simcomsockfd, &commandData, &sensorDataIn, &sensorDataOut, &psend, &precv);
		 }


	return 0;
}
