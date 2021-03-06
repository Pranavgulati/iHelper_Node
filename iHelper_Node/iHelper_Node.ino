/*
 Name:		iHelper_Node.ino
 Created:	2/3/2016 1:56:16 PM
 Author:	Pranav
*/


#define USE_SERIAL Serial
#define DEBUG
#define DEBUG_HTTPCLIENT Serial.printf

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <Wire.h>

//EEPROM STRUCTURE
/*
IDend length of Hkey =hardware key loaded at boot
Ukey of length Ukeylen
Last configured ssid and pwd may or maynot be required
OFFSET including the above
Relaystatus of length NUMRELAY loaded at boot
Relaymodes of length NUMRELAY loaded at boot
schedules
*/

#define DS3231_I2C_ADDRESS 0x68
#define NUMHOPS 5 //no of hops fromt the main router to the leaf 
#define URL "www.mysh.net23.net"
#define PORT 80
#define NUMRELAY 2
#define RELAYstart 12// gpio pin no from which relay is connected
#define MY_PWD "passphrase"
#define MY_PREFIX "iHelp"
#define Hkeypart 4
#define SERVER_IP_ADDR "192.168.4.1"
#define SERVER_PORT 2123
#define Server_WaitResponse "@wait "
#define ERROR1 "@wait E1@"
#define OFFSET 101
byte RELAYstatus[NUMRELAY] = { 'O', 'F' };
byte RELAYmodes[NUMRELAY] = { 'M', 'M' };
String ssid, password, Ukey;
byte requestCounter = 0;
byte totalQuery = 0;
enum ConnectionMode{ INTERNET, NO_INTERNET, TO_ROUTER, TO_MESH, NONE };

// this is the hardware key that is linked with the hardware
// and must be changed everytime on flashing the hardware and shall be associated to ONE USER ONLY
String Hkey;
//byte IDend = Hkey.length() + 1;
#define IDend 16
#define Ukeylen 21
#define SSIDlen 32
#define PASSWORDlen 20


//HTTPClient http;

ESP8266WebServer server(2123);// to initialise a server that connects at port 80
String exceptions;
ConnectionMode connection = NONE;
struct query {
	char HID[17];
	char ques[30];
	char response[30];
	query *next;
	query *prev;
};
query *temp, *currentQuery;
void addQuery(const char hid[], const char answer[], const char ques[]){
	Serial.printf("add q total %d", totalQuery);
	if (currentQuery == NULL)
	{
		currentQuery = new query;
		currentQuery->next = NULL;
		currentQuery->prev = NULL;
		ets_strncpy(currentQuery->HID, hid, sizeof(currentQuery->HID));
		ets_strncpy(currentQuery->response, answer, sizeof(currentQuery->response));
		ets_strncpy(currentQuery->ques, ques, sizeof(currentQuery->ques));
		totalQuery++;

	}
	else
	{
		move2end();//just a precaution
		temp = new query;
		temp->prev = currentQuery;
		temp->next = NULL;
		currentQuery->next = temp;
		ets_strncpy(temp->HID, hid, sizeof(temp->HID));
		ets_strncpy(temp->response, answer, sizeof(temp->response));
		ets_strncpy(temp->ques, ques, sizeof(temp->ques));
		currentQuery = temp;
		totalQuery++;
	}
	Serial.printf("%s,%s,%s\n", currentQuery->HID, currentQuery->ques, currentQuery->response);
}
void removeQuery(){
	query* del = currentQuery;
	if (currentQuery->prev == NULL && currentQuery->next == NULL){ currentQuery = NULL; }
	else if (currentQuery->prev == NULL){
		(currentQuery->next)->prev = currentQuery->prev;
		currentQuery = currentQuery->next;

	}
	else if (currentQuery->next == NULL){
		currentQuery->prev->next = currentQuery->next;
		currentQuery = currentQuery->prev;

	}
	else{
		currentQuery->prev->next = currentQuery->next;
		(currentQuery->next)->prev = currentQuery->prev;
		currentQuery = currentQuery->prev;

	}

	delete del;
	totalQuery--;
	Serial.printf("remv q total %d", totalQuery);
}
int editQuery(const char hid[], const char answer[], const char ques[]){
	if (currentQuery == NULL)
	{
		return -1;
	}
	else
	{
		Serial.println(answer);
		ets_strncpy(currentQuery->HID, hid, sizeof(currentQuery->HID));
		ets_strncpy(currentQuery->response, answer, sizeof(currentQuery->response));
		ets_strncpy(currentQuery->ques, ques, sizeof(currentQuery->ques));
	}
	Serial.printf("editQuery total %d", totalQuery);
}
void move2start(){
	while (currentQuery->prev != NULL){ currentQuery = currentQuery->prev; }
}
void move2end(){
	while (currentQuery->next != NULL){ currentQuery = currentQuery->next; }
}
String isQuery(const char hid[], const char ques[]){
	if (currentQuery == NULL){ return "0"; }
	move2start();
	while (1){
		if (currentQuery->next != NULL){
			if (ets_strcmp(currentQuery->HID, hid) != 0 || ets_strcmp(currentQuery->ques, ques) != 0){
				currentQuery = currentQuery->next;
			}
			else{
				if ((String)currentQuery->response != (String)(Server_WaitResponse + (String)currentQuery->HID + "@")) { return currentQuery->response; }
				move2end(); return "2";
			}
		}
		else {
			if (ets_strcmp(currentQuery->HID, hid) == 0 && ets_strcmp(currentQuery->ques, ques) == 0){
				Serial.print(currentQuery->HID);
				Serial.print(currentQuery->response);
				if ((String)currentQuery->response != (String)(Server_WaitResponse + (String)currentQuery->HID + "@")){ return currentQuery->response; }
				return "2";
			}
			else{ return "0"; }

		}

	}

}
void move2query(byte index){
	move2start();
	index--;
	for (index; index > 0; index--){
		currentQuery = currentQuery->next;
	}
}

bool checkExceptions(String sample){
	if (exceptions.indexOf(sample) == -1){ return true; }
	else { return false; }
}


String sendGET(const char url[], int port, const char path[], int length){

	String recv;
	String temp;
	temp.reserve(120);
	WiFiClient client;
	
	//	if (client.connect(IPAddress(192,168,4,1), port)){
	if (client.connect(url, port)){
		client.print("GET ");
		client.print(path);
		client.print(" HTTP/1.1\r\n");
		client.print("Host: ");
		client.print(url);
		client.print("\r\nUser-Agent: Mozilla/5.0\r\n");
		client.print("Connection: close\r\n\r\n");
		Serial.println("WOW connected");
	}

	int wait = 1000;
	while (client.connected() && !client.available() && wait--){
		delay(3);
	}
	while (client.available()){
		temp += (char)client.read();
	}
	
	recv = temp.substring(temp.indexOf("@"), temp.lastIndexOf("@") + 1);
	//// Attempt to make a connection to the remote server
	//if (client.connect("192.168.4.1", 80)) {
	//	Serial.println("\n connect\n");
	//	
	//}
	//else{ Serial.println("not connected\n"); return "not"; }
	//
	//// Make an HTTP GET request
	//client.println("GET /mesh?Hof=htyg&ques=command HTTP/1.1");
	//client.print("Host: ");
	//client.println(url);
	//client.println("Connection: close");
	//client.println();
	//Serial.println("\nmade req\n");
	//
	//
	//	delay(100);
	//	String temp = "";
	//	while (client.available()) {
	//		char c = client.read();
	//			temp += c;
	//		Serial.print(c);
	//	}
	//	
	//	recv = temp.substring(temp.indexOf("@"), temp.lastIndexOf("@") + 1);
	//
	//Serial.printf("%s \n %d \n %s \n", url, port, path);
	/*if (httpCode == 200) {
	String temp= http.getString();
	Serial.println(temp);
	recv=temp.substring(temp.indexOf("@"), temp.lastIndexOf("@")+1);
	}
	else {
	recv = ERROR1;
	}*/
	//Serial.println(recv);
	client.stop();
	return recv;
}
void setStatus(byte relayno, byte status) {
	USE_SERIAL.println("setStatus= ");
	if (relayno <= NUMRELAY) {
		EEPROM.write(OFFSET - 1 + relayno, status);
		RELAYstatus[relayno - 1] = status;
		digitalWrite(RELAYstart + relayno - 1, status);
		USE_SERIAL.printf("%d , %d", relayno, status);
	}
	EEPROM.commit();

}

void checkSchedule(byte hour, byte minute, byte status) {
	//  current time in 24h format is
	//will do checking of all schedules here using relay mode
	Serial.println(hour);
	Serial.println(minute);
	Serial.println("----");
	int times[NUMRELAY][4] = { 0 };
	for (byte rel = 0; rel < NUMRELAY; rel++) {
		if (RELAYmodes[rel] == 'S') {
			int k = OFFSET + (NUMRELAY * 2) + (rel * 32) + (status * 16);
			for (byte i = 0; i < 4; i++) {
				for (byte j = 0; j < 4; j++) {
					times[rel][i] = (times[rel][i] * 10) + (EEPROM.read(k + j + 4 * i) - '0');
				}
			}

			for (byte i = 0; i < 4; i++) {
				//           Serial.println(times[rel][i])   ;
				if (times[rel][i] == ((hour * 100) + minute) && RELAYstatus[rel] != status) { /*turn status relay*/
					setStatus(rel + 1, status);
				}
			}

		}
	}
}


class Command {
public:
	Command(String incoming) {
		start = incoming.indexOf("<"); end = incoming.indexOf(">", start + 1);
		byte i = 0;
		if (start != 255 && end != 255) {
			byte temp = start; //stores previousindex of ","
			for (i = 0; i < 12; i++) {
				temp = incoming.indexOf(",", temp + 1);
				if (commas[i - 1] == temp || temp == 255) {
					break;
				}
				commas[i] = temp;
			}
		}
		NumVar = i + 1;
		command = incoming;
		//      Serial.println(command);
	}
	//------------------------------------------------------------------------------------------------
	void separate(String Hkey) {
		byte relayno; byte status = 255;
		String temp = command.substring(start + 1, commas[0]);
		//            Serial.println(Hkey);
		//            Serial.println(start);
		//            Serial.println(commas[0]);
		//            Serial.println(temp);
		//            Serial.println(command);
		if (temp.indexOf(Hkey) != -1) {

			//please check recvACK
			switch (NumVar) {
			case 3:
				temp = command.substring(commas[0] + 1, commas[1]);

				if (temp[0] - '0' > 0 && temp[0] - '0' < 9) {
					relayno = temp[0] - '0';
					temp = command.substring(commas[1] + 1, commas[2]);
					if (temp[0] == (int)'R') {
						/*SET to manual mode*/
						setMode(relayno, 'M');
					}
				}
				break; //optional
			case 4:
				temp = command.substring(commas[0] + 1, commas[1]);
				if (temp[0] == (int)'C') {
					ComType = 2;
					temp = command.substring(commas[1] + 1, commas[2]);
					if (temp[0] == (int)'I') {
						temp = command.substring(commas[2] + 1, end);
						ChangeID(temp.c_str());
					}
				}

				else if (temp[0] == (int)'G') {
					ComType = 2;
					//get wtf
				}
				else if (temp[0] - '0' > 0 && temp[0] - '0' < 9) {
					relayno = temp[0] - '0'; ComType = 1;
					temp = command.substring(commas[1] + 1, commas[2]);

					if (temp[0] == (int)'M') {
						if (RELAYmodes[relayno - 1] == 'M') {
							temp = command.substring(commas[2] + 1, end);
							if (temp[0] == (int)'O') {
								setStatus(relayno, 1);
							}
							else if (temp[0] == (int)'F') {
								if (relayno == NUMRELAY) {
									//                      noTone(RELAYstart + NUMRELAY - 1);
									//                      alarm = 0;
								}
								setStatus(relayno, 0);
							}

						}
						else if (RELAYmodes[relayno - 1] == 'S') {
							//sendCommand(Hkey, "E2");
							//                String ack = "<";
							//                  ack = ack + ID + ",E2>";
							//                  wifi.send(ack.c_str(), ack.length());
							//                  could use this code for the class#consider making a function
							//send relay scheduled plese remove schedule E2=error code
						}

					}
					else if (temp[0] == (int)'G') {
						//get schedule
						char times[4][4];
						//                if (EEPROM.read(OFFSET + NUMRELAY + relayno - 1 ) == (int)'S')
						temp = command.substring(commas[2] + 1, end);
						if (temp[0] == (int)'O') {
							status = 1;
						}
						else if (temp[0] == (int)'F') {
							status = 0;
						}
						int k = OFFSET + (NUMRELAY * 2) + ((relayno - 1) * 32) + (status * 16);
						for (byte i = 0; i < 4; i++) {
							for (byte j = 0; j < 4; j++) {
								times[i][j] = EEPROM.read(k + j + 4 * i);
							}
						}
						String resp = "<";
						resp = resp + Hkey + ',' + (char)(relayno + '0') + ",S," + temp[0]
							+ ',' + times[0][0] + times[0][1] + times[0][2] + times[0][3]
							+ ',' + times[1][0] + times[1][1] + times[1][2] + times[1][3]
							+ ',' + times[2][0] + times[2][1] + times[2][2] + times[2][3]
							+ ',' + times[3][0] + times[3][1] + times[3][2] + times[3][3] + '>';
						//	sendGET(resp.c_str(), resp.length());//customm send

						if (EEPROM.read(OFFSET + NUMRELAY + relayno - 1) == (int)'M')  {
							/*not on schedule response
							String ack = "<";
							ack = ack + Hkey + ",E1>";
							sendGET(ack.c_str(), ack.length());
							*/
						}
					}
				}
				break; //optional

			case 8:
				temp = command.substring(commas[0] + 1, commas[1]);
				if (temp[0] - '0' > 0 && temp[0] - '0' < 9) {
					relayno = temp[0] - '0'; ComType = 1;
					temp = command.substring(commas[1] + 1, commas[2]);
					if (temp[0] == (int)'S') {
						//set schedule
						byte state = 0;
						temp = command.substring(commas[2] + 1, commas[3]);
						if (temp[0] == (int)'O') {
							state = 1;
						}
						else if (temp[0] == (int)'F') {
							state = 0;
						}
						const char times[][4] = {
							{ command.substring(commas[3] + 1, commas[4] + 1)[0], command.substring(commas[3] + 1, commas[4] + 1)[1], command.substring(commas[3] + 1, commas[4] + 1)[2], command.substring(commas[3] + 1, commas[4] + 1)[3] },
							{ command.substring(commas[4] + 1, commas[5] + 1)[0], command.substring(commas[4] + 1, commas[5] + 1)[1], command.substring(commas[4] + 1, commas[5] + 1)[2], command.substring(commas[4] + 1, commas[5] + 1)[3] },
							{ command.substring(commas[5] + 1, commas[6] + 1)[0], command.substring(commas[5] + 1, commas[6] + 1)[1], command.substring(commas[5] + 1, commas[6] + 1)[2], command.substring(commas[5] + 1, commas[6] + 1)[3] },
							{ command.substring(commas[6] + 1, end)[0], command.substring(commas[6] + 1, end)[1], command.substring(commas[6] + 1, end)[2], command.substring(commas[6] + 1, end)[3] }
						};
						setSchedule(relayno, state, times);
					}
				}

				else if (temp[0] == (int)'C') {
					/*change something*/
					temp = command.substring(commas[1] + 1, commas[2]);
					if (temp[0] == (int)'T') {
						/*change time*/
						const byte RTCtime[5] = {
							command.substring(commas[2] + 1, commas[3]).toInt(),//dd
							command.substring(commas[3] + 1, commas[4]).toInt(),//mm
							command.substring(commas[4] + 1, commas[5]).toInt(),//yy
							command.substring(commas[5] + 1, commas[6]).toInt(),//hh
							command.substring(commas[6] + 1, end).toInt()//mm
						};
						//    setDS3231time(00, RTCtime[4], RTCtime[3], 0, RTCtime[0], RTCtime[1], RTCtime[2]);
						//^^removed until rtc
					}
				}
				break;

			}// switch scope ends here
		}//ID check ends here
	}
private:
	void setStatus(byte relayno, byte status) {

		if (relayno <= NUMRELAY) {
			EEPROM.write(OFFSET - 1 + relayno, status);
			RELAYstatus[relayno - 1] = status;
			digitalWrite(RELAYstart + relayno - 1, status);
			USE_SERIAL.printf("SetStatus = %d , %d \n", relayno, status);
		}
		EEPROM.commit();

	}
	void setMode(byte relayno, char mode) {
		if (relayno <= NUMRELAY) {
			EEPROM.write(OFFSET + NUMRELAY + relayno - 1, mode);
			RELAYmodes[relayno - 1] = mode;
			USE_SERIAL.printf("setMode= %d , %d\n", relayno, mode);
		}
		EEPROM.commit();
	}


	void setSchedule(byte relayno, char status, const char times[4][4]) {
		EEPROM.write(OFFSET + NUMRELAY + relayno - 1, 'S');
		RELAYmodes[relayno - 1] = 'S';
		int k = OFFSET + (NUMRELAY * 2) + ((relayno - 1) * 32) + (status * 16);
		for (byte i = 0; i < 4; i++) {
			for (byte j = 0; j<4; j++){
				EEPROM.write(k + 4 * i + j, times[i][j]);
			}
		}
		USE_SERIAL.printf("setschedule= %d , %d \n", relayno, status);
		EEPROM.commit();
	}
	void ChangeID(const char* id) {
		for (byte i = 0; i < IDend; i++) {
			EEPROM.write(i, (char)id[i]);
			Hkey[i] = id[i];
		}
		USE_SERIAL.print("changeID=");
		USE_SERIAL.println(id);
		EEPROM.commit();
	}
	byte NumVar;
	String command;
	byte ComType = 0;
	byte start, end;
	byte commas[12];
};


void configuration()
{
	ssid = server.arg("ssid");
	password = server.arg("pwd");
	String key = server.arg("Hkey");
	if (key == Hkey){
		Ukey = server.arg("Ukey");
		for (byte i = 0; i < Ukeylen; i++){ EEPROM.write(IDend + i, Ukey[i]); }
		WiFi.disconnect();
		WiFi.begin(ssid.c_str(), password.c_str());
		if (WiFi.waitForConnectResult() == WL_CONNECTED){

			if (ssid.indexOf(MY_PREFIX) != -1 && password == MY_PWD){
				connection = TO_MESH;
				server.send(200, "text/html", "connect to mesh node success");
			}
			else{ server.send(200, "text/html", "connect to wifi success"); }
			byte i = 0;
			for (i = 0; i < SSIDlen; i++){ EEPROM.write(IDend + Ukeylen + i, ssid[i]); }
			EEPROM.write(IDend + Ukeylen + i, '\0');
			for (i = 0; i < PASSWORDlen; i++){ EEPROM.write(IDend + Ukeylen + SSIDlen + i, password[i]); }
			EEPROM.write(IDend + Ukeylen + SSIDlen + i, '\0');
		}
		else{
			//couldnt connect to given ssid please change or get in range
			server.send(200, "text/html", "ACK config.not connect");
		}
	}
	USE_SERIAL.print(ssid);
	USE_SERIAL.print("<-ssid  pwd->");
	USE_SERIAL.print(password);
	USE_SERIAL.print("   Ukey->");
	USE_SERIAL.println(Ukey);
	//ukey can be made secure by checking it against the database
	//and the ssid and ip can be stored in the database which receives requests
	//from module.
	//the ukey can be checked against whether an original purchase has been made r not
	EEPROM.commit();
}

void command() {
	String recvCommand = server.arg("command");
	Command cmd(recvCommand);//cost to memory is 112 bytes of ram
	cmd.separate(Hkey);
	server.send(200, "text/html", "ACK babe");
}

void manageMesh(){
	String Hof = server.arg("Hof");
	String question = server.arg("state");
	Serial.printf("hof=%s state=%s\n", Hof.c_str(), question.c_str());

	String state = isQuery(Hof.c_str(), question.c_str());
	if (state == "2"){
		//query already there but no response registered
		//send to wait currentQuery point to the end of queue
		server.send(200, "text/plain", Server_WaitResponse + Hof + "@");
	}
	else if (state == "0"){
		//query not found must be added
		//currentQuery points to last query
		String temp = (String)(Server_WaitResponse + Hof + "@");
		addQuery(Hof.c_str(), temp.c_str(), question.c_str());
		server.send(200, "text/plain", Server_WaitResponse + Hof + "@");
	}
	else {
		//query there and response available send back response
		//currentQuery points correctly
		server.send(200, "text/plain", state);
		removeQuery();
	}

}

void connectNode(){
	String key = server.arg("Ukey");
	if (key == Ukey){
		server.send(200, "text/html", Ukey + "@" + exceptions);
		exceptions += server.arg("exceptions");
	}
}
void InitialSetup(){
	//come cback in the end to check for corrections
	//setDS3231time(30, 21, 16, 4, 1, 7, 15);
	String Hkey = "0234567890123456";
	String Ukey = "default";
	byte i = 0;
	for (i = 0; i < IDend; i++) {
		EEPROM.write(i, Hkey[i]);
	}
	for (i = 0; i < Ukeylen - 1; i++) {
		if (Ukey[i] != '\0'){
			EEPROM.write(IDend + i, Ukey[i]);
		}
		else{ EEPROM.write(IDend + i, Ukey[i]); break; }
	}
	for (i = 0; i < NUMRELAY; i++) {
		EEPROM.write(OFFSET + NUMRELAY + i, RELAYmodes[i]);
	}

	EEPROM.commit();

}
bool connectTomeshAP(byte minRSSI){
	int n = WiFi.scanNetworks();

	for (int i = 0; i < n; ++i) {
		String current_ssid = WiFi.SSID(i);
		int index = current_ssid.indexOf(MY_PREFIX);
		String target_chip_id = current_ssid.substring(index + sizeof(MY_PREFIX));

		/* Connect to any _suitable_ APs which contain _ssid_prefix */
		if (index != -1 && (target_chip_id != Hkey.substring(0, Hkeypart)) && abs(WiFi.RSSI(i)) < minRSSI && checkExceptions(target_chip_id)) {

			HTTPClient curr_client;
			if (WiFi.status() != WL_CONNECTED){
				WiFi.disconnect(1);
				WiFi.begin(current_ssid.c_str(), MY_PWD);
			}
			for (uint8_t t = 5; t > 0; t--) {
				USE_SERIAL.printf("connecting to mesh %d...\n", t);
				USE_SERIAL.flush();
				delay(1000);
			}
			if (WiFi.waitForConnectResult() != WL_CONNECTED){ continue; }

			/* Connect to the node's server */
			curr_client.begin(SERVER_IP_ADDR, SERVER_PORT, ("/connect?Ukey=" + Ukey + "&exceptions=" + exceptions));
			delay(100);
			String response = curr_client.getString();
			if (response.length() <= 2){
				//wrong server connections weird things can happen
				//more like a timeout

				continue;
			}
			else{
				byte indUkey = response.indexOf(Ukey);
				if (indUkey != -1){
					exceptions += response.substring(indUkey, response.lastIndexOf("@") + Hkeypart);
				}
				else{
					//disconnect wifi since incorrect mesh part
					curr_client.end();
					WiFi.disconnect();
					delay(100);
					continue;
				}

			}
			delay(100);
		}
	}
	if (WiFi.status() != WL_CONNECTED){ return false; }
	else{ return true; }
}
void setup() {
	
	Hkey.reserve(IDend);
	Ukey.reserve(Ukeylen);
	ssid.reserve(SSIDlen);
	password.reserve(PASSWORDlen);
	exceptions.reserve(5 * NUMHOPS - 1);
	USE_SERIAL.begin(115200);
	EEPROM.begin(512);
	byte i = 0;
	//for (i = 0; i < 100; i++) {
	//	EEPROM.write(i, '\0');
	//}
	//EEPROM.commit();
	//put a button type resetting mechanism
	//InitialSetup();//for new module
	//Read Hkey from EEPROM

	for (i = 0; i < IDend; i++) {
		char a = EEPROM.read(i);
		if (a == '\0'){ break; }
		else { Hkey += a; }
	}
	exceptions = Hkey.substring(0, Hkeypart);
	//UKEY
	for (i = 0; i < Ukeylen - 1; i++) {
		char a = EEPROM.read(IDend + i);
		if (a == '\0'){ break; }
		else{ Ukey += a; }
	}

	//SSID
	for (i = 0; i < SSIDlen; i++) {
		char a = EEPROM.read(Ukeylen + IDend + i);
		if (a == '\0'){ break; }
		else{ ssid += a; }
	}
	//PWD
	for (i = 0; i < PASSWORDlen; i++) {
		char a = EEPROM.read(Ukeylen + IDend + SSIDlen + i);
		if (a == '\0'){ break; }
		else{ password += a; }
	}
	//STATUS
	for (i = 0; i < NUMRELAY; i++) {
		RELAYstatus[i] = EEPROM.read(OFFSET + i);
		digitalWrite(RELAYstart + i, RELAYstatus[i]);
	}
	//MODES
	for (i = 0; i < NUMRELAY; i++) {
		RELAYmodes[i] = EEPROM.read(OFFSET + NUMRELAY + i);
	}
	for (i = 0; i < 100; i++) {
		Serial.printf("%d -> %c \n", i, (char)EEPROM.read(i));
	}

	//USE_SERIAL.setDebugOutput(true);
	WiFi.softAP(((String)MY_PREFIX + Hkey.substring(0, Hkeypart)).c_str(), ((String)MY_PWD).c_str());
	WiFi.mode(WIFI_AP_STA);
	Serial.println(Hkey.c_str());
	Serial.println(Ukey.c_str());
	Serial.println(ssid.c_str());
	Serial.println(password.c_str());
	WiFi.begin(ssid.c_str(), password.c_str());
	//server.on("/command", HTTP_GET, command);
	server.on("/config", HTTP_GET, configuration);
	server.on("/mesh", HTTP_GET, manageMesh);
	//server.on("/connect", HTTP_GET, connectNode);
	server.begin();
	for (uint8_t t = 5; t > 0; t--) {
		USE_SERIAL.printf("SETUP connecting to router %d...\n", t);
		USE_SERIAL.flush();
	
		delay(1000);
	}
	if (WiFi.waitForConnectResult() == WL_CONNECTED){
		if (WiFi.SSID().indexOf(MY_PREFIX) == 0 && password.compareTo(MY_PWD) == 0){ connection = TO_MESH; }
		else{ connection = TO_ROUTER; }
		Serial.println(WiFi.localIP());
	}
}

/*String getData() {
//here you can send relay statuses to server and any or all sensor data
return (String)counter++;
}
*/
long double timers = 0;
void loop() {
	server.handleClient();
	Serial.printf("%d <- reqCnt TotQ-> %d \n", requestCounter, totalQuery);
	Serial.printf("%d <- conn wifi->%d", connection, WiFi.status());
	//Serial.printf("%d <- wifi status \n", WiFi.status());
	//please call checkschedule every minute or so

	if (WiFi.status() != WL_CONNECTED){
		//WiFi.begin(ssid.c_str(), password.c_str()); //loaded from the eeprom 
		for (uint8_t t = 5; t > 0; t--) {
			USE_SERIAL.printf("SETUP connecting to router %d...\n", t);
			USE_SERIAL.flush();
			server.handleClient();
			delay(1000);
		}
	
		//if (WiFi.status() == WL_DISCONNECTED){ if (connectTomeshAP(60)){ connection = TO_MESH; } }

	}
	else{
		if (WiFi.SSID().indexOf(MY_PREFIX) == 0 && password.compareTo(MY_PWD) == 0){ connection = TO_MESH; }
		else{ connection = TO_ROUTER; }
	}

	if ((WiFi.status() == WL_CONNECTED) && (connection == TO_ROUTER || connection == INTERNET)) {
		//for now it is assumed the data is only relay status
		delay(500);
		
		//PROCESS TABLE REQUESTS HERE from internet
		if (requestCounter == totalQuery){
			//run my request
			String path = "/index.php?Hkey=";
			path += Hkey;
			path += "&Ukey=";
			path += Ukey;
			path += "&data="; //this contains the relay status
			path += "command";//this could be sensor values and other stuff
			String recv = sendGET(URL, PORT, path.c_str(), path.length());
			if (recv != ERROR1 && recv.indexOf("@") != -1){
				//Serial.println(recv);
				//please check for relevant error codes
				}
			requestCounter = 0;
		}
		else if (requestCounter < totalQuery){
			move2query(++requestCounter);
			//run the current query if it doesnt have a valid response 
			if ((String)currentQuery->response == (String)(Server_WaitResponse + (String)currentQuery->HID + "@")){
				String path = "/index.php?Hkey=";
				path += currentQuery->HID;
				path += "&Ukey=";
				path += Ukey;
				path += "&data="; //this contains the relay status
				path += currentQuery->ques;//this could be sensor values and other stuff
				Serial.println(path);
				String recv = sendGET(URL, PORT, path.c_str(), path.length());
				if (recv != (String)(Server_WaitResponse + (String)currentQuery->HID + "@") && recv != ERROR1 && recv.indexOf("@") != -1){
					//please check for relevant error codes
					//valid response 
					connection = INTERNET;
					removeQuery();
				}
			}
		}
		else { requestCounter = 0; }
	}
	delay(500);


}




