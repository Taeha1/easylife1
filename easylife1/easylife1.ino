#include <SoftwareSerial.h>
#include "HX711.h"


// 핀 설정: 블루투스 모듈의 RX와 TX
SoftwareSerial btSerial(3, 2); // RX = 3, TX = 2
const int LOADCELL_DOUT_PIN = 5;
const int LOADCELL_SCK_PIN = 4;
HX711 scale;

// Wi-Fi 정보
String ssid = "";
String password = "";
bool isConnected = false; // Wi-Fi 연결 상태를 추적하기 위한 변수

// 앱인벤터에서 받은 텍스트를 저장할 변수
String receivedItem = "Item 1"; // 기본값 설정

void setup() {
    Serial.begin(9600);    // USB 시리얼 통신을 위한 설정
    btSerial.begin(9600);  // 블루투스 모듈과의 통신 설정

    Serial.println("Setup started");

    scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
    scale.set_scale(-10500.00);   
    scale.tare();  


    // 블루투스 모듈과 연결 상태를 확인
    checkBluetoothConnection();

    // 블루투스 모듈로부터 Wi-Fi 정보 수신
    while(!isConnected){
        receiveCredentials();
        // ESP8266 모듈 설정
        Serial.println("Configuring ESP8266...");
        sendATCommand("AT");
        sendATCommand("AT+CWMODE=1");

        // Wi-Fi 연결 시도
        String connectCommand = "AT+CWJAP=\"" + ssid + "\",\"" + password + "\"";
        Serial.println("Checking Wi-Fi status...");
        sendATCommand(connectCommand);
        unsigned long startTime = millis();
        while (millis() - startTime < 10000) { // 최대 10초 대기
            if (btSerial.available()) {
                String response = readResponse();
                if (response.indexOf("WIFI CONNECTED") != -1 || response.indexOf("WIFI GOT IP") != -1) {
                    isConnected = true;
                    break;
                }
            }
        }
    }
    Serial.println("Wi-Fi 연결 성공");
    // 서버 설정
    sendATCommand("AT+CIPMUX=1");
    sendATCommand("AT+CIPSERVER=1,80");
    Serial.println("Web server started...");
    
    // IP 주소를 얻기 위한 명령어
    sendATCommand("AT+CIFSR");
    
    // IP 주소를 블루투스로 송신
    sendIPAddressToBluetooth();
}

void loop() {

  Serial.println(scale.get_units()*-0.453592, 1);
  
    // ESP8266 모듈로부터 데이터를 수신하여 처리
    if (btSerial.available()) {
        String request = "";
        while (btSerial.available()) {
            char c = btSerial.read();
            request += c;
        }
        Serial.println("Received request:");
        Serial.println(request);

        // 클라이언트로부터 요청이 들어온 경우 HTML 페이지 전송
        if (request.indexOf("+IPD,") != -1) {
            sendHTMLPage(); 
        }

        // 앱인벤터로부터 데이터 수신하여 변수에 저장
        if (request.startsWith("#ITEM:")) { // "#ITEM:"으로 시작하는 경우
            receivedItem = request.substring(6); // "#ITEM:" 이후의 텍스트를 저장
            receivedItem.trim(); // 공백 제거
            Serial.println("Received Item from App: " + receivedItem);
            sendHTMLPage();
        }
    }
}

void checkBluetoothConnection() {
    // 블루투스 연결 상태를 체크하고, 연결된 경우 데이터 수신 준비
    if (btSerial.available()) {
        Serial.println("Bluetooth connection detected, ready to receive commands...");
    } else {
        Serial.println("No Bluetooth connection detected, waiting...");
    }
}

void receiveCredentials() {
    Serial.println("Enter SSID:");
    while (btSerial.available() == 0) {} // 입력이 있을 때까지 대기
    ssid = btSerial.readStringUntil('\n');
    ssid.trim(); // 공백 제거
    Serial.println("SSID received: " + ssid);

    Serial.println("Enter Password:");
    while (btSerial.available() == 0) {} // 입력이 있을 때까지 대기
    password = btSerial.readStringUntil('\n');
    password.trim(); // 공백 제거
    Serial.println("Password received.");
}

void sendATCommand(String command) {
    Serial.print("Sending command: ");
    Serial.println(command);
    btSerial.println(command);

    // 응답 대기
    waitForResponse();
}

void waitForResponse() {
    unsigned long startTime = millis();
    while (millis() - startTime < 5000) { // 최대 5초 대기
        if (btSerial.available()) {
            String response = "";
            while (btSerial.available()) {
                char c = btSerial.read();
                response += c;
            }
            Serial.print(response); // 시리얼 모니터에 응답 출력
            if (response.indexOf("OK") != -1 || response.indexOf(">") != -1) {
                Serial.println("Received OK or >");
                break; // "OK" 또는 ">" 응답을 받으면 루프 종료
            }
        }
    }
}

String readResponse() {
    String response = "";
    unsigned long startTime = millis();
    while (millis() - startTime < 5000) { // 최대 5초 대기
        if (btSerial.available()) {
            char c = btSerial.read();
            response += c;
        }
    }
    Serial.print(response); // 시리얼 모니터에 응답 출력
    return response;
}

void sendHTMLPage() {
    String html = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n";
    html += "<!DOCTYPE HTML>";
    html += "<html>";
    html += "<h1>Arduino Web Server</h1>";
    html += "<p>This is a simple web server example.</p>";
    html += "<p>ESP8266 is running...</p>";
    html += "<table border='1' style='width:100%; text-align:center;'>";
    html += "<tr><td>Item</td><td>Quantity</td><td>Link</td></tr>";
    html += "<tr><td>" + receivedItem + "</td><td>10</td><td><a href='#'>Link</a></td></tr>"; // receivedItem 변수 사용
    html += "</table>";
    html += "</html>";

    String sendCommand = "AT+CIPSEND=0," + String(html.length());
    sendATCommand(sendCommand);

    // 데이터 전송을 기다림
    waitForSendReady();
    
    btSerial.print(html); // HTML 페이지 전송
}

void waitForSendReady() {
    unsigned long startTime = millis();
    while (millis() - startTime < 5000) { // 최대 5초 대기
        if (btSerial.available()) {
            String response = "";
            while (btSerial.available()) {
                char c = btSerial.read();
                response += c;
            }
            Serial.print(response); // 시리얼 모니터에 응답 출력
            if (response.indexOf(">") != -1) {
                Serial.println("Ready to send data");
                break; // ">" 응답을 받으면 루프 종료
            }
        }
    }
}

void sendIPAddressToBluetooth() {
    String response = readResponse(); // IP 주소를 포함한 응답을 읽음
    String ipAddress = extractIPAddress(response); // IP 주소 추출
    Serial.println("Sending IP Address via Bluetooth: " + ipAddress);
    btSerial.println(ipAddress); // IP 주소를 블루투스를 통해 송신
}

String extractIPAddress(String response) {
    // IP 주소를 응답에서 추출
    int startIdx = response.indexOf("IP") + 3; // "IP" 뒤의 시작 인덱스
    int endIdx = response.indexOf("\r\n", startIdx); // 줄바꿈 문자로 끝 인덱스 찾기
    if (startIdx == -1 || endIdx == -1) {
        return "IP Not Found";
    }
    return response.substring(startIdx, endIdx); // IP 주소 추출
}
