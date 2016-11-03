/*
 * GenericPacket.h
 *
 *  Created on: Jun 15, 2016
 *      Author: ga49xig
 */

#ifndef GENERICPACKET_H_
#define GENERICPACKET_H_

/*
 * Generisches Paket mit folgendem Aufbau:
 * 1. Länge
 * 2. Pakettyp
 * 3. Rohdaten
 */


class GenericPacket {
public:
	GenericPacket(int maxsize);
	virtual ~GenericPacket();

	/*
	 * Fügt Daten vom Pointer der Länge length dem Paket hinzu
	 */
	int add(void *ptr, int length);

	/*
	 * Holt Daten der Länge length aus dem Paket und kopiert sie zum Pointer
	 */
	int get(void *ptr, int length);

	/*
	 * Gibt die aktuelle Paketlänge zurück
	 */
	int getPacketLength();

	void clear();

	void setPacketType(int);

	int getPacketType();

	void* getPacketPtr();

	void* getRawDataPtr();



private:
	//Packetgrösse
	int packetsize;

	//Pointer auf gesamtes Packet
	char *packetPtr;

	//Pointer auf aktuelle Position in den Rohdaten
	char *rawDataPtr;
	char *getPtr;
	char *addPtr;

	int maxpacketsize;
};

#endif /* GENERICPACKET_H_ */
