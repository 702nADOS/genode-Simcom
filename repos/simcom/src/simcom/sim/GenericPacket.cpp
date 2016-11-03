/*
 * GenericPacket.cpp
 *
 *  Created on: Jun 15, 2016
 *      Author: ga49xig
 */

#include <simcom/GenericPacket.h>
#include <cstring>
#include <stdlib.h>
#include <iostream>

using std::cout;
using std::endl;

//TODO packettype einfügen


GenericPacket::GenericPacket(int maxsize){
	if(maxsize>=4){
		packetsize = 2*sizeof(int);
		maxpacketsize = maxsize;
		packetPtr = (char*)malloc((size_t)maxpacketsize);
		memcpy(packetPtr, &packetsize, sizeof(int));
		rawDataPtr = packetPtr+2*sizeof(int);
		getPtr = packetPtr+2*sizeof(int);
		addPtr = packetPtr+2*sizeof(int);
	}
	else{
		cout << "maxsize zu klein!" << endl;
	}
}

GenericPacket::~GenericPacket() {
	free(packetPtr);
}

int GenericPacket::add(void *ptr, int length){
	if((packetsize+length)>maxpacketsize){
		return -1;
	}
	memcpy(addPtr, ptr, length);
	addPtr += length;
	packetsize += length;
	memcpy(packetPtr, &packetsize, sizeof(int));

	return length;
}

int GenericPacket::get(void *ptr, int length){
	if ((packetsize-length)<0){
		return -1;
	}
	memcpy(ptr, getPtr, length);
	getPtr += length;
	packetsize -= length;
	memcpy(packetPtr, &packetsize, sizeof(int));

	return length;
}

int GenericPacket::getPacketLength(){
	memcpy(&packetsize, packetPtr, sizeof(int));
	return packetsize;
}

void GenericPacket::clear(){
	rawDataPtr = packetPtr + 2*sizeof(int);
	getPtr = packetPtr + 2*sizeof(int);
	addPtr = packetPtr + 2*sizeof(int);
	packetsize = 2*sizeof(int);
	memcpy(packetPtr, &packetsize, sizeof(int));
}

void GenericPacket::setPacketType(int packettype){
	memcpy(packetPtr + sizeof(int), &packettype, sizeof(int));
}

int GenericPacket::getPacketType(){
	int packettype;
	memcpy(&packettype, packetPtr + sizeof(int), sizeof(int));
	return packettype;
}

void* GenericPacket::getPacketPtr(){
	return packetPtr;
}

void* GenericPacket::getRawDataPtr(){
	return rawDataPtr;
}





