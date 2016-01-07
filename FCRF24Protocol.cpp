#include "FCRF24Protocol.h"

void FCRF24Protocol::init(String name)
{
	nodeName = name;
	nodeAddress = 0;
	validAddress = 0;
	broadcastName = "\xFFNode";
	
	for (int j=0; j<255; j++)
	{
		routeKnownNode[j] = 0;
		routeNextHope[j] = 0;
		routePathLength[j] = 0;
	}
	gwNextHopeNode = 0;
	gwNHopesToGw = 0;
	
	rf24 = new RF24(7,8);
	rf24->begin();
	rf24->setAutoAck(1);
	rf24->setRetries(5,15);
	byte address[6] = "1Node";	
	rf24->openReadingPipe(1, address);
	broadcastName.getBytes(address, 6);
	rf24->openReadingPipe(2, address);	
}

void FCRF24Protocol::sendAnswerGeneric ( char* message )
{
	byte* bName;
	bName = String2Byte(broadcastName);
	rf24->openWritingPipe(bName);
	rf24->stopListening();
	rf24->write(message, 32, 1);
	rf24->startListening();
	delete bName;
}

uint8_t FCRF24Protocol::dataAvailable()
{
	uint8_t pipeNumber;
	rf24->available(&pipeNumber);
	return pipeNumber;
}



void FCRF24Protocol::sendManagementCommand(unsigned char command, String payload)
{
	byte* bName;
	bName = String2Byte(broadcastName);
	rf24->openWritingPipe(bName);
	rf24->stopListening();
	switch(command)
	{
		case REQ_NGH_ANTENNA:
			sendRequestNghAntenna();
			break;	
	}
	delete bName;
}

void FCRF24Protocol::sendRequestNghAntenna()
{
	char* command = getEmptyBuffer();
	
	int l = nodeName.length();
	for (int j=0;j<l;j++)
		command[32-l+j]=nodeName[j];
	command[0] = REQ_NGH_ANTENNA;
	rf24->write(command, 32, 1);
	rf24->startListening();
	delete command;
}





void FCRF24Protocol::receiveManagementCommand ( )
{
	char command[32];
	rf24->read(command, 32);
	
	switch(command[0])
	{
		case ANS_NGH_ANTENNA:
			receiveAnswerNghAntenna(command);
			break;
					
		case ANS_ADDR:
			receiveAnswerAddress(command);
			break;
			
		case REQ_NGH_ANTENNA:
			receiveRequestNghAntenna(command);
			break;
				
		case REQ_ADDR:
			receiveRequestAddress(command);
			break;
	}
}

/*
 * Byte 0: command type
 * Byte 1: from address
 * Byte 2: to address
 * Byte 3: number of hops = 1 : direct connection
 * Byte 4-29: 0x00
 * Byte 30: hops to gw (0 = no route to gw)
 * Byte 31: node address
 * 
 */
void FCRF24Protocol::receiveRequestNghAntenna ( char* command )
{
	if (validAddress)
	{
		char* answer = getEmptyBuffer();
		answer[0]=ANS_NGH_ANTENNA;
		answer[1]=nodeAddress;
		answer[2]=0;
		answer[3]=1;
		answer[30]=gwNHopesToGw;
		answer[31]=nodeAddress;
		sendAnswerGeneric(answer);
		delete answer;
	}
}

void FCRF24Protocol::receiveAnswerNghAntenna ( char* command )
{
	Serial.println("receiveAnswerNghAntenna");
	Serial.println(command[31],HEX);
	Serial.println(command[3],HEX);
	routeKnownNode[command[31]] = 1;
	routeNextHope[command[31]] = command[31];
	routePathLength[command[31]] = command[3];
}


// -------------------------------
byte* FCRF24Protocol::String2Byte(String stringa)
{
	byte* res = new byte(stringa.length()+1);
	for (int j=0; j< stringa.length(); j++)
		res[j] = stringa[j];
	res[stringa.length()]='\0';
	return res;
}
