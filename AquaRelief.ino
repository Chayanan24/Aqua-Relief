#include <WiFiS3.h>
#include <Servo.h>


// 1. WIFI SETTINGS
char ssid[] = "FLOOD_BOAT";
char pass[] = "12345678";

WiFiServer server(80);

// 2. MOTOR PINS (L298N)
const int ENA = 5;
const int IN1 = 7;
const int IN2 = 8;

const int ENB = 6;
const int IN3 = 9;
const int IN4 = 10;

// 3. SERVO AND SWITCH
const int SERVO_PIN = 11;
const int SWITCH_PIN = 2;

Servo foodServo;

const int SERVO_CLOSE = 0;
const int SERVO_OPEN  = 90;

bool boxOpen = false;

// 4. MOTOR SETTINGS
int motorSpeed = 180;

char currentMove = 'S';

unsigned long lastCommandTime = 0;

const unsigned long FAILSAFE_TIME = 500;

// 5. DESTINATION SETTINGS
int selectedDestination = 0;

bool missionRunning = false;

int missionStep = 0;

unsigned long missionTimer = 0;


unsigned long routeA[] = {
  10000, 
  15000,   
  1000  
};

unsigned long routeB[] = {
  10000,  
  15000,   
  1000 
};

unsigned long routeC[] = {
  15000,  
  20000,  
  3000 
};

const int ROUTE_STEPS = 3;


// 6. SYSTEM STATUS
String boatStatus = "STOPPED";

// 7. HTML DASHBOARD
const char dashboard[] PROGMEM = R"rawliteral(

<!DOCTYPE html>
<html>

<head>

<meta name="viewport" content="width=device-width, initial-scale=1">

<title>AquaRelief</title>

<style>

body {
  font-family: Arial;
  text-align: center;
  background: #eaf2f8;
  margin: 0;
  padding: 15px;
}

h1 {
  color: #154360;
  font-size: 24px;
}

.card {
  background: white;
  padding: 18px;
  margin: 15px auto;
  border-radius: 15px;
  max-width: 400px;
  box-shadow: 0 2px 8px #bbb;
}

button {
  padding: 15px;
  margin: 5px;
  min-width: 75px;
  font-size: 16px;
  border: none;
  border-radius: 10px;
  background: #2874a6;
  color: white;
  touch-action: none;
}

.stop {
  background: #e74c3c;
}

.open {
  background: #27ae60;
}

.close {
  background: #7f8c8d;
}

.destination {
  background: #8e44ad;
}

.status {
  font-size: 18px;
  font-weight: bold;
  color: #154360;
}

button:active {
  opacity: 0.7;
}

</style>

</head>


<body>

<h1>AquaRelief</h1>

<p>Boat Control Dashboard</p>


<!-- BOAT STATUS -->

<div class="card">

<h2>Boat Status</h2>

<p>Movement:
<span class="status" id="movement">STOPPED</span>
</p>

<p>Mission:
<span class="status" id="mission">IDLE</span>
</p>

<p>Food Box:
<span class="status" id="foodbox">CLOSED</span>
</p>

</div>


<!-- MANUAL CONTROL -->

<div class="card">

<h2>Manual Control</h2>

<div>
<button id="forward">^</button>
</div>

<div>

<button id="left"><</button>

<button class="stop" id="stop">STOP</button>

<button id="right">></button>

</div>

<div>
<button id="backward">v</button>
</div>

<p>Hold to move , release to stop</p>

</div>


<!-- DESTINATION -->

<div class="card">

<h2>Destination Mode</h2>

<button class="destination"
onclick="selectDestination('A')">
HOME A
</button>

<button class="destination"
onclick="selectDestination('B')">
HOME B
</button>

<button class="destination"
onclick="selectDestination('C')">
EVACUATION CENTER
</button>

<p id="selected">No Destination Selected</p>

<button onclick="startMission()">
START DELIVERY
</button>

<button class="stop" onclick="cancelMission()">
CANCEL MISSION
</button>

</div>


<!-- FOOD BOX -->

<div class="card">

<h2>Food Box Control</h2>

<button class="open"
onclick="foodOpen()">
OPEN BOX
</button>

<button class="close"
onclick="foodClose()">
CLOSE BOX
</button>

<p>press the switch to open the box</p>

</div>


<script>

let destination = "";


function sendCommand(url) {

  fetch(url, {cache: "no-store"})
  .catch(error => console.log(error));

}


// MANUAL CONTROL

function holdButton(id, command) {

  const btn = document.getElementById(id);
  let repeatTimer = null;

  function startMoving(e) {

    e.preventDefault();

    if (repeatTimer !== null) {
      clearInterval(repeatTimer);
    }

    sendCommand("/move?dir=" + command);

    repeatTimer = setInterval(function() {
      sendCommand("/move?dir=" + command);
    }, 200);

  }

  function stopMoving() {

    if (repeatTimer !== null) {
      clearInterval(repeatTimer);
      repeatTimer = null;
    }

    sendCommand("/move?dir=S");
  }

  btn.addEventListener("pointerdown", startMoving);
  btn.addEventListener("pointerup", stopMoving);
  btn.addEventListener("pointercancel", stopMoving);
  btn.addEventListener("lostpointercapture", stopMoving);

}


holdButton("forward", "F");
holdButton("backward", "B");
holdButton("left", "L");
holdButton("right", "R");


document.getElementById("stop").onclick = function() {

  sendCommand("/move?dir=S");

};


// DESTINATION

function selectDestination(value) {

  destination = value;

  document.getElementById("selected").innerText =
    "Selected: " + value;

  sendCommand("/destination?to=" + value);

}


function startMission() {

  if(destination == "") {

    alert("Please select destination");

    return;

  }

  sendCommand("/start");

}


function cancelMission() {

  sendCommand("/cancel");

}


// FOOD BOX

function foodOpen() {

  sendCommand("/box?state=open");

}

function foodClose() {

  sendCommand("/box?state=close");

}


// STATUS UPDATE

function updateStatus() {

  fetch("/status", {cache: "no-store"})

  .then(response => response.json())

  .then(data => {

    document.getElementById("movement").innerText =
      data.movement;

    document.getElementById("mission").innerText =
      data.mission;

    document.getElementById("foodbox").innerText =
      data.box;

  })

  .catch(error => console.log(error));

}


setInterval(updateStatus, 500);

</script>

</body>
</html>

)rawliteral";


// 8. MOTOR FUNCTIONS
void stopMotor() {

  analogWrite(ENA, 0);
  analogWrite(ENB, 0);

  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);

  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);

  currentMove = 'S';

  boatStatus = "STOPPED";

}


void forward() {

  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);

  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);

  analogWrite(ENA, motorSpeed);
  analogWrite(ENB, motorSpeed);

  currentMove = 'F';

  boatStatus = "FORWARD";

}


void backward() {

  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);

  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);

  analogWrite(ENA, motorSpeed);
  analogWrite(ENB, motorSpeed);

  currentMove = 'B';

  boatStatus = "BACKWARD";

}


void turnLeft() {

  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);

  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);

  analogWrite(ENA, 0);
  analogWrite(ENB, motorSpeed);

  currentMove = 'L';

  boatStatus = "TURN LEFT";

}


void turnRight() {

  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);

  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);

  analogWrite(ENA, motorSpeed);
  analogWrite(ENB, 0);

  currentMove = 'R';

  boatStatus = "TURN RIGHT";

}

// 9. FOOD BOX FUNCTIONS
void openFoodBox() {

  stopMotor();

  foodServo.write(SERVO_OPEN);

  boxOpen = true;

  Serial.println("FOOD BOX OPENED");

}


void closeFoodBox() {

  foodServo.write(SERVO_CLOSE);

  boxOpen = false;

  Serial.println("FOOD BOX CLOSED");

}

// 10. DESTINATION FUNCTIONS

void selectRoute(int destination) {

  selectedDestination = destination;

  Serial.print("Destination: ");
  Serial.println(destination);

}


void startMission() {

  if(selectedDestination == 0) {
    return;
  }

  stopMotor();

  closeFoodBox();

  missionRunning = true;

  missionStep = 0;

  missionTimer = millis();

  boatStatus = "MISSION STARTED";

  Serial.println("MISSION STARTED");

}


void cancelMission() {

  missionRunning = false;

  stopMotor();

  boatStatus = "MISSION CANCELLED";

  Serial.println("MISSION CANCELLED");

}


void runMission() {

  if(!missionRunning) {
    return;
  }


  unsigned long *route;

  if(selectedDestination == 1) {

    route = routeA;

  }

  else if(selectedDestination == 2) {

    route = routeB;

  }

  else {

    route = routeC;

  }


  // เมื่อครบทุกขั้นตอน
  if(missionStep >= ROUTE_STEPS) {

    missionRunning = false;

    stopMotor();

    boatStatus = "ARRIVED";

    Serial.println("DESTINATION REACHED");

    // เปิดฝากล่องอัตโนมัติ
    openFoodBox();

    return;

  }


  // ควบคุมการเคลื่อนที่ตามลำดับ
  if(missionStep == 0) {

    forward();

  }

  else if(missionStep == 1) {

    if(selectedDestination == 1 ||
       selectedDestination == 3) {

      turnRight();

    }

    else {

      turnLeft();

    }

  }

  else if(missionStep == 2) {

    forward();

  }


  // รอให้ครบเวลาของแต่ละขั้น
  if(millis() - missionTimer >= route[missionStep]) {

    stopMotor();

    missionStep++;

    missionTimer = millis();

  }

}


// 11. PHYSICAL SWITCH

bool lastSwitchState = HIGH;
unsigned long lastDebounceTime = 0;

void checkFoodSwitch() {

  bool reading = digitalRead(SWITCH_PIN);

  if (reading != lastSwitchState) {
    lastDebounceTime = millis();
  }

  if (millis() - lastDebounceTime > 40) {

    static bool stableState = HIGH;

    if (reading != stableState) {

      stableState = reading;

      // ตรวจจับตอนกดปุ่ม
      if (stableState == LOW) {

        if (boxOpen == false) {
          openFoodBox();
        }
        else {
          closeFoodBox();
        }

      }
    }
  }

  lastSwitchState = reading;
}

// 12. WEB SERVER
void sendResponse(WiFiClient &client,
                  const String &type,
                  const String &body) {

  client.println("HTTP/1.1 200 OK");

  client.println("Content-Type: " + type);

  client.println("Connection: close");

  client.println("Cache-Control: no-store");

  client.println();

  client.print(body);

}


void handleWebClient() {

  WiFiClient client = server.available();

  if(!client) {
    return;
  }


  client.setTimeout(100);

  String request = client.readStringUntil('\n');

  request.trim();

  Serial.println(request);


  // อ่าน HTTP headers
  while(client.connected()) {

    String line = client.readStringUntil('\n');

    if(line == "\r" || line.length() == 0) {
      break;
    }

  }


  // DASHBOARD
  if(request.indexOf("GET / ") >= 0) {

    sendResponse(
      client,
      "text/html",
      dashboard
    );

  }


  // MANUAL MOVEMENT
  else if(request.indexOf("GET /move?dir=") >= 0) {

    missionRunning = false;

    int index = request.indexOf("dir=");

    char direction = request.charAt(index + 4);


    if(direction == 'F') {

      forward();

    }

    else if(direction == 'B') {

      backward();

    }

    else if(direction == 'L') {

      turnLeft();

    }

    else if(direction == 'R') {
    turnRight();
}

    else {

      stopMotor();

    }


    lastCommandTime = millis();

    sendResponse(client, "text/plain", "OK");

  }


  // DESTINATION SELECTION
  else if(request.indexOf("GET /destination?to=") >= 0) {

    char destination =
      request.charAt(request.indexOf("to=") + 3);


    if(destination == 'A') {
      selectRoute(1);
    }

    else if(destination == 'B') {
      selectRoute(2);
    }

    else if(destination == 'C') {
      selectRoute(3);
    }


    sendResponse(client, "text/plain", "OK");

  }


  // START DELIVERY
  else if(request.indexOf("GET /start") >= 0) {

    startMission();

    sendResponse(client, "text/plain", "OK");

  }


  // CANCEL DELIVERY
  else if(request.indexOf("GET /cancel") >= 0) {

    cancelMission();

    sendResponse(client, "text/plain", "OK");

  }


  // FOOD BOX CONTROL
  else if(request.indexOf("GET /box?state=open") >= 0) {

    openFoodBox();

    sendResponse(client, "text/plain", "OPEN");

  }


  else if(request.indexOf("GET /box?state=close") >= 0) {

    closeFoodBox();

    sendResponse(client, "text/plain", "CLOSED");

  }


  // STATUS
  else if(request.indexOf("GET /status") >= 0) {

    String missionStatus =
      missionRunning ? "RUNNING" : "IDLE";

    String boxStatus =
      boxOpen ? "OPEN" : "CLOSED";


    String json = "{";

    json += "\"movement\":\"" + boatStatus + "\",";

    json += "\"mission\":\"" + missionStatus + "\",";

    json += "\"box\":\"" + boxStatus + "\"";

    json += "}";


    sendResponse(client, "application/json", json);

  }


  else {

    sendResponse(client, "text/plain", "NOT FOUND");

  }


  delay(1);

  client.stop();

}


// 13. SETUP
void setup() {

  Serial.begin(9600);


  pinMode(ENA, OUTPUT);

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);

  pinMode(ENB, OUTPUT);

  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);


  pinMode(SWITCH_PIN, INPUT_PULLUP);

  foodServo.attach(SERVO_PIN);

  closeFoodBox();


  stopMotor();


  // WiFi Access Point
  Serial.println("Starting WiFi...");

  int status = WiFi.beginAP(ssid, pass);


  if(status != WL_AP_LISTENING) {

    Serial.println("WiFi AP FAILED");

    while(true) {
      delay(1000);
    }

  }


  delay(1000);

  server.begin();


  Serial.println("========================");

  Serial.println("FLOOD FOOD DELIVERY BOAT");

  Serial.println("WiFi Started");

  Serial.print("SSID: ");
  Serial.println(ssid);

  Serial.print("IP Address: ");

  Serial.println(WiFi.localIP());

  Serial.println("========================");

}

// 14. MAIN LOOP
void loop() {

  handleWebClient();

  checkFoodSwitch();

  runMission();


  if(!missionRunning &&
     currentMove != 'S' &&
     millis() - lastCommandTime > FAILSAFE_TIME) {

    stopMotor();

    Serial.println("FAILSAFE: MOTOR STOPPED");

  }

}