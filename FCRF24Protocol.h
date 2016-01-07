#include "RF24.h"

#define REQ_NGH_ANTENNA  0x01
#define ANS_NGH_ANTENNA  0x02
#define REQ_ADDR         0x03
#define ANS_ADDR         0x04

class FCRF24Protocol
{
	private:
		String nodeName;
		byte nodeAddress;
		String broadcastName;
		RF24 *rf24;
		bool validAddress;
		
		char routeNextHope[255];
		char routeKnownNode[255];
		char routePathLength[255];
		byte gwNextHopeNode;
		byte gwNHopesToGw;
	
		byte* String2Byte(String stringa);
		inline char* getEmptyBuffer() { return new char[32](); }
		void sendAnswerGeneric(char* message);
		
		void sendRequestNghAntenna();
		void sendRequestAddress() {};
		void receiveRequestNghAntenna(char* command);
		void receiveRequestAddress(char* command) {};
		
		void receiveAnswerNghAntenna(char* command);
		void receiveAnswerAddress(char* command) {};
		
	public:
		FCRF24Protocol() {};
		void init(String name);
		uint8_t  dataAvailable();
		void sendManagementCommand(unsigned char command, String payload = "");
		void receiveManagementCommand();
	
};