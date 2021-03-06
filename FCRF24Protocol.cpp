#include "FCRF24Protocol.h"

void FCRF24Protocol::init(String name, int ce, int csn)
{
	nodeName = name;
	nodeAddress = 0;
	validAddress = 0;
	broadcastName = "\xFFNode";
	fullAddress[0]=0x00;
	fullAddress[1]='N';
	fullAddress[2]='o';
	fullAddress[3]='d';
	fullAddress[4]='e';	
	fullAddress[5]='\0';
	hasNeighborhood = false;
	
	for (int j=0; j<255; j++)
	{
		routeKnownNode[j] = 0;
		routeNextHop[j] = 0;
		// routePathLength[j] = 0;
	}
	gwNextHopNode = 0;
	gwNHopsToGw = 0;
	
	rf24 = new RF24(ce,csn);
	rf24->begin();
	rf24->setAutoAck(1);
	rf24->setRetries(5,15);	
	rf24->openReadingPipe(1, fullAddress);
	byte address[6];
	broadcastName.getBytes(address, 6);
	rf24->openReadingPipe(2, address);
	
	randomSeed(analogRead(0));
}

void FCRF24Protocol::sendCommandGeneric ( char* message )
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
		case REQ_ADDR:
			sendRequestAddress();
			break;
		case TRACE_ROUTE:
			sendTraceRoute();
			break;
	}
	delete bName;
}

/*
 * byte 0 : command type
 * byte 1 : from address (if available) 
 * byte 2-21 : 0x00
 * byte 22-31 : nodeName right aligned
 */
void FCRF24Protocol::sendRequestNghAntenna()
{
	char* command = getEmptyBuffer();
	
	int l = nodeName.length();
	for (int j=0;j<l;j++)
		command[32-l+j]=nodeName[j];
	command[0] = REQ_NGH_ANTENNA;
	if (nodeAddress > 0)
		command[1] = nodeAddress;
	rf24->write(command, 32, 1);
	rf24->startListening();
	delete command;
}


/*
 * byte 0 : command type
 * byte 1-4: random id
 * byte 5-21 : 0x00
 * byte 22-31 : nodeName rigth aligned
 */

void FCRF24Protocol::sendRequestAddress()
{
	char* command = getEmptyBuffer();
	
	int l = nodeName.length();
	for (int j=0; j<l;j++)
		command[32-l+j] = nodeName[j];
	command[0] = REQ_ADDR;
	long randomId = random();
	lastRandomId = randomId;
	command[1]= (uint8_t)((randomId >> 24) & 0xFF);
	command[2]= (uint8_t)((randomId >> 16) & 0xFF);
	command[3]= (uint8_t)((randomId >> 8) & 0xFF);
	command[4]= (uint8_t)( randomId  & 0xFF);
	rf24->write(command, 32, 1);
	rf24->startListening();
	delete command;
}

/*
 * Byte 0: command type
 * Byte 1: from address
 * Byte 2: origin address
 * Byte 3: destination address
 * Byte 4: hop address
 * Byte 5-21: 0x00
 * Byte 22-31: nodeName
 * 
 */
void FCRF24Protocol::sendTraceRoute(uint8_t destinationAddress)
{
	if (validAddress && nodeAddress>0)
	{
		char* command = getEmptyBuffer();
		command[0] = TRACE_ROUTE;
		command[1] = nodeAddress;
		command[2] = nodeAddress;
		command[3] = destinationAddress;
		command[4] = gwNextHopNode;
		int l = nodeName.length();
		for (int j=0;j<l;j++)
			command[32-l+j]=nodeName[j];
		sendCommandGeneric(command);
		delete command;
	}
}


void FCRF24Protocol::receiveManagementCommand ( )
{
	char command[32];
	rf24->read(command, 32);
	Serial.print("Answer command: ");
	for (int j=0; j<32; j++)
	{;
		Serial.print(command[j], HEX);
		Serial.print("|");
	}
	Serial.println();
	
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
			
		case TRACE_ROUTE:
			receiveTraceRoute(command);
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
		answer[30]=gwNHopsToGw;
		answer[31]=nodeAddress;
		sendCommandGeneric(answer);
		delete answer;
		
		// if the sender address is valid all routing information
		// need to be updated
		if (command[1]!='\0')
		{
			routeKnownNode[command[1]] = 1;
			// routePathLength[command[1]] = 1;
			routeNextHop[command[1]] = command[1];
		}
	}
}

void FCRF24Protocol::receiveAnswerNghAntenna ( char* command )
{
	routeKnownNode[command[31]] = 1;
	routeNextHop[command[31]] = command[31];
	// routePathLength[command[31]] = command[3];
	
	if ((command[30]+1)<gwNHopsToGw)
	{
		gwNHopsToGw = command[30]+1;
		gwNextHopNode = command[31];
	}
	hasNeighborhood = true;
}

/*
 * Byte 0: command type
 * Byte 1-4: random packet id
 * Byte 5: address
 * Byte 6-21: 0x00
 * Byte 22-31: target nodeName
 * 
 */

void FCRF24Protocol::receiveRequestAddress ( char* command )
{
	/*
	 * The command is processed if and only if the node already has an address
	 */
	if (validAddress && nodeAddress>0)
	{
		long randomId = (command[1] << 24) + (command[2] << 16) + (command[3] << 8) + command[4];
		bool found = false;
		int i = 0;
		while (i < MaxAddressRequests && !found)
		{
			if (randomIDs[i] == randomId) found = true;
			i++;
		}
		if (!found)
		{
			// look for the first 0 entry in the randomIDs
			found = false;
			int i = 0;
			while (i<MaxAddressRequests && !found)
				if (randomIDs[i]==0)				
					found = true;				
				else				
					i++;
			if (found)
			{
				randomIDs[i] = randomId;
				sendCommandGeneric(command);
			}
		}
	}
	else
	{
		// this is the GW / DHCP
		#if defined _GW
		
		#endif
	}
}

void FCRF24Protocol::receiveAnswerAddress ( char* command )
{
	/*
	 * If this node is the target update data
	 * If this is not the target node, then delete the randomId from the
	 * list and forward the request 
	 * If the randomId is not in the list stop the forwarding
	 */
	
	if (!validAddress)
	{
		bool found = true;
		int l = nodeName.length();
		int i = l;
		while(found && i>0)
		{
			if (nodeName[i-1] != command[31-l+i])
				found = false;
			else
				i--;
		}
		if (found)
		{
			nodeAddress = command[5];
			validAddress = true;
			fullAddress[0] = command[5];
			rf24->closeReadingPipe(1);
			rf24->openReadingPipe(1, fullAddress);
			Serial.print("Address: ");
			Serial.println(nodeAddress);
		}
	}
	else
	{
		long randomId = (command[1] << 24) + (command[2] << 16) + (command[3] << 8) + command[4];
		if (nodeAddress > 0 && lastRandomId != randomId)	// GW never forward request/answer address and this node message
		{
			bool found = false;
			int i = 0;
			while (i < MaxAddressRequests && !found)
			{
				if (randomIDs[i] == randomId)
				{
					found = true;
					randomIDs[i] = 0;
					sendCommandGeneric(command);
				}
				i++;
			}
		}
	}
}

void FCRF24Protocol::receiveTraceRoute ( char* command )
{
	if (validAddress && nodeAddress >0)
	{
		if (nodeAddress == command[4])
		{
			routeNextHop[command[2]]=command[1];
			command[1] = nodeAddress;
			command[4] = gwNextHopNode;
			sendCommandGeneric(command);
		}
	}
}


/*
 * 
 * ********************** DATA TRASMISSION
 * 
 */

/*
 * Byte 0: message type
 * Byte 1: from address
 * Byte 2: to address
 * Byte 3: reserved
 * byte 4-31: data
 */

void FCRF24Protocol::sendPacket ( uint8_t toAddress, char* data, bool ack, bool cmd )
{
	char* message = getEmptyBuffer();
	fullAddress[0] = gwNextHopNode;
	rf24->openWritingPipe(fullAddress);
	fullAddress[0] = nodeAddress;
	// Data packet type
	if (cmd) message[0] = CMD;
	if (ack) message[0] = DATA_ACK;
	if (!ack & !cmd) message[0] = DATA_NOACK;
	
	// Data from
	message[1] = nodeAddress;
	message[2] = toAddress;
	message[3] = 0x00;
	for (int i=0;i<28;i++)
		message[4+i] = data[i];
	rf24->stopListening();
	rf24->write(message, 32, 1);
	rf24->startListening();	
	
	delete message;
}

void FCRF24Protocol::forwardPacket ( char* message )
{
	fullAddress[0] = gwNextHopNode;
	rf24->openWritingPipe(fullAddress);
	fullAddress[0] = nodeAddress;
	rf24->stopListening();
	rf24->write(message, 32, 1);
	rf24->startListening();	
}


void FCRF24Protocol::receivePacket (char* data)
{
	/*
	 * If to address is the one of this node the menage address
	 * else forward address to next hop
	 */
	
	char* message = getEmptyBuffer();
	rf24->read(message, 32);
	message[32] = '\0';
	
	if (message[2]==nodeAddress)
	{
		Serial.print("Data: ");
		for (int j=0;j<=28;j++)
			Serial.print(message[4+j]);
		Serial.println();
		if (data != NULL)
			for (int j=0;j<=28;j++)
				data[j]=message[4+j];
	}
	else
	{
		forwardPacket(message);
	}
	
	delete message;
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
